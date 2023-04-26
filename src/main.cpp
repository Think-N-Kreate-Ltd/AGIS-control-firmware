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
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <AsyncElegantOTA.h>  // define after <ESPAsyncWebServer.h>

// TODO: refactor names, follow standard naming conventions

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_TEXT_FONT 2

#define OLED_MOSI   17
#define OLED_CLK    47
#define OLED_DC     5
#define OLED_CS     6
#define OLED_RESET  7

#define DROP_SENSOR_PIN  36 // input pin for geting output from sensor
// #define SENSOR_IN 35  // input pin for input signal to sensor
#define MOTOR_CTRL_PIN_1 15 // Motorl Control Board PWM 1
#define MOTOR_CTRL_PIN_2 16 // Motorl Control Board PWM 2
#define PWM_PIN          4  // input pin for the potentiometer

enum class motorState_t { UP, DOWN, OFF };
motorState_t motorState = motorState_t::OFF;


enum class buttonState_t { UP, DOWN, ENTER, IDLE };
buttonState_t buttonState = buttonState_t::IDLE;

// NOTE: when infusionState_t type is modified, update the same type in script.js 
enum class infusionState_t {
  NOT_STARTED,           // when the board is powered on and no drops detected
  STARTED,               // as soon as drops are detected
  IN_PROGRESS,           // when infusion is started by user and not completed yet
  ALARM_COMPLETED,       // when infusion has completed, i.e. infusedVolume reaches the target volume
  ALARM_STOPPED,         // when infusion stopped unexpectly, it's likely to have a problem
  ALARM_VOLUME_EXCEEDED  // when infusion has completed but we still detect drops
  // add more states here when needed
};
// Initially, infusionState is NOT_STARTED
infusionState_t infusionState = infusionState_t::NOT_STARTED;

// set up for OLED display
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT,
  OLED_MOSI, OLED_CLK, OLED_DC, OLED_RESET, OLED_CS);

void oledSetUp() {
  // Initialize OLED
  if(!display.begin(SSD1306_SWITCHCAPVCC)) {
    Serial.println(F("SSD1306 allocation failed"));
    return;
  } else {
    // clear the original display on the screen
    display.clearDisplay();
    display.display();
  }
}

// TODO: delete time1Drop, totalTime
// var for EXT interrupt (sensor)
volatile unsigned long totalTime = 0; // for calculating the time used within 15s
volatile unsigned int numDrops = 0;   // for counting the number of drops within 15s
volatile unsigned int dripRate = 0;   // for calculating the drip rate
volatile unsigned int time1Drop = 0;  // for storing the time of 1 drop
volatile unsigned int timeBtw2Drops = UINT_MAX; // i.e. no more drop recently
ezButton sensor_READ(DROP_SENSOR_PIN);     // create ezButton object that attach to pin 36;

// var for timer1 interrupt
volatile float infusedVolume = 0;  // unit: mL
volatile unsigned long infusedTime = 0;     // unit: seconds
volatile unsigned long infusionStartTime = 0;

volatile unsigned int autoControlCount = 0;  // use for regulating frequency of motor is on
volatile unsigned int autoControlOnTime = 0;  // use for regulating frequency of motor is on
int dripRateDifference = 0; 
volatile unsigned int dripRatePeak = 1;   // drip rate at the position when 1st drop is detected,
                                          // set to 1 to avoid zero division

// var for timer2 interrupt
int PWMValue = 0; // PWM value to control the speed of motor

ezButton button_UP(3);         // create ezButton object that attach to pin 6;
ezButton button_ENTER(8);      // create ezButton object that attach to pin 7;
ezButton button_DOWN(46);      // create ezButton object that attach to pin 8;
ezButton limitSwitch_Up(37);   // create ezButton object that attach to pin 7;
ezButton limitSwitch_Down(38); // create ezButton object that attach to pin 7;

// var for checking the currently condition
// state that shows the condition of web button
int web_but_state = 0; 
// state that shows the condition of auto control
unsigned int targetDripRate = 0; 
unsigned int targetVTBI = 0;   // target total volume to be infused
unsigned int targetTotalTime = 0;   // target total time to be infused
unsigned int targetNumDrops = UINT_MAX;    // used for stopping infusion when complete
unsigned int dropFactor = UINT_MAX;  // to avoid divide by zero, unit: drops/mL

volatile bool enableAutoControl = false; // to enable AutoControl() or not

volatile bool firstDropDetected = false; // to check when we receive the 1st drop
volatile bool autoControlOnPeriod = false;
bool homingCompleted = false;   // true when lower limit switch is activated

// To reduce the sensitive of autoControl()
// i.e. (targetDripRate +/-3) is good enough
#define AUTO_CONTROL_ALLOW_RANGE 3
#define AUTO_CONTROL_ON_TIME_MAX 600  // motor will be enabled for this amount of time at maximum (unit: ms)
#define AUTO_CONTROL_ON_TIME_MIN 30   // motor will be enabled for this amount of time at minimum (unit: ms)
#define AUTO_CONTROL_TOTAL_TIME  1000  // 1000ms
#define DROP_DEBOUNCE_TIME       10   // if two pulses are generated within 10ms, it must be detected as 1 drop

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
void tableOledDisplay(int i, int j, int k);
void alertOledDisplay(const char* s);
int getLastDigit(int n);
int check_state();
void motorOnUp();
void motorOnDown();
void motorOff();
const char *getMotorState(motorState_t state);
const char *getButtonState(buttonState_t state);
const char *getInfusionState(infusionState_t state);
void initWebSocket();
void onEvent(AsyncWebSocket *server, AsyncWebSocketClient *client,
             AwsEventType type, void *arg, uint8_t *data, size_t len);
void handleWebSocketMessage(void *arg, uint8_t *data, size_t len);
void sendInfusionMonitoringDataWs();
void homingRollerClamp();
void infusionInit();

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
hw_timer_t *Timer3_cfg = NULL; // create a pointer for timer0

// EXT interrupt to pin 36, for sensor detected drops and measure the time
void IRAM_ATTR dropSensor() {
  static int lastState;    // var to record the last value of the sensor
  static int lastTime;     // var to record the last value of the calling time
  static int lastDropTime; // var to record the time of last drop

  // in fact, the interrupt will only be called when state change
  // just one more protection to prevent calling twice when state doesn't change
  if (lastState != sensor_READ.getStateRaw()) {
    lastState = sensor_READ.getStateRaw();
    // call when drop detected
    // disable for 10 ms after called
    if ((sensor_READ.getStateRaw() == 1) && 
        ((millis()-lastTime)>=DROP_DEBOUNCE_TIME)) {
      lastTime = millis();

      // FIRST DROP DETECTION
      if (!firstDropDetected){
        firstDropDetected = true;
        lastDropTime = -9999; // prevent timeBtw2Drops become inf
      }
      if (infusionState != infusionState_t::IN_PROGRESS) {
        // TODO: when click "Set and Run" button on the website again to
        // start another infusion, infusionState should be IN_PROGRESS but
        // somehow it is STARTED
        infusionState = infusionState_t::STARTED; // droping has started
      }

      // record the value
      timeBtw2Drops = millis() - lastDropTime;
      lastDropTime = millis();
      numDrops++;

      // if infusion has completed but we still detect drop,
      // something must be wrong. Need to sound the alarm.
      if (infusionState == infusionState_t::ALARM_COMPLETED) {
        infusionState = infusionState_t::ALARM_VOLUME_EXCEEDED;
      }
      
      // get latest value of dripRate
      // explain: dripRate = 60 seconds / time between 2 consecutive drops
      dripRate = 60000 / timeBtw2Drops;

      // get dripRatePeak, i.e. drip rate when 1st drop is detected
      if (firstDropDetected) {
        dripRatePeak = max(dripRatePeak, dripRate);
      }

      // get infusion time so far:
      if (infusionState != infusionState_t::ALARM_COMPLETED) {
        infusedTime = (millis() - infusionStartTime) / 1000;  // in seconds
      }
    } else if (sensor_READ.getStateRaw() == 0) {/*nothing*/}
  } 
}

// timer3 inerrupt, for I2C OLED display
void IRAM_ATTR OledDisplayISR(){
  if (infusionState == infusionState_t::ALARM_COMPLETED) {
    alertOledDisplay("infusion \ncompleted");
  } else if (infusionState == infusionState_t::ALARM_VOLUME_EXCEEDED) {
    alertOledDisplay("volume \nexceeded");
  } else if (infusionState == infusionState_t::ALARM_STOPPED) {
    alertOledDisplay("no recent \ndrop");
  } else {
    tableOledDisplay(numDrops/dropFactor, getLastDigit(numDrops*10/dropFactor), getLastDigit(numDrops*100/dropFactor));
  }
}

void setup() {
  Serial.begin(9600);
  pinMode(DROP_SENSOR_PIN, INPUT);

  oledSetUp();
  
  // setup for sensor interrupt
  attachInterrupt(DROP_SENSOR_PIN, &dropSensor, CHANGE);  // call interrupt when state change

  // setup for timer3
  Timer3_cfg = timerBegin(3, 4000, true); // Prescaler = 4000
  timerAttachInterrupt(Timer3_cfg, &OledDisplayISR,
                       true);              // call the function OledDisplayISR()
  timerAlarmWrite(Timer3_cfg, 1000, true); // Time = 4000*1000/80,000,000 = 50ms
  timerAlarmEnable(Timer3_cfg);            // start the interrupt

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


  // homing the roller clamp
  while (!homingCompleted) {
    homingRollerClamp();
  }
}

void loop() {
  // DEBUG:
  // Serial.printf(
  //     "dripRate: %u \ttarget_drip_rate: %u \tmotor_state: %s\n",
  //     dripRate, targetDripRate, getMotorState(motorState));

}

// display the table on the screen
// only one display.display() should be used
void tableOledDisplay(int i, int j, int k) {
  // initialize setting of display
  display.clearDisplay();
  display.setTextSize(OLED_TEXT_FONT);
  display.setTextColor(SSD1306_WHITE);  // draw 'on' pixels

  // display.setCursor(1,16);  // set the position of the first letter
  // display.printf("Drip rate: %d\n", dripRate);

  // display.setCursor(1,24);  // set the position of the first letter
  // display.printf("Infused volume: %d.%d%d\n", i, j, k);

  // display.setCursor(1,32);  // set the position of the first letter
  // // if less than 1hour / 1minute, then not to display them
  // if((infusedTime/3600) >= 1){
  //   display.printf("Infused time: \n%dh %dmin %ds\n", infusedTime/3600, (infusedTime%3600)/60, infusedTime%60);
  // } else if((infusedTime/60) >= 1){
  //   display.printf("Infused time: \n%dmin %ds\n", infusedTime/60, infusedTime%60);
  // } else {
  //   display.printf("Infused time: \n%ds\n", infusedTime%60);
  // }

  display.setCursor(1,16);  // set the position of the first letter
  display.printf("%d.%d%dmL\n", i, j, k);
  if((infusedTime/3600) >= 1){
    display.printf("%dh%dm%ds\n", infusedTime/3600, (infusedTime%3600)/60, infusedTime%60);
  } else if((infusedTime/60) >= 1){
    display.printf("%dm%ds\n", infusedTime/60, infusedTime%60);
  } else {
    display.printf("%ds\n", infusedTime%60);
  }
  
  display.display();  
}

// display the warning message on the screen
void alertOledDisplay(const char* s) {
  display.clearDisplay();
  display.setTextSize(OLED_TEXT_FONT);
  display.setTextColor(SSD1306_WHITE);

  display.setCursor(1,16);
  display.println(F("ALARM: "));
  display.println(F(s));
  display.display();
}

// get the last digit of a number
int getLastDigit(int n) {
  static int j;
  static int k;
  j = (n / 10) * 10;
  k = n - j;
  return k;
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

void motorOnUp() {
  limitSwitch_Up.loop();   // MUST call the loop() function first

  if (limitSwitch_Up.getState()) { // untouched
    // Read PWM value
    PWMValue = analogRead(PWM_PIN);

    analogWrite(MOTOR_CTRL_PIN_1, (PWMValue / 16)); // PWMValue: 0->4095
    analogWrite(MOTOR_CTRL_PIN_2, 0);

    motorState = motorState_t::UP;
  }
  else { // touched
    motorOff();
  }
}

void motorOnDown() {
  limitSwitch_Down.loop();   // MUST call the loop() function first

  if (limitSwitch_Down.getState()) { // untouched
    // Read PWM value
    PWMValue = analogRead(PWM_PIN);

    analogWrite(MOTOR_CTRL_PIN_2, (PWMValue / 16)); // PWMValue: 0->4095
    analogWrite(MOTOR_CTRL_PIN_1, 0);

    motorState = motorState_t::DOWN;
  }
  else { // touched
    motorOff();
  }
}

void motorOff() {
  analogWrite(MOTOR_CTRL_PIN_1, 0);
  analogWrite(MOTOR_CTRL_PIN_2, 0);

  motorState = motorState_t::OFF;
}

const char *getMotorState(motorState_t state) {
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

const char *getButtonState(buttonState_t state) {
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

const char *getInfusionState(infusionState_t state) {
  switch (state) {
  case infusionState_t::NOT_STARTED:
    return "NOT_STARTED";
  case infusionState_t::STARTED:
    return "STARTED";
  case infusionState_t::IN_PROGRESS:
    return "IN_PROGRESS";
  case infusionState_t::ALARM_COMPLETED:
    return "ALARM_COMPLETED";
  case infusionState_t::ALARM_STOPPED:
    return "ALARM_STOPPED";
  default:
    return "Undefined infusion state";
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
      else if (root.containsKey("COMMAND")) {
        // parse the command and execute
        if (root["COMMAND"] == "ENABLE_AUTOCONTROL_WS") {
          infusionInit();

          // override the ENTER button to enable autoControl()
          enableAutoControl = true;
        }
        else if (root["COMMAND"] == "GET_INFUSION_MONITORING_DATA_WS") {
          sendInfusionMonitoringDataWs();
        }
        else {
          Serial.printf("Command undefined\n");
        }
      }
    }
  }
}

void sendInfusionMonitoringDataWs() {
  // TODO: check how to migrate to newest version of DynamicJsonBuffer
  DynamicJsonBuffer dataBuffer;
  JsonObject &root = dataBuffer.createObject();
  root["INFUSION_STATE"] = getInfusionState(infusionState);
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

// Move down the roller clamp to completely closed position
// Copied and modified from motorOnDown()
void homingRollerClamp() {
  limitSwitch_Down.loop();   // MUST call the loop() function first

  if (limitSwitch_Down.getState()) { // untouched
    // Read PWM value
    PWMValue = analogRead(PWM_PIN);

    analogWrite(MOTOR_CTRL_PIN_2, (PWMValue / 16)); // PWMValue: 0->4095
    analogWrite(MOTOR_CTRL_PIN_1, 0);

    motorState = motorState_t::DOWN;
  }
  else { // touched
    motorOff();
    homingCompleted = true;
  }
}

void infusionInit() {
  // Reset infusion parameters the first time button_ENTER is pressed.
  // Parameters need to be reset:
  //    (1) numDrops
  //    (2) infusedVolume
  //    (3) infusedTime
  //    Add more if necessary
  numDrops = 0;
  infusedVolume = 0.0f;
  infusedTime = 0;
  infusionState = infusionState_t::IN_PROGRESS;

  // TODO: start timing from here is not correct
  // we should start timing from when we receive the first drop
  // mark this as starting time of infusion
  infusionStartTime = millis();

  homingCompleted = false;  // if not set, the infusion cannot be stopped
}