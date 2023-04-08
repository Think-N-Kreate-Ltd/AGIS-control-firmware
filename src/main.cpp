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
  GPIO36 -> reading sensor data (Change in L28)
  timer0 -> read sensor & time measure
  timer1 -> auto control
  timer2 -> control the motor
*/

#include <Arduino.h>
#include <AsyncTCP.h>
#include <WiFiManager.h>
#include <ESPAsyncWebServer.h>
#include <SPIFFS.h>
#include <WiFi.h>
#include <ezButton.h>
#include <limits.h>

#define SENSOR_PIN 36 // input pin for geting output from sensor

#define motorCTRL_1 15 // Motorl Control Board PWM 1
#define motorCTRL_2 16 // Motorl Control Board PWM 2
#define ADCPin 4       // input pin for the potentiometer

enum motor_state_t{UP, DOWN, OFF};
motor_state_t motor_state = OFF;

// var for checking the time
volatile bool print_state = false; // true if it is printing currently
volatile bool phase_change_for_timer1 = false; // true if timer1 phase chenge
volatile int time_1ms = 0;         // count for every 1ms passed
volatile bool no_drop_with_20s = false;  // true if no drop appears in next 20s
volatile bool volume_exceed  = false;  // true if no_of_drop exceed amount

// var for timer0 interrupt
// for reading the sensor value
volatile int occur;                
// for measuring the time that sensor detect a drop
unsigned long start_time = 0;
// for measuring the time that sensor detect a drop is disappear
unsigned long leave_time = 0;
// for measuring the time that sensor detect the next drop
unsigned long next_time = 0;
unsigned long total_time = 0; // for calculating the time used within 15s
unsigned int no_of_drop = 0;  // for counting the number of drops within 15s
volatile unsigned int drip_rate = 0;   // for calculating the drip rate
String time1 = "not started"; // for storing the time of 1 drop
String time2 = "not started"; // for storing the time between 2 drop
volatile unsigned int int_time2=UINT_MAX;

// var for timer2 interrupt
int ADCValue = 0; // variable to store the value coming from the sensor
int Motor_Direction = LOW;
int DripMode = LOW;

ezButton button_UP(3);         // create ezButton object that attach to pin 6;
ezButton button_ENTER(8);      // create ezButton object that attach to pin 7;
ezButton button_DOWN(46);      // create ezButton object that attach to pin 8;
ezButton limitSwitch_Up(37);   // create ezButton object that attach to pin 7;
ezButton limitSwitch_Down(38); // create ezButton object that attach to pin 7;

// var for checking the currently condition
volatile bool but_state = false;  // true if it is controlled by the real button currently
volatile bool web_state = false;  // true if it is controlled by the web button currently
volatile bool auto_state = false;  // true if it is controlled automaticly currently
int web_but_state = 0; // that state that shows the condition of web button
unsigned int target_drip_rate = 0; // that state that shows the condition of auto control

volatile bool enable_autocontrol = false;  // to enable AutoControl() or not
volatile bool infuse_completed = false;    // true when infusion is completed

#define AUTO_CONTROL_ALLOW_RANGE  5  // To reduce the sensitive of AutoControl()
                                     // i.e. (target_drip_rate +/-5) is good enough
volatile bool init_state = true;  // true if droping is not started yet

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
void Motor_Run();
// void Motor_Mode();
const char* get_motor_state(enum motor_state_t state);

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

// replaces placeholder with stored values
String processor(const String &var) {
  // Serial.println(var);
  if (var == "input1") {
    return readFile(SPIFFS, "/input1.txt");
  } else if (var == "input2") {
    return readFile(SPIFFS, "/input2.txt");
  } else if (var == "input3") {
    return readFile(SPIFFS, "/input3.txt");
  } else if (var == "time1") {
    return time1;
  } else if (var == "time2") {
    return time2;
  } else if (var == "no_of_drop") {
    return String(no_of_drop);
  } else if (var == "total_time") {
    return String(total_time);
  } else if (var == "drop_rate") {
    return String(drip_rate);
  }
  return String();
}

// create pointer for timer
hw_timer_t *Timer0_cfg = NULL; // create a pointer for timer0
hw_timer_t *Timer1_cfg = NULL; // create a pointer for timer1
hw_timer_t *Timer2_cfg = NULL; // create a pointer for timer2

// timer0 interrupt, for sensor detected drops and measure the time
void IRAM_ATTR DropSensor() { 
  static int phase;
  static bool occur_state = false; // true when obstacle detected
  static int time_for_no_drop;  // counting when no drop appears, for measuring the time that have no drop
  if (phase == 0) {
    occur = digitalRead(SENSOR_PIN); // read the sensor value
    phase++;
  }

  if (phase == 1) {
    if (occur == 1) {
      time_for_no_drop = 0;
      no_drop_with_20s = false;
      init_state = false; // return false because droping is started
      if (!occur_state) { // condition that check for the drop is just detected
        occur_state = true;
        next_time = millis(); // record the time for measuring
        total_time += (next_time - start_time); // measure the time
        no_of_drop++; // counting the drop
        int_time2 = next_time - start_time; // measure the time
        time2 = String(int_time2) + "ms";
        start_time = millis();  // record th time for measuring
      }
    }
    if (occur == 0) {
      time_for_no_drop++;
      if (occur_state) {
        leave_time = millis();  // record the time for measuring
        occur_state = false;
        time1 = String(leave_time - start_time) + "ms"; // measure the time
      }
    }
    phase++;
  }
  if (phase == 2) {
    // call when no drop appears within 20s, reset all data
    if((time_for_no_drop >= 20000) && (!init_state)){ 
      time1 = "no drop appears currently";
      time2 = "no drop appears currently";
      // no_of_drop = 0;
      total_time = 0;
      no_drop_with_20s = true;

      // set int_time2 to a very large number
      int_time2 = UINT_MAX;
    }
    // call when the no of drops exceed target
    if(no_of_drop >= 500){
      volume_exceed = true;
      // TODO: alert volume exceed
      // alert("VolumeExceed");
    }
    phase = 0;
  }
  time_1ms++;         // count for 1ms
  print_state = true; // start printing
  phase_change_for_timer1 = true; // allow timer1 1 INT phase counting

  // get latest value of drip_rate
  drip_rate = 60000 / int_time2;  // TODO: explain this formular

  // NOTE: maybe we should average most recent drip_rate,
  // s.t. the auto control is not too sensitive and motor runs too frequently
}

void IRAM_ATTR AutoControl() { // timer1 interrupt, for auto control motor
  // Only run AutoControl() when the following conditions satisfy:
  //   1. button_ENTER is pressed
  //   2. target_drip_rate is set on the website by user
  //   3. infusion is not completed, i.e. infuse_completed = false
  // TODO: update value of infuse_completed in other function
  if(enable_autocontrol && (target_drip_rate!=0) && !infuse_completed){

    // TODO: alert when no drop is detected, i.e. could be out of fluid or get stuck

    // if currently SLOWER than set value -> speed up, i.e. move up
    if(drip_rate < (target_drip_rate - AUTO_CONTROL_ALLOW_RANGE)){
      Motor_On_Up();
      // analogWrite(motorCTRL_1, (1400 / 16));
      // analogWrite(motorCTRL_2, 0); 
    }
    
    // if currently FASTER than set value -> slow down, i.e. move down
    else if(drip_rate > (target_drip_rate + AUTO_CONTROL_ALLOW_RANGE)) {
      Motor_On_Down();
      // analogWrite(motorCTRL_2, (1400 / 16));
      // analogWrite(motorCTRL_1, 0); 
    }

    // otherwise, current drip rate is in allowed range -> stop motor
    else {
      Motor_Off();
    }
  }
  else {
    Motor_Off();
  }
}

void IRAM_ATTR motorControl() {
  static int phase;
  if (phase == 0) {
    button_UP.loop();        // MUST call the loop() function first
    button_ENTER.loop();     // MUST call the loop() function first
    button_DOWN.loop();      // MUST call the loop() function first
    limitSwitch_Up.loop();   // MUST call the loop() function first
    limitSwitch_Down.loop(); // MUST call the loop() function first
    phase++;
  }
  if (phase == 1) {
    ADCValue = analogRead(ADCPin); // read the value from the analog channel
    phase++;
  }
  if (phase == 2) {
    if (button_UP.isPressed()) {
      Motor_On_Up();
      but_state = true;
      web_but_state = 0;
    }
    if (button_UP.isReleased()) {
      Motor_Off();
      but_state = false;
    }
    if (button_ENTER.isPressed()) {
      // Use button_ENTER to toggle AutoControl()
      enable_autocontrol = !enable_autocontrol;

      // Before: use button_ENTER to go up and down
      // Motor_Mode();
      // if (but_state) {
      //   but_state = true;
      // }
      // if (!but_state) {
      //   but_state = false;
      //   web_but_state = 0;
      // }
    }
    if (button_DOWN.isPressed()) {
      Motor_On_Down();
      but_state = true;
      web_but_state = 0;
    }
    if (button_DOWN.isReleased()) {
      Motor_Off();
      but_state = false;
    }
    if (DripMode == HIGH) {
      Motor_Run();
    }

    if ((web_but_state == 1) && (!but_state) && (!auto_state)) {
      Motor_On_Up();
      web_state = true;
    }
    if ((web_but_state == 2) && (!but_state) && (!auto_state)) {
      DripMode = HIGH;
      web_state = true;
    }
    if ((web_but_state == 3) && (!but_state) && (!auto_state)) {
      Motor_On_Down();
      web_state = true;
    }
    if ((web_but_state == 0) && (!but_state) && (!auto_state)) {
      DripMode = LOW;
      // Motor_Off();
      web_state = false;
    }
    phase = 0;
  }
}

void setup() {
  Serial.begin(9600);
  pinMode(SENSOR_PIN, INPUT);

  // setup for timer0
  Timer0_cfg = timerBegin(0, 80, true); // Prescaler = 80
  timerAttachInterrupt(Timer0_cfg, &DropSensor,
                       true);              // call the function DropSensor()
  timerAlarmWrite(Timer0_cfg, 1000, true); // Time = 1000*80/80,000,000 = 1ms
  timerAlarmEnable(Timer0_cfg);            // start the interrupt

  // setup for timer1
  Timer1_cfg = timerBegin(1, 80, true); // Prescaler = 80
  timerAttachInterrupt(Timer1_cfg, &AutoControl,
                       true);              // call the function AutoControl()
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

  // Send web page to client
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(SPIFFS, "/index.html", String(), false, processor);
  });

  server.on("/style.css", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(SPIFFS, "/style.css", "text/css");
  });

  server.on("/script.js", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(SPIFFS, "/script.js", "text/javascript");
  });

  // send and data of sensor reading
  server.on("/sendsensor", [](AsyncWebServerRequest *request) {
    request->send(200, "text/html", String(occur));
  });
  // send and update the data of drops
  server.on("/sendtime1", [](AsyncWebServerRequest *request) {
    request->send(200, "text/html", time1);
  });
  server.on("/sendtime2", [](AsyncWebServerRequest *request) {
    request->send(200, "text/html", time2);
  });
  server.on("/sendno_of_drop", [](AsyncWebServerRequest *request) {
    request->send(200, "text/html", String(no_of_drop));
  });
  server.on("/sendtotal_time", [](AsyncWebServerRequest *request) {
    request->send(200, "text/html", String(total_time) + "ms");
  });
  server.on("/senddrop_rate", [](AsyncWebServerRequest *request) {
    request->send(200, "text/html", String(drip_rate));
  });

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
    else if (request->hasParam(PARAM_AUTO_1)) {
      inputMessage = request->getParam(PARAM_AUTO_1)->value();
      writeFile(SPIFFS, "/auto1.txt", inputMessage.c_str());
      target_drip_rate = inputMessage.toInt(); // convert the input from AGIS1 to integer,
                                     // and store in web_but_state
    } else {
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
  server.begin();
}

void loop() {

  Serial.printf("drip_rate: %u \ttarget_drip_rate: %u \tmotor_state: %s \tint_time2: %u\n",
                drip_rate, target_drip_rate, get_motor_state(motor_state), int_time2);

  // printing only
  // for debug use
  // if (((time_1ms == 5000) || (time_1ms == 10000)) &&
  //     print_state) { // 1ms *5000 =5s, print msg every 5s
  //   // To access your stored values on input1, input2, input3
  //   String yourInput1 = readFile(SPIFFS, "/input1.txt");
  //   Serial.print("*** Your input1: ");
  //   Serial.println(yourInput1);

  //   String yourInput2 = readFile(SPIFFS, "/input2.txt");
  //   Serial.print("*** Your input2: ");
  //   Serial.println(yourInput2);

  //   String yourInput3 = readFile(SPIFFS, "/input3.txt");
  //   Serial.print("*** Your input3: ");
  //   Serial.println(yourInput3);

  //   Serial.print("Motor Mode = ");
  //   Serial.println(DripMode); // print digital value on serial monitor
  //   Serial.print("web_but_state is ");
  //   Serial.println(web_but_state);

  //   Serial.print("The time of 1 drop is ");
  //   Serial.println(time1);

  //   Serial.print("The time between 2 drop is ");
  //   Serial.println(time2);

  //   Serial.print("Target drip rate is ");
  //   Serial.println(target_drip_rate);

  //   print_state = false; // finish print
  // }

  // if ((time_1ms == 15000) &&
  //     print_state) { // 1ms *15000 =15s, print msg every 15s
  //   Serial.print("The time of 1 drop is ");
  //   Serial.print(time1);
  //   Serial.println("ms");

  //   Serial.print("The time between 2 drop is ");
  //   Serial.print(time2);
  //   Serial.println("ms");

  //   Serial.print("15 seconds is passed, ");
  //   Serial.print(no_of_drop);
  //   Serial.print(" drop passed and use ");
  //   Serial.print(total_time);
  //   Serial.println("ms (not include the first drop)");

  //   time_1ms = 0;        // init value for next count
  //   print_state = false; // finish print
  // }
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
  if (limitSwitch_Up.isPressed()) {
    // TODO: fix the limit switch problem

    // Serial.println("The up limit switch: UNTOUCHED -> TOUCHED");
    Motor_Off();
  } else {
    analogWrite(motorCTRL_1,
                (ADCValue / 16)); // analogRead values go from 0 to 4095,
                                  // analogWrite values from 0 to 255
    analogWrite(motorCTRL_2, 0);

    motor_state = UP;
  }
}

void Motor_On_Down() {
  if (limitSwitch_Down.isPressed()) {
    // Serial.println("The down limit switch: UNTOUCHED -> TOUCHED");
    Motor_Off();
  } else {
    analogWrite(motorCTRL_2,
                (ADCValue / 16)); // analogRead values go from 0 to 4095,
                                  // analogWrite values from 0 to 255
    analogWrite(motorCTRL_1, 0);

    motor_state = DOWN;
  }
}

void Motor_Off() {
  analogWrite(motorCTRL_1, 0);
  analogWrite(motorCTRL_2, 0);

  motor_state = OFF;
}

void Motor_Run() {
  if (Motor_Direction == LOW) {
    Motor_On_Up();
    if (limitSwitch_Up.isPressed()) {
      Motor_Direction = HIGH;
    }
  }
  if (Motor_Direction == HIGH) {
    Motor_On_Down();
    if (limitSwitch_Down.isPressed()) {
      Motor_Direction = LOW;
    }
  }
}

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

const char* get_motor_state(motor_state_t state){
  switch (state){
    case UP: return "UP";
    case DOWN: return "DOWN";
    case OFF: return "OFF";
    default:
      return "Undefined motor state";
      break;
  }
}

void alert(String x){

}