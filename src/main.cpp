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

#define SENSOR_PIN 36 // input pin for geting output from sensor

#define motorCTRL_1 15 // Motorl Control Board PWM 1
#define motorCTRL_2 16 // Motorl Control Board PWM 2
#define PWM_PIN      4  // input pin for the potentiometer

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
volatile bool print_state = false; // true if it is printing currently
volatile bool phase_change_for_timer1 = false; // true if timer1 phase chenge
volatile int time_1ms = 0;                     // count for every 1ms passed
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
volatile unsigned int dripRate = 0;   // for calculating the drip rate
volatile unsigned int time1Drop = 0;      // for storing the time of 1 drop
volatile unsigned int timeBtw2Drops = UINT_MAX; // i.e. no more drop recently

// var for timer2 interrupt
int PWMValue = 0; // PWM value to control the speed of motor
int Motor_Direction = LOW;
int DripMode = LOW;

ezButton button_UP(3);         // create ezButton object that attach to pin 6;
ezButton button_ENTER(8);      // create ezButton object that attach to pin 7;
ezButton button_DOWN(46);      // create ezButton object that attach to pin 8;
ezButton limitSwitch_Up(37);   // create ezButton object that attach to pin 7;
ezButton limitSwitch_Down(38); // create ezButton object that attach to pin 7;

// var for checking the currently condition
// true if it is controlled by the real button currently
volatile bool but_state = false;
 // true if it is controlled by the web button currently
volatile bool web_state = false;
// true if it is controlled automaticly currently
volatile bool auto_state = false;
// state that shows the condition of web button
int web_but_state = 0; 
// state that shows the condition of auto control
unsigned int targetDripRate = 0; 

volatile bool enableAutoControl = false; // to enable AutoControl() or not
volatile bool infuseCompleted = false;   // true when infusion is completed

// To reduce the sensitive of autoControl()
// i.e. (targetDripRate +/-5) is good enough
#define AUTO_CONTROL_ALLOW_RANGE 5

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
  static int phase;
  static bool occur_state = false; // true when obstacle detected
  static int time_for_no_drop; // counting when no drop appears, for measuring
                               // the time that have no drop
  if (phase == 0) {
    occur = digitalRead(SENSOR_PIN); // read the sensor value
    phase++;
  }

  if (phase == 1) {
    if (occur == 1) {
      time_for_no_drop = 0;
      no_drop_with_20s = false;
      droppingState = droppingState_t::STARTED; // droping has started
      if (!occur_state) { // condition that check for the drop is just detected
        occur_state = true;
        next_time = millis();                  // record the time for measuring
        totalTime += (next_time - start_time); // measure the time
        numDrops++;                            // counting the drop
        timeBtw2Drops = next_time - start_time;    // measure the time
        start_time = millis(); // record th time for measuring
      }
    }
    if (occur == 0) {
      time_for_no_drop++;
      if (occur_state) {
        leave_time = millis(); // record the time for measuring
        occur_state = false;
        time1Drop = leave_time - start_time;
      }
    }
    phase++;
  }
  if (phase == 2) {
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
    }
    // call when the no of drops exceed target
    if (numDrops >= 500) {
      volume_exceed = true;
      // TODO: alert volume exceed
      // alert("VolumeExceed");
    }
    phase = 0;
  }
  time_1ms++;                     // count for 1ms
  print_state = true;             // start printing
  phase_change_for_timer1 = true; // allow timer1 1 INT phase counting

  // get latest value of dripRate
  dripRate = 60000 / timeBtw2Drops; // TODO: explain this formular

  // NOTE: maybe we should average most recent dripRate,
  // s.t. the auto control is not too sensitive and motor runs too frequently
}

void IRAM_ATTR autoControl() { // timer1 interrupt, for auto control motor
  // Only run autoControl() when the following conditions satisfy:
  //   1. button_ENTER is pressed
  //   2. targetDripRate is set on the website by user
  //   3. infusion is not completed, i.e. infuseCompleted = false
  // TODO: update value of infuseCompleted in other function
  if (enableAutoControl && (targetDripRate != 0) && !infuseCompleted) {

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
  }

  if (button_UP.isReleased() || button_DOWN.isReleased() || button_ENTER.isReleased()) {
    buttonState = buttonState_t::IDLE;
    Motor_Off();
  }
}

void setup() {
  Serial.begin(9600);
  pinMode(SENSOR_PIN, INPUT);

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

    analogWrite(motorCTRL_1, (PWMValue / 16)); // PWMValue: 0->4095
    analogWrite(motorCTRL_2, 0);

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

    analogWrite(motorCTRL_2, (PWMValue / 16)); // PWMValue: 0->4095
    analogWrite(motorCTRL_1, 0);

    motorState = motorState_t::DOWN;
  }
  else { // touched
    Motor_Off();
  }
}

void Motor_Off() {
  analogWrite(motorCTRL_1, 0);
  analogWrite(motorCTRL_2, 0);

  motorState = motorState_t::OFF;
}

// void Motor_Run() {
//   if (Motor_Direction == LOW) {
//     Motor_On_Up();
//     if (limitSwitch_Up.isPressed()) {
//       Motor_Direction = HIGH;
//     }
//   }
//   if (Motor_Direction == HIGH) {
//     Motor_On_Down();
//     if (limitSwitch_Down.isPressed()) {
//       Motor_Direction = LOW;
//     }
//   }
// }

// void Motor_Mode() {
//   if (DripMode == LOW) {
//     DripMode = HIGH;
//     but_state = true;
//   } else {
//     DripMode = LOW;
//     but_state = false;
//     Motor_Off();
//   }
// }

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
        Serial.printf("Target drip rate is set to: %u\n", targetDripRate);
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
  size_t len = root.measureLength();
  AsyncWebSocketMessageBuffer *buffer =
      ws.makeBuffer(len); //  creates a buffer (len + 1) for you.
  if (buffer) {
    root.printTo((char *)buffer->get(), len + 1);
    ws.textAll(buffer);
  }
}

// TODO: refactor: create a function to send json object as websocket message