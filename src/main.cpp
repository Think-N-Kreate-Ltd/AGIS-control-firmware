/*********
  Rui Santos
  Complete project details at
https://RandomNerdTutorials.com/esp32-esp8266-input-data-html-form/

  Permission is hereby granted, free of charge, to any person obtaining a copy
  of this software and associated documentation files.

  The above copyright notice and this permission notice shall be included in all
  copies or substantial portions of the Software.
*********/
/*
  wifi ssid:AutoConnectAP, password:password.
  go to http://<IPAddress>/update for OTA update
  upload firmware.bin for main, spiffs.bin for SPIFFS files
  GPIO36 -> reading sensor data (Change in L34)
  timer0 -> read sensor & time measure
  timer1 -> auto control
  timer2 -> control the motor
*/

#include <Arduino.h>
#include <AsyncTCP.h>
#include <WiFiManager.h> // define before <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <SPIFFS.h>
#include <WiFi.h>
#include <ezButton.h>
#include <limits.h>
#include <ArduinoJson.h>
#include <AsyncElegantOTA.h>  // define after <ESPAsyncWebServer.h>

// TODO: refactor names, follow standard naming conventions

#define DROP_SENSOR_PIN  36 // input pin for geting output from sensor
#define MOTOR_CTRL_PIN_1 15 // Motorl Control Board PWM 1
#define MOTOR_CTRL_PIN_2 16 // Motorl Control Board PWM 2
#define PWM_PIN          4  // input pin for the potentiometer

enum class motorState_t { UP, DOWN, OFF };
motorState_t motorState = motorState_t::OFF;

enum class buttonState_t { UP, DOWN, ENTER, IDLE };
buttonState_t buttonState = buttonState_t::IDLE;

// NOTE: when droppingState_t type is modified, update the same type in script.js 
enum droppingState_t {NOT_STARTED, STARTED, STOPPED};
// Initially, dropping is not started
droppingState_t droppingState = droppingState_t::NOT_STARTED;
// volatile bool dropStarted = false; 

// var for checking the time
volatile bool no_drop_with_20s = false; // true if no drop appears in next 20s
volatile bool volume_exceed = false;    // true if numDrops exceed amount

// var for timer0 interrupt
// for reading the sensor value
volatile int occur;
// for measuring the time that sensor detect a drop
unsigned long start_time = 0;
// for measuring the time that sensor detect a drop is disappear
unsigned long leave_time = 0;
// for measuring the time that sensor detect the next drop
unsigned long next_time = 0;
unsigned long totalTime = 0; // for calculating the time used within 15s
unsigned int numDrops = 0;   // for counting the number of drops within 15s
volatile unsigned int dripRate = 0;       // for calculating the drip rate
volatile unsigned int prevDripRate = 0;   // use this to check if drip rate change drastically
volatile unsigned int time1Drop = 0;      // for storing the time of 1 drop
volatile unsigned int timeBtw2Drops = UINT_MAX; // i.e. no more drop recently
volatile float infusedVolume = 0;  // unit: mL
volatile unsigned long infusedTime = 0;     // unit: seconds
unsigned long infusionStartTime = 0;

volatile unsigned int dripRateSamplingCount = 0;  // use for drip rate sampling
volatile unsigned int numDropsInterval = 0;  // number of drops in 15 seconds
volatile unsigned int autoControlCount = 0;  // use for regulating frequency of motor is on

// var for timer2 interrupt
int PWMValue = 0; // PWM value to control the speed of motor

ezButton button_UP(3);         // create ezButton object that attach to pin 6;
ezButton button_ENTER(8);      // create ezButton object that attach to pin 7;
ezButton button_DOWN(46);      // create ezButton object that attach to pin 8;
ezButton limitSwitch_Up(37);   // create ezButton object that attach to pin 7;
ezButton limitSwitch_Down(38); // create ezButton object that attach to pin 7;

// var for checking the currently condition
 // true if it is controlled by the web button currently
volatile bool web_state = false;
// state that shows the condition of web button
int web_but_state = 0; 
// state that shows the condition of auto control
unsigned int targetDripRate = 0; 
unsigned int targetVTBI = 0;   // target total volume to be infused
unsigned int targetTotalTime = 0;   // target total time to be infused
unsigned int targetNumDrops = 0;    // used for stopping infusion when complete
unsigned int dropFactor = UINT_MAX;  // to avoid divide by zero, unit: drops/mL

volatile bool enableAutoControl = false; // to enable AutoControl() or not
volatile bool infusionCompleted = false;   // true when infusion is completed
volatile bool infusionStarted = false;     // true when button_ENTER is pressed the 1st time
                                         // to activate autoControl()

volatile bool drippingIsStable = true; // true when receiving the first NUM_DROPS_TILL_STABLE drops
                                       // initially needs to set to true, otherwise autoControl() cannot start

volatile bool firstDropDetected = false; // to check when we receive the 1st drop
volatile bool autoControlOnPeriod = false;

// To reduce the sensitive of autoControl()
// i.e. (targetDripRate +/-5) is good enough
#define AUTO_CONTROL_ALLOW_RANGE 5
// #define DRIP_RATE_SAMPLE_PERIOD  5   // 5 seconds
#define AUTO_CONTROL_ON_TIME     50  // motor will be enabled for this amount of time (unit: ms)
#define AUTO_CONTROL_TOTAL_TIME  1000  // 1000ms
#define DRIP_RATE_MAX_RANGE      1000  // no more than 1000 drops/min (this can be changed)

// WiFiManager, Local intialization. Once its business is done, there is no need
// to keep it around
WiFiManager wm;

// var for web page
String inputMessage; // store the input from web page

// create AsyncWebServer object on port 80
AsyncWebServer server(80);
AsyncWebSocket ws("/ws");
// TODO: use websocket for communication

// REPLACE WITH YOUR NETWORK CREDENTIALS  // for hard-coding the wifi
// const char* ssid = "REPLACE_WITH_YOUR_SSID";
// const char* password = "REPLACE_WITH_YOUR_PASSWORD";

const char *PARAM_INPUT_1 = "input1";
const char *PARAM_INPUT_2 = "input2";
const char *PARAM_INPUT_3 = "input3";
const char *PARAM_AUTO_1 = "auto1";

// Function prototypes
int check_state();
void Motor_On_Up();
void Motor_On_Down();
void Motor_Off();
// void Motor_Mode();
const char *get_motor_state(motorState_t state);
const char *get_button_state(buttonState_t state);
void initWebSocket();
void onEvent(AsyncWebSocket *server, AsyncWebSocketClient *client,
             AwsEventType type, void *arg, uint8_t *data, size_t len);
void handleWebSocketMessage(void *arg, uint8_t *data, size_t len);
void sendDataWs();

// HTML web page to handle 3 input fields (input1, input2, input3)

// goto 404 not found when 404 not found
void notFound(AsyncWebServerRequest *request) {
  request->send(404, "text/plain", "Not found");
}

String readFile(fs::FS &fs, const char *path) {
  Serial.printf("Reading file: %s\r\n", path);
  File file = fs.open(path, "r");
  if (!file || file.isDirectory()) {
    Serial.println("- empty file or failed to open file");
    return String();
  }
  Serial.print("- read from file: ");
  String fileContent;
  while (file.available()) {
    fileContent += String((char)file.read());
  }
  file.close();
  Serial.println(fileContent);
  return fileContent;
}

void writeFile(fs::FS &fs, const char *path, const char *message) {
  Serial.printf("Writing file: %s\r\n", path);
  File file = fs.open(path, "w");
  if (!file) {
    Serial.println("- failed to open file for writing");
    return;
  }
  if (file.print(message)) {
    Serial.println("- file written");
  } else {
    Serial.println("- write failed");
  }
  file.close();
}

// create pointer for timer
hw_timer_t *Timer0_cfg = NULL; // create a pointer for timer0
hw_timer_t *Timer1_cfg = NULL; // create a pointer for timer1
hw_timer_t *Timer2_cfg = NULL; // create a pointer for timer2

// timer0 interrupt, for sensor detected drops and measure the time
void IRAM_ATTR dropSensor() {
  static bool occur_state = false; // true when obstacle detected
  static int time_for_no_drop; // counting when no drop appears, for measuring
                               // the time that have no drop

  occur = digitalRead(DROP_SENSOR_PIN); // read the sensor value

  dripRateSamplingCount++;  // increment 1ms

  if (occur == 1) {
    time_for_no_drop = 0;
    no_drop_with_20s = false;
    droppingState = droppingState_t::STARTED; // droping has started
    if (!occur_state) { // condition that check for the drop is just detected


      // FIRST DROP DETECTION
      // stop the motor and disable autoControl()
      if (!firstDropDetected) {
        firstDropDetected = true;

        // mark this as starting time of infusion
        infusionStartTime = millis();
      }

      numDropsInterval++;

      occur_state = true;
      numDrops++;       // counting the drop
      next_time = millis();
      timeBtw2Drops = next_time - start_time;
      totalTime += timeBtw2Drops;
      start_time = millis();
    }
  }
  else if (occur == 0) {
    time_for_no_drop++;
    if (occur_state) {
      leave_time = millis(); // record the time for measuring
      occur_state = false;
      time1Drop = leave_time - start_time;
    }
  }

  // call when no drop appears within 20s, reset all data
  if ((time_for_no_drop >= 20000) && (droppingState == droppingState_t::STARTED)) {
    time1Drop = 0;
    // numDrops = 0;

    // TODO: how do we define totalTime? Should it be RTC time or only the time
    // when we have drops?
    totalTime = 0;
    no_drop_with_20s = true;

    // set timeBtw2Drops to a very large number
    timeBtw2Drops = UINT_MAX;
    droppingState = droppingState_t::STOPPED;

    // reset this to enable the next first drop detection
    firstDropDetected = false;
  }
  // call when the no of drops exceed target
  // TODO: replace hardcoded maximum number of drops below
  // if (numDrops >= 500) {
  //   volume_exceed = true;
  //   // TODO: alert volume exceed
  //   // alert("VolumeExceed");
  // }

  // Calculate dripRate using number of drops in every DRIP_RATE_SAMPLE_PERIOD seconds
  // if (dripRateSamplingCount == (DRIP_RATE_SAMPLE_PERIOD * 1000)) {
  //   // 15 seconds reached
  //   dripRate = numDropsInterval * (60 / DRIP_RATE_SAMPLE_PERIOD);
  //   // enableAutoControl = true;

  //   // reset for the next sampling
  //   dripRateSamplingCount = 0;
  //   numDropsInterval = 0;
  // }

  // NOTE: use a fixed DRIP_RATE_MAX_RANGE to ignore huge drip rate from unstable measurements
  // get latest value of dripRate
  dripRate = 60000 / timeBtw2Drops; // TODO: explain this formular
  if (dripRate < DRIP_RATE_MAX_RANGE) {
    prevDripRate = dripRate;
  }
  else {
    dripRate = prevDripRate;
  }

  // Get infusion time so far:
  if (!no_drop_with_20s) {
    infusedTime = (millis() - infusionStartTime) / 1000;  // in seconds
  }

  // NOTE: maybe we should average most recent dripRate,
  // s.t. the auto control is not too sensitive and motor runs too frequently
}

void IRAM_ATTR autoControl() { // timer1 interrupt, for auto control motor
  // Only run autoControl() when the following conditions satisfy:
  //   1. button_ENTER is pressed
  //   2. dripping is stable, i.e. drippingIsStable = true
  //   3. targetDripRate is set on the website by user
  //   4. infusion is not completed, i.e. infusionCompleted = false
  // TODO: update value of infusionCompleted in other function

  autoControlCount++;
  if (firstDropDetected) {
    // on for 50 ms, off for 950 ms
    autoControlOnPeriod = (0 <= autoControlCount) && (autoControlCount <= AUTO_CONTROL_ON_TIME);
  }
  else {
    autoControlOnPeriod = true;  // no limitation on motor on period
  }


  // Check if infusion has completed or not
  if (numDrops >= targetNumDrops) {
    // disable autoControl()
    enableAutoControl = false;

    // TODO: sound the alarm

    // TODO: notify website
  }
  

  if (enableAutoControl && autoControlOnPeriod && (targetDripRate != 0) && !infusionCompleted) {

    // TODO: alert when no drop is detected, i.e. could be out of fluid or get
    // stuck

    // if currently SLOWER than set value -> speed up, i.e. move up
    if (dripRate < (targetDripRate - AUTO_CONTROL_ALLOW_RANGE)) {
      Motor_On_Up();
    }

    // if currently FASTER than set value -> slow down, i.e. move down
    else if (dripRate > (targetDripRate + AUTO_CONTROL_ALLOW_RANGE)) {
      Motor_On_Down();
    }

    // otherwise, current drip rate is in allowed range -> stop motor
    else {
      Motor_Off();
    }
  }
  else {
    Motor_Off();
  }

  // reset this for the next autoControl()
  if (autoControlCount == AUTO_CONTROL_TOTAL_TIME) {   // reset count every 1s
    autoControlCount = 0;
  }
}

void IRAM_ATTR motorControl() {
  // Read buttons and switches state
  button_UP.loop();        // MUST call the loop() function first
  button_ENTER.loop();     // MUST call the loop() function first
  button_DOWN.loop();      // MUST call the loop() function first

  // Use button_UP to manually move up
  if (!button_UP.getState()) {  // touched
    buttonState = buttonState_t::UP;
    Motor_On_Up();
  }

  // Use button_UP to manually move down
  if (!button_DOWN.getState()) {  // touched
    buttonState = buttonState_t::DOWN;
    Motor_On_Down();
  }

  // Use button_ENTER to toggle autoControl()
  if (button_ENTER.isPressed()) {  // pressed is different from touched
    buttonState = buttonState_t::ENTER;
    enableAutoControl = !enableAutoControl;

    // Reset infusion parameters the first time button_ENTER is pressed.
    // Parameters need to be reset:
    //    (1) numDrops
    //    (2) infusedVolume
    //    (3) infusedTime
    //    Add more if necessary
    if (!infusionStarted) {
      numDrops = 0;
      infusedVolume = 0.0f;
      infusedTime = 0;

      infusionStarted = true;
    }
  }

  if (button_UP.isReleased() || button_DOWN.isReleased() || button_ENTER.isReleased()) {
    buttonState = buttonState_t::IDLE;
    Motor_Off();
  }
}

void setup() {
  Serial.begin(9600);
  pinMode(DROP_SENSOR_PIN, INPUT);

  // setup for timer0
  Timer0_cfg = timerBegin(0, 80, true); // Prescaler = 80
  timerAttachInterrupt(Timer0_cfg, &dropSensor,
                       true);              // call the function dropSensor()
  timerAlarmWrite(Timer0_cfg, 1000, true); // Time = 1000*80/80,000,000 = 1ms
  timerAlarmEnable(Timer0_cfg);            // start the interrupt

  // setup for timer1
  Timer1_cfg = timerBegin(1, 80, true); // Prescaler = 80
  timerAttachInterrupt(Timer1_cfg, &autoControl,
                       true);              // call the function autoControl()
  timerAlarmWrite(Timer1_cfg, 1000, true); // Time = 80*1000/80,000,000 = 1ms
  timerAlarmEnable(Timer1_cfg);            // start the interrupt

  // setup for timer2
  Timer2_cfg = timerBegin(2, 80, true); // Prescaler = 80
  timerAttachInterrupt(Timer2_cfg, &motorControl,
                       true);              // call the function motorControl()
  timerAlarmWrite(Timer2_cfg, 1000, true); // Time = 1000*80/80,000,000 = 1ms
  timerAlarmEnable(Timer2_cfg);            // start the interrupt

  // Initialize SPIFFS
  if (!SPIFFS.begin(true)) {
    Serial.println("An Error has occurred while mounting SPIFFS");
    return;
  }

  WiFi.mode(WIFI_STA); // wifi station mode

  // reset settings - wipe stored credentials for testing
  // these are stored by the esp library
  // wm.resetSettings();

  if (!wm.autoConnect("AutoConnectAP",
                      "password")) { // set esp32-s3 wifi ssid and pw to
    // AutoConnectAP & password
    Serial.println("Failed to connect");
    ESP.restart();
  } else {
    // if you get here you have connected to the WiFi
    Serial.println("connected...yeah :)");
  }

  if (WiFi.waitForConnectResult() != WL_CONNECTED) {
    Serial.println("WiFi Failed!");
    return;
  }

  // print the IP address of the web page
  Serial.println();
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());

  // Init Websocket
  initWebSocket();

  // Send web page to client
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(SPIFFS, "/index.html", String(), false);
  });

  server.on("/style.css", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(SPIFFS, "/style.css", "text/css");
  });

  server.on("/script.js", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(SPIFFS, "/script.js", "text/javascript");
  });

  // TODO: should we use websocket for below requests?
  // probably depending on file size
  // Send a GET request to <ESP_IP>/get?input1=<inputMessage>
  server.on("/get", HTTP_GET, [](AsyncWebServerRequest *request) {
    // only 1 motor is controlled now, all inputs will change the same motor
    // GET input1 value on <ESP_IP>/get?input1=<inputMessage>
    if (request->hasParam(PARAM_INPUT_1)) {
      inputMessage = request->getParam(PARAM_INPUT_1)->value();
      // inputParam = PARAM_INPUT_1;
      writeFile(SPIFFS, "/input1.txt", inputMessage.c_str());
      web_but_state = check_state(); // convert the input from AGIS1 to integer,
                                     // and store in web_but_state
    }
    // GET input2 value on <ESP_IP>/get?input2=<inputMessage>
    else if (request->hasParam(PARAM_INPUT_2)) {
      inputMessage = request->getParam(PARAM_INPUT_2)->value();
      // inputParam = PARAM_INPUT_2;
      writeFile(SPIFFS, "/input2.txt", inputMessage.c_str());
      web_but_state = check_state(); // convert the input from AGIS2 to integer,
                                     // and store in web_but_state
    }
    // GET input3 value on <ESP_IP>/get?input3=<inputMessage>
    else if (request->hasParam(PARAM_INPUT_3)) {
      inputMessage = request->getParam(PARAM_INPUT_3)->value();
      // inputParam = PARAM_INPUT_3;
      writeFile(SPIFFS, "/input3.txt", inputMessage.c_str());
      web_but_state = check_state(); // convert the input from AGIS3 to integer,
                                     // and store in web_but_state
    }
    // GET auto1 value on <ESP_IP>/get?auto1=t
    // else if (request->hasParam(PARAM_AUTO_1)) {
    //   inputMessage = request->getParam(PARAM_AUTO_1)->value();
    //   writeFile(SPIFFS, "/auto1.txt", inputMessage.c_str());
    //   targetDripRate = inputMessage.toInt(); // convert the input from
    //   AGIS1 to integer,
    //                                  // and store in web_but_state
    // }
    else {
      inputMessage = "No message sent";
      // inputParam = "none";
    }
    Serial.println(inputMessage);

    // this page will br created after sending input, but I set that this page
    // will never enter Therefore, can comment it, but chrome will give
    // "net::ERR_EMPTY_RESPONSE" if don't add this page
    request->send(200, "text/html", "<a href=\"/\">Return to Home Page</a>");
    // request->send(200, "text/html", "Request Sent! <br>"
    //                                 + inputParam + " with value: " +
    //                                 inputMessage +
    //                                 "<br><a href=\"/get?" + inputParam +
    //                                 "=off\">Stop it</a><br>" +
    //                                 "<a href=\"/\">Return to Home Page</a>");
  });
  server.onNotFound(notFound); // if 404 not found, go to 404 not found
  AsyncElegantOTA.begin(&server); // for OTA update
  server.begin();
}

void loop() {
  // DEBUG:
  // Serial.printf(
  //     "dripRate: %u \ttarget_drip_rate: %u \tmotor_state: %s\tint_time2: %u\n",
  //     dripRate, targetDripRate, get_motor_state(motorState), timeBtw2Drops);
}

// check the condition of the switch/input from web page
// convert inputMessage(String) to integer
int check_state() {
  static int state;
  if (inputMessage == "Up") {
    state = 1;
  } else if (inputMessage == "Up_and_Down") {
    state = 2;
  } else if (inputMessage == "Down") {
    state = 3;
  } else if (inputMessage == "STOP") {
    state = 0;
  }
  return state;
}

void Motor_On_Up() {
  limitSwitch_Up.loop();   // MUST call the loop() function first

  if (limitSwitch_Up.getState()) { // untouched
    // Read PWM value
    PWMValue = analogRead(PWM_PIN);

    analogWrite(MOTOR_CTRL_PIN_1, (PWMValue / 16)); // PWMValue: 0->4095
    analogWrite(MOTOR_CTRL_PIN_2, 0);

    motorState = motorState_t::UP;
  }
  else { // touched
    Motor_Off();
  }
}

void Motor_On_Down() {
  limitSwitch_Down.loop();   // MUST call the loop() function first

  if (limitSwitch_Down.getState()) { // untouched
    // Read PWM value
    PWMValue = analogRead(PWM_PIN);

    analogWrite(MOTOR_CTRL_PIN_2, (PWMValue / 16)); // PWMValue: 0->4095
    analogWrite(MOTOR_CTRL_PIN_1, 0);

    motorState = motorState_t::DOWN;
  }
  else { // touched
    Motor_Off();
  }
}

void Motor_Off() {
  analogWrite(MOTOR_CTRL_PIN_1, 0);
  analogWrite(MOTOR_CTRL_PIN_2, 0);

  motorState = motorState_t::OFF;
}

const char *get_motor_state(motorState_t state) {
  switch (state) {
  case motorState_t::UP:
    return "UP";
  case motorState_t::DOWN:
    return "DOWN";
  case motorState_t::OFF:
    return "OFF";
  default:
    return "Undefined motor state";
    break;
  }
}

const char *get_button_state(buttonState_t state) {
  switch (state) {
  case buttonState_t::UP:
    return "UP";
  case buttonState_t::DOWN:
    return "DOWN";
  case buttonState_t::ENTER:
    return "ENTER";
  case buttonState_t::IDLE:
    return "IDLE";
  default:
    return "Undefined motor state";
    break;
  }
}

void alert(String x) {}

void initWebSocket() {
  ws.onEvent(onEvent);
  server.addHandler(&ws);
}

void onEvent(AsyncWebSocket *server, AsyncWebSocketClient *client,
             AwsEventType type, void *arg, uint8_t *data, size_t len) {
  switch (type) {
  case WS_EVT_CONNECT:
    Serial.printf("WebSocket client #%u connected from %s\n", client->id(),
                  client->remoteIP().toString().c_str());
    break;
  case WS_EVT_DISCONNECT:
    Serial.printf("WebSocket client #%u disconnected\n", client->id());
    break;
  case WS_EVT_DATA:
    handleWebSocketMessage(arg, data, len);
    break;
  case WS_EVT_PONG:
  case WS_EVT_ERROR:
    break;
  }
}

void handleWebSocketMessage(void *arg, uint8_t *data, size_t len) {
  // TODO: check github repo for official documentation
  AwsFrameInfo *info = (AwsFrameInfo *)arg;
  if (info->final && info->index == 0 && info->len == len &&
      info->opcode == WS_TEXT) {
    data[len] = 0;
    // DEBUG:
    // Serial.printf("Received from website: %s\n", (char *)data);

    // Parse the received WebSocket message as JSON
    DynamicJsonBuffer jsonBuffer;
    JsonObject &root = jsonBuffer.parseObject((const char *)data);
    if (!root.success()) {
      Serial.printf("Parse WebSocket message failed\n");
    } else {
      if (root.containsKey("SET_TARGET_DRIP_RATE_WS")) {
        targetDripRate = root["SET_TARGET_DRIP_RATE_WS"];
        targetVTBI = root["SET_VTBI_WS"];
        // convert total time to number of seconds
        unsigned int targetTotalTimeHours = root["SET_TOTAL_TIME_HOURS_WS"];
        unsigned int targetTotalTimeMinutes = root["SET_TOTAL_TIME_MINUTES_WS"];
        targetTotalTime = targetTotalTimeHours * 3600 +
                          targetTotalTimeMinutes * 60;
        dropFactor = root["SET_DROP_FACTOR_WS"];
        targetNumDrops = targetVTBI / (1.0f / dropFactor);  // rounded to integer part

        // DEBUG:
        // Serial.printf("---\n");
        // Serial.printf("Target VTBI is set to: %u mL\n", targetVTBI);
        // Serial.printf("Target total time is set to: %u seconds\n", targetTotalTime);
        // Serial.printf("Drop factor is set as: %u drops/mL\n", dropFactor);
        // Serial.printf("Target drip rate is set to: %u drops/min\n", targetDripRate);
        // Serial.printf("Target number of drops is: %d\n", targetNumDrops);
      }
      else if (root.containsKey("GET_DATA_WS")) {
        sendDataWs();
      }
    }
  }
}

void sendDataWs() {
  // TODO: check how to migrate to newest version of DynamicJsonBuffer
  DynamicJsonBuffer dataBuffer;
  JsonObject &root = dataBuffer.createObject();
  root["DROPPING_STATE"] = String(droppingState);  // need to convert to String
  root["TIME_1_DROP"] = time1Drop;
  root["TIME_BTW_2_DROPS"] = timeBtw2Drops;
  root["NUM_DROPS"] = numDrops;
  root["TOTAL_TIME"] = totalTime;
  root["DRIP_RATE"] = dripRate;

  // Calculate the infusedVolume here since we cannot do it in interrupt.
  // Only calculate when dropFactor is provided.
  // Problem is explained at:
  // https://esp32.com/viewtopic.php?f=19&t=1292&start=10
  if (dropFactor != UINT_MAX) {
    infusedVolume = numDrops * (1.0f / dropFactor);
  }

  root["INFUSED_VOLUME"] = infusedVolume;
  root["INFUSED_TIME"] = infusedTime;
  size_t len = root.measureLength();
  AsyncWebSocketMessageBuffer *buffer =
      ws.makeBuffer(len); //  creates a buffer (len + 1) for you.
  if (buffer) {
    root.printTo((char *)buffer->get(), len + 1);
    ws.textAll(buffer);
  }
}

// TODO: refactor: create a function to send json object as websocket message