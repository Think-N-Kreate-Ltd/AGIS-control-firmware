/*
  wifi ssid:AutoConnectAP, password:password.
  go to http://<IPAddress>/update for OTA update
  upload firmware.bin for main, spiffs.bin for SPIFFS files
  GPIO36 -> EXT interrupt for reading sensor data
  timer0 -> INT interrupt for auto control
  timer1 -> INT interrupt for motor control by keypad

  Problem will occur when homing and click "Set and Run" at the same time
*/

#include <Arduino.h>
#include <AsyncTCP.h>
#include <WiFiManager.h>      // define before <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <SPI.h>
#include "SdFat.h"
#include <LittleFS.h>
#include <WiFi.h>
#include <limits.h>
#include <ArduinoJson.h>
#include "AsyncElegantOTA.h"  // define after <ESPAsyncWebServer.h>
#include <AGIS_Commons.h>
#include <AGIS_OLED.h>
#include <AGIS_Types.h>       // user defined data types
#include <AGIS_Utilities.h>
#include <AGIS_Display.h>
#include <AGIS_INA219.h>
#include <AGIS_SD.h>
#include <esp_log.h>

// NOTE: it is used for deleting task, but task delete is decrecated
TaskHandle_t xHandle = NULL;

// var use for testing
volatile int testCount = 0;
volatile long testTime = 0;

#define DROP_SENSOR_PIN  36 // input pin for geting output from sensor
#define SENSOR_LED_PIN   35 // output pin to sensor for toggling LED
#define MOTOR_CTRL_PIN_1 15 // Motorl Control Board PWM 1
#define MOTOR_CTRL_PIN_2 16 // Motorl Control Board PWM 2
#define PWM_PIN          4  // input pin for the potentiometer

// var for EXT interrupt (sensor)
volatile unsigned int numDrops = 0;       // for counting the number of drops within 15s
volatile unsigned int dripRate = 0;       // for calculating the drip rate
volatile unsigned int timeBtw2Drops = UINT_MAX; // i.e. no more drop recently
volatile unsigned int dripRatePeak = 1;   // drip rate at the position when 1st drop is detected
uint8_t dripFactor[] = {10, 15, 20, 60, 100};   // an array to store the option of drip factor
size_t lengthOfDF = sizeof(dripFactor)/sizeof(dripFactor[0]);

// var for reading PWM value, for controlling motor
volatile int PWMValue = 0; // PWM value to control the speed of motor

// var for showing the input/target of auto control
unsigned int targetDripRate = 0;     // the DR that the infusion should be
unsigned int targetVTBI = 0;         // target total volume to be infused
unsigned int targetTotalTime = 0;    // the time that the infusion should spend
unsigned int targetNumDrops = UINT_MAX; // the number of drops that the infusion should give
unsigned int dropFactor = UINT_MAX;  // init-ly at max to avoid divide by zero, unit: drops/mL

// var for tracking the infusion
volatile unsigned int infusedVolume_x100 = 0;  // 100 times larger than actual value, unit: mL
volatile unsigned int infusedTime = 0;         // unit: seconds
volatile unsigned long infusionStartTime = 0;

// main state that check the infusion and commonly used
volatile char dropState = 'F';           // stating the condition of drop, 'T'=true(have drop currently), 'F'=false(for telling sensor LED should dark), 'W'=waiting(Sensor LED not blinked yet, but drop leave the sensor region)
volatile bool enableAutoControl = false; // to enable AutoControl() or not
volatile bool enableLogging = false;     // true when (start) doing logging
volatile bool firstDropDetected = false; // to check when we receive the 1st drop
buttonState_t buttonState = buttonState_t::IDLE;
infusionState_t infusionState = infusionState_t::NOT_STARTED;
volatile bool motorHoming = true;   // will directly go to homing when true
volatile int homingCompletedTime;   // the time that homing completed 

// To reduce the sensitive of autoControlISR()
// i.e. (targetDripRate +/-3) is good enough
#define AUTO_CONTROL_ALLOW_RANGE 3
#define AUTO_CONTROL_ON_TIME_MAX 200  // motor will be enabled for this amount of time at maximum (unit: ms)
#define AUTO_CONTROL_ON_TIME_MIN 30   // motor will be enabled for this amount of time at minimum (unit: ms)
#define AUTO_CONTROL_TOTAL_TIME_FAST    500  // 500ms
#define AUTO_CONTROL_TOTAL_TIME_NORMAL  1000  // 1000ms
#define DROP_DEBOUNCE_TIME       10   // if two pulses are generated within 10ms, it must be detected as 1 drop

// create AsyncWebServer object on port 80
AsyncWebServer server(80);
AsyncWebSocket ws("/ws");
// TODO: use websocket for communication

// Function prototypes
void motorOnUp();
void motorOnDown();
void motorOff();
void initWebSocket();
void onEvent(AsyncWebSocket *server, AsyncWebSocketClient *client,
             AwsEventType type, void *arg, uint8_t *data, size_t len);
void handleWebSocketMessage(void *arg, uint8_t *data, size_t len);
void sendInfusionMonitoringDataWs();
void resetValues();
int volumeCount(bool reset = false);
void loggingData(void * parameter);
void getI2CData(void * arg);
void tftDisplay(void * arg);
void oledDisplay(void * arg);
void enableWifi(void * arg);
void otherLittleWorks(void * arg);
void homingRollerClamp(void * arg);
// void taskWifiDelete();

// goto 404 not found when 404 not found
void notFound(AsyncWebServerRequest *request) {
  request->send(404, "text/plain", "Not found");
}

// create pointer for timer
hw_timer_t *Timer0_cfg = NULL; // create a pointer for timer0
hw_timer_t *Timer1_cfg = NULL; // create a pointer for timer1

// EXT interrupt to pin 36, for sensor detected drops and measure the time
void IRAM_ATTR dropSensorISR() {
  static int lastState;    // var to record the last value of the sensor
  static int lastTime;     // var to record the last value of the calling time
  static int lastDropTime; // var to record the time of last drop

  // in fact, the interrupt will only be called when state change
  // just one more protection to prevent calling twice when state doesn't change
  int dropSensorState = digitalRead(DROP_SENSOR_PIN);
  if (lastState != dropSensorState) {
    lastState = dropSensorState;
    // call when drop detected
    // disable for `DROP_DEBOUNCE_TIME` after called
    if ((dropSensorState == 1) && 
        ((millis()-lastTime)>=DROP_DEBOUNCE_TIME)) {
      dropState = 'T'; // turn on LED on drop sensor on task

      lastTime = millis();

      // FIRST DROP DETECTION
      if (!firstDropDetected){
        firstDropDetected = true;
        lastDropTime = -9999; // prevent timeBtw2Drops become inf

        if ((infusionState != infusionState_t::PAUSED)) {
          if (infusionState != infusionState_t::ALARM_STOPPED) {
            // STOPPED and PAUSED are special states which note that the condition is stopped/paused, but still in progress
            // mark this as starting time of infusion
            infusionStartTime = millis();
            infusedVolume_x100 = volumeCount(true); // NOTE: this is not needed as it has been reseted already
          } else {
            // change the state from stopped back to in pregress
            infusionState = infusionState_t::IN_PROGRESS;
          }
        }
      }

      if (infusionState == infusionState_t::NOT_STARTED) {
        infusionState = infusionState_t::STARTED; // droping has started
      }

      // record the value
      timeBtw2Drops = millis() - lastDropTime;
      lastDropTime = millis();
      numDrops++;
      dripRate = 60000 / timeBtw2Drops;

      // NOTE: Since we cannot do floating point calculation in interrupt,
      // we multiply the actual infused volume by 100 times to perform the integer calculation
      // Later when we need to display, divide it by 100 to get actual value.
      for (int i=0; i<lengthOfDF; i++) {
        if (dropFactor == dripFactor[i]) {
          infusedVolume_x100 = volumeCount();
        }
      }

      // Check if infusion has completed or not
      // acceptable for exceeding 1 drops (error)
      if ((numDrops >= targetNumDrops) && (numDrops <= (targetNumDrops+1))) {
        infusionState = infusionState_t::ALARM_COMPLETED;

        // finish auto ctrl
        enableAutoControl = false;
        motorHoming = true;
      
      } else if ((infusionState == infusionState_t::ALARM_COMPLETED) && ((millis() - homingCompletedTime) >= 200) && !motorHoming) {
        // when finish infusion, it will go homing first, that time may also have drops
        // for those drop, should not let the state go to exceeded
        infusionState = infusionState_t::ALARM_VOLUME_EXCEEDED;
      }

      // get dripRatePeak, i.e. drip rate when 1st drop is detected
      if (firstDropDetected) {
        dripRatePeak = max(dripRatePeak, dripRate);
      }
    } else if (dropSensorState == 0) {  // NOTE: may get into else while debouncing, so, just jeep the condition
      dropState = 'W'; // send that the drop leave the sensor region
    }
  } 
}

void IRAM_ATTR autoControlISR() { // timer1 interrupt, for auto control motor

  static bool motorOnPeriod = false;          // true = motor should move, false = motor should stop
  static unsigned int recordTime = millis();  // var to measure the time interval
  static unsigned int motorOnTime = 0;        // var to store the time that motor should move
  static unsigned int motorInterval = 0;      // var to store the stop time of the motor

  // Checking for no drop for 20s
  static int timeWithNoDrop = millis();
  if ((dropState == 'F') && firstDropDetected) {  // the time btw state = 'F' & 'W' have nearly no difference
  // so that the time measurement will start after detect drop
    if ((millis() - timeWithNoDrop) >= 20000) { 
      // reset these values
      firstDropDetected = false;
      timeBtw2Drops = UINT_MAX;
      dripRate = 0;

      if ((infusionState == infusionState_t::ALARM_COMPLETED) || 
          (infusionState == infusionState_t::ALARM_VOLUME_EXCEEDED) || 
          (infusionState == infusionState_t::STARTED)) {
        // reset value on display as the last infusion is finished
        infusionState = infusionState_t::NOT_STARTED;
        infusedTime = 0;
        infusionStartTime = millis(); // prevent there is one more calculation for `infusedTime`
        numDrops = 0;
        infusedVolume_x100 = volumeCount(true);
      }

      // infusion is still in progress but we cannot detect drops for 20s,
      // something must be wrong, sound the alarm
      if (infusionState == infusionState_t::IN_PROGRESS) {
        infusionState = infusionState_t::ALARM_STOPPED;
      }
    }
  } else if (infusionState == infusionState_t::ALARM_STOPPED) {
    if ((getPinState(LS_UP_PIN) == 0) && ((millis() - timeWithNoDrop) >= 28000)) {
      // reset the time to ensure that statement here will only run once
      timeWithNoDrop = millis();
      // stop and finish the infusion
      motorHoming = true;
      enableAutoControl = false;
      infusionState = infusionState_t::ALARM_OUT_OF_FLUID;
    }
  } else {
    timeWithNoDrop = millis();
  }

  // get infusion time so far:
  if ((infusionState != infusionState_t::ALARM_COMPLETED) &&
      (infusionState != infusionState_t::ALARM_OUT_OF_FLUID) && firstDropDetected) {
    infusedTime = (millis() - infusionStartTime) / 1000;  // in seconds
  }

  // belows are the auto ctrl of the motor

  // set the value of `motorOnPeriod`
  if (firstDropDetected) {
    // on for `motorOnTime` ms, off for `motorInterval` ms
    motorOnPeriod = (millis()-recordTime) <= motorOnTime;
  } else {  // before the first drop, move quickly to find the init position
    motorOnPeriod = true;  // move freely, no interval
  }

  if (enableAutoControl && motorOnPeriod && (targetDripRate != 0)) {
    // if currently SLOWER than set value -> speed up, i.e. move up
    if (dripRate < (targetDripRate - AUTO_CONTROL_ALLOW_RANGE)) {
      motorOnUp();
    }
    // if currently FASTER than set value -> slow down, i.e. move down
    else if (dripRate > (targetDripRate + AUTO_CONTROL_ALLOW_RANGE)) {
      motorOnDown();
    }
    // otherwise, current drip rate is in allowed range -> stop motor
    else {
      motorOff();
    }
  } else if ( enableAutoControl) {
    motorOff();
  }

  // find the interval to next motor period
  int difference = abs(long(dripRate - targetDripRate));
  if (difference <= 10) {
    motorInterval = AUTO_CONTROL_TOTAL_TIME_NORMAL;
  }
  else {
    motorInterval = AUTO_CONTROL_TOTAL_TIME_FAST;
  }

  if ((millis()-recordTime) >= motorInterval) {   // reset count every `motorInterval`
    recordTime = millis();

    // calculate new motorOnTime based on the absolute difference
    // between dripRate and targetDripRate
    motorOnTime =
        max(difference * AUTO_CONTROL_ON_TIME_MAX / dripRatePeak,
            (unsigned int)AUTO_CONTROL_ON_TIME_MIN);
  }
}

void IRAM_ATTR motorControlISR() {
  static bool pressing = false;  // check for the key is pressing or not
  // not to control motor when doing homing
  if (!motorHoming) {
    // Use keypad `U` to manually move up
    if (buttonState == buttonState_t::UP) {
      motorOnUp();
      pressing = true;
    }

    // Use keypad `D` to manually move down
    if (buttonState == buttonState_t::DOWN) {
      motorOnDown();
      pressing = true;
    }

    // Use keypad `*` to pause / resume / reset the infusion
    // ensure it will only run once when holding the key
    // when the data is not decided, it should not do anything (the first infusion must not start by this)
    if (buttonState == buttonState_t::ENTER && !pressing 
        && (targetDripRate != 0) && (targetNumDrops != UINT_MAX)) {
      // pause / resume the infusion
      if (enableAutoControl) {
        infusionState = infusionState_t::PAUSED;
      } else {
        if ((infusionState == infusionState_t::NOT_STARTED) || (infusionState == infusionState_t::STARTED)
            || (infusionState == infusionState_t::ALARM_COMPLETED) || (infusionState == infusionState_t::ALARM_VOLUME_EXCEEDED)
            || (infusionState == infusionState_t::ALARM_OUT_OF_FLUID)) {
          // for the case that use `*` to start auto ctrl, not recommended to do so
          // set all vars same as keypad infusion confirmed
          resetValues();
          firstDropDetected = false;
          enableLogging = true;
        }
        infusionState = infusionState_t::IN_PROGRESS;
      }
      enableAutoControl = !enableAutoControl;

      // if press it twice within 500ms, reset the infusion
      static int recordTime;
      if ((millis()-recordTime)<500 && ((infusionState == infusionState_t::IN_PROGRESS) 
          || (infusionState == infusionState_t::PAUSED))) {
        // reset the value
        // infusionInit();
        motorHoming = true;
        enableAutoControl = false;

        // stop droping and mark as complete
        infusionState = infusionState_t::ALARM_COMPLETED;
        /*auto control will do homing*/
      }
      recordTime = millis();
      pressing = true;
    } 
  } else if (buttonState != buttonState_t::IDLE) {
    // clear btn state after homing, to prevent legacy problem
    buttonState = buttonState_t::IDLE;
    pressing = false;
  }

  // it will only run once each time
  if (buttonState == buttonState_t::IDLE && pressing) {
    motorOff();
    pressing = false;
  }
}

void setup() {
  Serial.begin(115200);
  pinMode(DROP_SENSOR_PIN, INPUT);
  pinMode(SENSOR_LED_PIN, OUTPUT);
  digitalWrite(SENSOR_LED_PIN, HIGH); // prevent the LED is turned on initially
  pinMode(SD_CS, OUTPUT);
  pinMode(TFT_CS, OUTPUT);

  ina219SetUp();
  oledSetUp();
  
  useSdCard();  // compulsorily change to communicate with SD
  sdCardSetUp();      

  // setup for sensor interrupt
  attachInterrupt(DROP_SENSOR_PIN, &dropSensorISR, CHANGE);  // call interrupt when state change

  // setup for timer0
  Timer0_cfg = timerBegin(0, 400, true);    // prescaler = 400
  timerAttachInterrupt(Timer0_cfg, &autoControlISR,
                       false);             // call the function autoControlISR()
  timerAlarmWrite(Timer0_cfg, 1000, true); // time = 400*1000/80,000,000 = 5ms
  timerAlarmEnable(Timer0_cfg);            // start the interrupt

  // setup for timer1
  Timer1_cfg = timerBegin(1, 800, true);    // Prescaler = 800
  timerAttachInterrupt(Timer1_cfg, &motorControlISR,
                       false);             // call the function motorcontrol()
  timerAlarmWrite(Timer1_cfg, 1000, true); // Time = 800*1000/80,000,000 = 10ms
  timerAlarmEnable(Timer1_cfg);            // start the interrupt

  /*Initialize TFT display, LVGL*/
  display_init();

  /*Create a task for data logging*/
  xTaskCreate(loggingData,       /* Task function. */
              "Data Logging",    /* String with name of task. */
              4096,              /* Stack size in bytes. */
              NULL,              /* Parameter passed as input of the task */
              4,                 /* Priority of the task. */
              NULL);             /* Task handle. */

  // I2C is too slow that cannot use interrupt
  xTaskCreate(getI2CData,     // function that should be called
              "Get I2C Data", // name of the task (debug use)
              4096,           // stack size
              NULL,           // parameter to pass
              1,              // task priority, 0-24, 24 highest priority
              NULL);          // task handle
  
  // Create a task for TFT display
  xTaskCreate(tftDisplay,       // function that should be called
              "TFT display",    // name of the task (debug use)
              4096,             // stack size
              NULL,             // parameter to pass
              3,                // task priority, 0-24, 24 highest priority
              NULL);            // task handle

  // Create a task for OLED display
  xTaskCreate(oledDisplay,      // function that should be called
              "OLED display",   // name of the task (debug use)
              4096,             // stack size
              NULL,             // parameter to pass
              2,                // task priority, 0-24, 24 highest priority
              NULL);            // task handle

  xTaskCreate(enableWifi,     // function that should be called
              "Enable WiFi",  // name of the task (debug use)
              4096,           // stack size
              NULL,           // parameter to pass
              24,             // task priority, 0-24, 24 highest priority
              &xHandle);      // task handle

  // *Create a task for different kinds of little things
  xTaskCreate(otherLittleWorks,                   // function that should be called
              "Different kinds of little things", // name of the task (debug use)
              4096,           // stack size
              NULL,           // parameter to pass
              0,              // task priority, 0-24, 24 highest priority
              NULL);          // task handle

  // *Create a task for different kinds of little things
  xTaskCreate(homingRollerClamp,      // function that should be called
              "Homing roller clamp",  // name of the task (debug use)
              4096,           // stack size
              NULL,           // parameter to pass
              0,              // task priority, 0-24, 24 highest priority
              NULL);          // task handle
}

void loop() {}

void motorOnUp() {
  if (getPinState(LS_UP_PIN)) { // untouched
    // Read PWM value
    PWMValue = analogRead(PWM_PIN);

    analogWrite(MOTOR_CTRL_PIN_1, (PWMValue / 16)); // PWMValue: 0->4095
    analogWrite(MOTOR_CTRL_PIN_2, 0);
  }
  else { // touched
    motorOff();
  }
}

void motorOnDown() {
  if (getPinState(LS_DOWN_PIN)) { // untouched
    // Read PWM value
    PWMValue = analogRead(PWM_PIN);

    analogWrite(MOTOR_CTRL_PIN_2, (PWMValue / 16)); // PWMValue: 0->4095
    analogWrite(MOTOR_CTRL_PIN_1, 0);
  }
  else { // touched
    motorOff();
  }
}

void motorOff() {
  analogWrite(MOTOR_CTRL_PIN_1, 0);
  analogWrite(MOTOR_CTRL_PIN_2, 0);
}

void initWebSocket() {
  ws.onEvent(onEvent);
  server.addHandler(&ws);
}

void onEvent(AsyncWebSocket *server, AsyncWebSocketClient *client,
             AwsEventType type, void *arg, uint8_t *data, size_t len) {
  switch (type) {
  case WS_EVT_CONNECT:
    ESP_LOGI(WEBSOCKET_TAG, "WebSocket client #%u connected from %s",
             client->id(), client->remoteIP().toString().c_str());
    break;
  case WS_EVT_DISCONNECT:
    ESP_LOGI(WEBSOCKET_TAG, "WebSocket client #%u disconnected",
             client->id());
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
    // ESP_LOGD(WEBSOCKET_TAG, "Received from website: %s", (char *)data);

    // Parse the received WebSocket message as JSON
    DynamicJsonDocument doc(1024);
    DeserializationError error = deserializeJson(doc, (const char *)data);
    if (error) {
      ESP_LOGE(WEBSOCKET_TAG, "deserializeJson() failed: %s", error.c_str());
    } else {
      if (doc.containsKey("SET_TARGET_DRIP_RATE_WS")) {
        targetDripRate = doc["SET_TARGET_DRIP_RATE_WS"];
        targetVTBI = doc["SET_VTBI_WS"];
        // convert total time to number of seconds
        unsigned int targetTotalTimeHours = doc["SET_TOTAL_TIME_HOURS_WS"];
        unsigned int targetTotalTimeMinutes = doc["SET_TOTAL_TIME_MINUTES_WS"];
        targetTotalTime = targetTotalTimeHours * 3600 +
                          targetTotalTimeMinutes * 60;
        dropFactor = doc["SET_DROP_FACTOR_WS"];
        targetNumDrops = targetVTBI / (1.0f / dropFactor);  // rounded to integer part

        // DEBUG:
        ESP_LOGD(WEBSOCKET_TAG, "Target VTBI is set to: %u mL", targetVTBI);
        ESP_LOGD(WEBSOCKET_TAG, "Target total time is set to: %u seconds", targetTotalTime);
        ESP_LOGD(WEBSOCKET_TAG, "Drop factor is set as: %u drops/mL", dropFactor);
        ESP_LOGD(WEBSOCKET_TAG, "Target drip rate is set to: %u drops/min", targetDripRate);
        ESP_LOGD(WEBSOCKET_TAG, "Target number of drops is: %d", targetNumDrops);
      }
      else if (doc.containsKey("COMMAND")) {
        // parse the command and execute
        if (doc["COMMAND"] == "ENABLE_AUTOCONTROL_WS") {
          resetValues();

          // override the ENTER button to enable autoControl()
          // seems useless already as it will go to in pregress directly
          infusionState = infusionState_t::IN_PROGRESS;
          enableAutoControl = true;
          firstDropDetected = false;  // to reset the firstdrop data before

          enableLogging = true;
        }
        else if (doc["COMMAND"] == "GET_INFUSION_MONITORING_DATA_WS") {
          sendInfusionMonitoringDataWs();

          // we also want to log the infusion data to file
          // frequency of logging is set from script.js file
          // if ((infusionState == infusionState_t::IN_PROGRESS ||
          //      infusionState == infusionState_t::ALARM_COMPLETED) &&
          //      !loggingCompleted) {
          //   loggingCompleted = logInfusionMonitoringData(logFilePath);
          // }
        }
        else {
          ESP_LOGE(WEBSOCKET_TAG, "Command undefined");
        }
      }
    }
  }
}

void sendInfusionMonitoringDataWs() {
  DynamicJsonDocument doc(1024);
  JsonObject root = doc.to<JsonObject>();
  root["INFUSION_STATE"] = getInfusionState(infusionState);
  root["TIME_BTW_2_DROPS"] = timeBtw2Drops;
  root["NUM_DROPS"] = numDrops;
  root["DRIP_RATE"] = dripRate;
  root["INFUSED_VOLUME"] = infusedVolume_x100 / 100.0f;
  root["INFUSED_TIME"] = infusedTime;
  char buffer[1024];
  size_t len = serializeJson(root, buffer);
  ws.textAll(buffer);
}

// TODO: refactor: create a function to send json object as websocket message

// Reset infusion parameters
void resetValues() {
  infusedTime = 0;
  infusionStartTime = millis(); // prevent there is one more calculation for `infusedTime`
  numDrops = 0;
  infusedVolume_x100 = volumeCount(true);
  /**DR is not reseted as it cannot be 0 at start
   * time between 2 drops is used for cal DR, and will not be displayed
   * state is not going to be not started, but in-progress
  */
}

// to calculate and store the accurate volume
// default perimeter is false, change to true to reset volume
int volumeCount(bool reset) {
  static int volume = 0;
  static int remainder = 0;
  if (reset) {
    volume = 0;
    remainder = 0;
  } else {
    volume += ((100+remainder) / dropFactor);
    remainder = (100+remainder) % dropFactor;
  }
  // return the volume which is used for display
  return volume;
}

void loggingData(void * parameter) {
  // set up, only run once
  // rmOldData(); // move to `enableWifi` as this is not needed with no wifi connection
  static bool finishLogging = false;

  for (;;) {
    if (enableLogging) {
      newFileInit();  // create new file and header
      ESP_LOGI(DATA_LOGGING_TAG, "Logging initialized");
      
      // after create file, do data logging
      while ((infusionState == infusionState_t::IN_PROGRESS) || 
            (infusionState == infusionState_t::ALARM_STOPPED))  {
        logData();
      }

      finishLogging = true;
    }

    // only run once when finish
    /**condition of state is not needed here because:
     * 1. the state `finishLogging` will only change after condition enablelogging
     *    where that condition will only satsify when auto-ctrl
     * 2. under the condition, it will keep doing the while loop(logging)
     *    as a result, it will not change the value of `finishLogging` until logging finish
     * 3. `finishLogging` will change back to false after run statements once
     * To conclude, `finishLogging` will only not be true when state is:
     *              not started, started, in-progress, paused, stopped
     *              thus, add state as a condition to check it is redundant
    */
    if (/*(infusionState == infusionState_t::ALARM_COMPLETED) &&*/ finishLogging) {
      while (motorHoming) {
        // wait for homing complete to log the last data
        vTaskDelay(200);
      }
      endLogging();
      finishLogging = false;
      enableLogging = false;
      useSdCard(false);
    }

    vTaskDelay(500);
  }
}

void getI2CData(void * arg) {
  for (;;) {
    getIna219Data();
    vTaskDelay(449);
  }
}

void tftDisplay(void * arg) {
  // get the screen object
  input_screen();
  vTaskDelay(20);  // avoid CPU crashing
  ask_for_wifi_enable_msgbox();
  monitor_screen();

  for(;;) {
    lv_timer_handler(); // Should be call periodically
    vTaskDelay(5);      // The timing is not critical but it should be about 5 milliseconds to keep the system responsive
  }
}

void oledDisplay(void * arg) {
  for(;;) {
    display.clearDisplay();

    /*show gtt/m on the left side of display*/
    display.setCursor(0, 15);
    display.printf("%d\n", dripRate);
    display.setCursor(0, 40);
    display.printf("gtt/m\n");

    /*show mL/h on the right side of display*/
    display.setCursor(80, 15);
    // Convert from drops/min to mL/h:
    display.printf("%d\n", dripRate * (60 / dropFactor));
    display.setCursor(80, 40);
    display.printf("mL/h\n");

    display.display();

    vTaskDelay(500);  // block for 500ms
  }
}

// TODO: see how to deal with it
void enableWifi(void * arg) {
  while (!wifiStart) {
    // waiting for response, for loop forever if no enable wifi
    vTaskDelay(2000);
    ESP_LOGD(WIFI_TAG, "waiting, or not enabled");
  }
  // WiFiManager, Local intialization. Once its business is done, there is no need to keep it around
  WiFiManager wm;
  
  // connect wifi
  WiFi.mode(WIFI_STA);  // wifi station mode

  // reset settings - wipe stored credentials for testing
  // these are stored by the esp library
  // wm.resetSettings();

  if (!wm.autoConnect("AutoConnectAP",
                      "password")) { // set esp32-s3 wifi ssid and pw to
    // AutoConnectAP & password
    ESP_LOGE(WIFI_TAG, "Failed to connect");
    ESP.restart();
  } else {
    // if you get here you have connected to the WiFi
    ESP_LOGI(WIFI_TAG, "connected...yeah :)");
  }

  if (WiFi.waitForConnectResult() != WL_CONNECTED) {
    ESP_LOGE(WIFI_TAG, "WiFi Failed!");
    return;
  }

  // print the IP address of the web page
  ESP_LOGI(WIFI_TAG, "IP Address: %s", WiFi.localIP().toString());

  // Initialize LittleFS
  if (!LittleFS.begin(true)) {
    ESP_LOGE(LITTLE_FS_TAG, "An Error has occurred while mounting LittleFS");
    return;
  }

  // Init Websocket
  initWebSocket();

  // Send web page to client
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(LittleFS, "/index.html", String(), false);
  });

  server.serveStatic("/", LittleFS, "/");

  // force download file
  server.on("/log", HTTP_GET, [](AsyncWebServerRequest *request) {
    loadFromSdCard(request);
  });

  server.onNotFound(notFound); // if 404 not found, go to 404 not found
  AsyncElegantOTA.begin(&server); // for OTA update
  server.begin();

  // remove sd card old data
  rmOldData();

  /*NOTE: The idle task is responsible for freeing the RTOS kernel allocated memory from tasks that have been deleted.
    It is therefore important that the idle task is not starved of microcontroller processing time if your application makes any calls to vTaskDelete ().
    Memory allocated by the task code is not automatically freed, and should be freed before the task is deleted.
    NOTE: check for how to free the memory <- `taskWifiDelete()`
    UPDATE: seems it may have problem for web page & download log file, just keep it may be better*/

  vTaskDelete(NULL);  // delete itself
}

void otherLittleWorks(void * arg) {
  for(;;) {
    // toggle LED
    if (dropState == 'T') {
      digitalWrite(SENSOR_LED_PIN, LOW);     // reversed because the LED is pull up
      vTaskDelay(10);
    } else if (dropState == 'W') {
      digitalWrite(SENSOR_LED_PIN, LOW);     // reversed because the LED is pull up
      dropState = 'F';  // must change the state first
      vTaskDelay(50);   // the state can change back to T/W when blocking here
    }

    // Handle keypad
    if (keypadInfusionConfirmed) {
      ESP_LOGI(KEYPAD_TAG, "Keypad inputs confirmed");
      resetValues();

      // override the ENTER button to enable autoControl()
      // seems useless already as it will go to in pregress directly
      infusionState = infusionState_t::IN_PROGRESS;
      enableAutoControl = true;
      firstDropDetected = false;  // to reset the firstdrop data before

      // enable logging task
      enableLogging = true;

      // make sure this if statement runs only once
      keypadInfusionConfirmed = false;
    }

    // NOTE: if there are more and more jobs in the future, may create a new bool var to hold this condition
    while ((dropState == 'F') && !keypadInfusionConfirmed) {
      if (digitalRead(SENSOR_LED_PIN) == LOW) {
        digitalWrite(SENSOR_LED_PIN, HIGH);  // reversed because the LED is pull up
      }

      vTaskDelay(50);
    }
  }
}

void homingRollerClamp(void * arg) {
  for(;;) {
    if (getPinState(LS_DOWN_PIN) == 0) {  // touched
      motorHoming = false;
      motorOff();
      homingCompletedTime = millis();
    } else {
      // motor move down
      analogWrite(MOTOR_CTRL_PIN_2, 191); // Value: 0->255
      analogWrite(MOTOR_CTRL_PIN_1, 0);
    }

    while (!motorHoming) {
      vTaskDelay(200);
    }

    vTaskDelay(50);
  }
}

// void taskWifiDelete() {
//   TaskHandle_t xTask = xHandle;
//   vTaskSuspendAll();

//   if( xHandle != NULL )
//   {
//       /* The task is going to be deleted.Set the handle to NULL. */
//       xHandle = NULL;

//       /* Delete using the copy of the handle. */
//       vTaskDelete( xTask );
//       Serial.println("deleted");
//   }
//   xTaskResumeAll();
// }