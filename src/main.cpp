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
  GPIO2 -> reading sensor data (Change in L40)
  timer0 -> read sensor & time measure
  timer1 -> not used currently
  timer2 -> control the motor
  should work, but haven't practically tested yet
  do not use delay()
    because it may cause problem on measuring the time, just add var to
  PrintForDebug() to control the time (current is 5s run 1loop) sth improving:
  - will save the wifi connected but there may be a bug when first connected
  (should already be fixed)
  - use 3 button and one page only (done)
  - press -> action, release -> stop (finish half?), use toggle switch instead
  - the button have higher priority (done)
  - but stop up_and_down, just press once (normal case) (done)
  - use array to do the web page (seems not in needed, cause there are only 4
  timers, only 3 motor(or even less) can br controled at the same time)
  - use SPIFFS to store the data about drop time
  - update the data on web page without refreshing the whole page(AJAX) (done)
  - nothing in main loop (done)
*/
// uncomment L93-95 (the String storing the html code) before upload

#include <Arduino.h>
#include <AsyncTCP.h>
#include <WiFiManager.h>
#include <ESPAsyncWebServer.h>
#include <SPIFFS.h>
#include <WiFi.h>
#include <ezButton.h>

#define SENSOR_PIN 2 // input pin for geting output from sensor

#define motorCTRL_1 15 // Motorl Control Board PWM 1
#define motorCTRL_2 16 // Motorl Control Board PWM 2
#define ADCPin 4       // input pin for the potentiometer

// var for checking the time
volatile bool print_state = false; // true if it is printing currently
volatile int time_5ms = 0;         // count for every 5ms passed

// var for timer0 interrupt
// for measuring the time that sensor detect a drop
unsigned long start_time = 0;
// for measuring the time that sensor detect a drop is disappear
unsigned long leave_time = 0;
// for measuring the time that sensor detect the next drop
unsigned long next_time = 0;
unsigned long total_time = 0; // for calculating the time used within 15s
unsigned int no_of_drop = 0;  // for counting the number of drops within 15s
String time1 = "?";           // for storing the time of 1 drop
String time2 = "?";           // for storing the time between 2 drop

// var for timer2 interrupt
int ADCValue = 0; // variable to store the value coming from the sensor
int Motor_Direction = LOW;
int DripMode = LOW;

ezButton button_UP(3);         // create ezButton object that attach to pin 6;
ezButton button_ENTER(8);      // create ezButton object that attach to pin 7;
ezButton button_DOWN(46);      // create ezButton object that attach to pin 8;
ezButton limitSwitch_Up(37);   // create ezButton object that attach to pin 7;
ezButton limitSwitch_Down(38); // create ezButton object that attach to pin 7;

// var for button
volatile bool but_state =
    false;             // true if it is controlled by the real button currently
int web_but_state = 0; // that state that shows the condition of web button

// WiFiManager, Local intialization. Once its business is done, there is no need
// to keep it around
WiFiManager wm;

// var for web page
String inputMessage; // store the input from web page

// create AsyncWebServer object on port 80
AsyncWebServer server(80);

// REPLACE WITH YOUR NETWORK CREDENTIALS  // for hard-coding the wifi
// const char* ssid = "REPLACE_WITH_YOUR_SSID";
// const char* password = "REPLACE_WITH_YOUR_PASSWORD";

const char *PARAM_INPUT_1 = "input1";
const char *PARAM_INPUT_2 = "input2";
const char *PARAM_INPUT_3 = "input3";

// Function prototypes
int check_state();
void Motor_On_Up();
void Motor_On_Down();
void Motor_Run();
void Motor_Off();
void Motor_Mode();

// HTML web page to handle 3 input fields (input1, input2, input3)
const char index_html[] PROGMEM = R"rawliteral(
  <!DOCTYPE HTML><html> <head> <title>ESP Input Form</title> <meta name="viewport" content="width=device-width, initial-scale=1"> <style>.switch{position: relative; display: inline-block; width: 120px; height: 68px}.switch input{display: none}.slider{position: absolute; top: 0; left: 0; right: 0; bottom: 0; background-color: #ccc; border-radius: 6px}.slider:before{position: absolute; content: ""; height: 52px; width: 52px; left: 8px; bottom: 8px; background-color: #fff; -webkit-transition: .4s; transition: .4s; border-radius: 3px}input:checked+.slider{background-color: #b30000}input:checked+.slider:before{-webkit-transform: translateX(52px); -ms-transform: translateX(52px); transform: translateX(52px)}</style> <script>setInterval(function(){refreshCount();}, 5000); function refreshCount(){var xhttp=new XMLHttpRequest(); xhttp.onreadystatechange=function(){document.getElementById("sendValue").innerHTML=this.responseText;}; xhttp.open("GET", "sendValue", true); xhttp.send();}function getValue(){setTimeout(function(){document.location.reload(false);}, 10);}function sendInput(element){var xhr=new XMLHttpRequest(); if(element.checked){xhr.open("GET", "/get?input1=" + element.id, true);}else{xhr.open("GET", "/get?input1=STOP", true);}xhr.send();}</script> </head> <body> AGIS 1 UP<label class='switch'><input type='checkbox' onchange='sendInput(this)' id='Up'><span class='slider'></span></label> AGIS 1 Up and Down<label class='switch'><input type='checkbox' onchange='sendInput(this)' id='Up_and_Down'><span class='slider'></span></label> AGIS 1 Down<label class='switch'><input type='checkbox' onchange='sendInput(this)' id='Down'><span class='slider'></span></label> <br><form action='/get' target='hidden-form'> AGIS 1 (current value %input1%): <input name="input1" type='submit' value='Up' onchange='getValue()'> <input name="input1" type='submit' value='Up_and_Down' onclick='getValue()'> <input name="input1" type='submit' value='Down' onclick='getValue()'> <input name="input1" type='submit' value='STOP' onclick='getValue()'> </form><br><div id='sendValue'> Time for 1 drop is %time1%ms. Time between 2 drop is %time2%ms. <br>15 seconds is passed, %no_of_drop% drops passed and use %total_time%ms (not include the first drop). <br><br></div><form action='/get' target='hidden-form'> AGIS 2 (current value %input2%): <input name='input2' type='submit' value='Up' onclick='getValue()'> <input name='input2' type='submit' value='Up_and_Down' onclick='getValue()'> <input name='input2' type='submit' value='Down' onclick='getValue()'> <input name='input2' type='submit' value='STOP' onclick='getValue()'> </form><br><form action='/get' target='hidden-form'> AGIS 3 (current value %input3%): <input name='input3' type='submit' value='Up' onclick='getValue()'> <input name='input3' type='submit' value='Up_and_Down' onclick='getValue()'> <input name='input3' type='submit' value='Down' onclick='getValue()'> <input name='input3' type='submit' value='STOP' onclick='getValue()'> </form><br><iframe style="display:none" name="hidden-form"></iframe> </body></html>
)rawliteral";

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
  }
  return String();
}

hw_timer_t *Timer0_cfg = NULL; // create a pointer for timer0
hw_timer_t *Timer2_cfg = NULL; // create a pointer for timer2

void IRAM_ATTR DropSensor() { // timer0 interrupt, for sensor detected drops and
  // measure the time
  static int phase;
  static int occur;                // for reading the sensor value
  static bool occur_state = false; // true when obstacle detected
  if (phase == 0) {
    occur = digitalRead(SENSOR_PIN); // read the sensor value
    phase++;
  }

  if (phase == 1) {
    if (occur == 1) {
      if (!occur_state) { // check for the drop is just detected
        occur_state = true;
        next_time = millis();
        total_time += (next_time - start_time);
        no_of_drop++;
        time2 = String(next_time - start_time);
        start_time = millis();
        if (total_time >= 15000) { // 15 seconds passed
          time_5ms = 0;            // reset for timer1 to print result
          total_time = 0;
          no_of_drop = 0;
        }
      }
    }
    if (occur == 0) {
      if (occur_state) {
        leave_time = millis();
        occur_state = false;
        time1 = String(leave_time - start_time);
      }
    }
    phase = 0;
  }
  time_5ms++;         // count for 5ms
  print_state = true; // start printing
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
      Motor_Mode();
      if (but_state) {
        but_state = true;
      }
      if (!but_state) {
        but_state = false;
        web_but_state = 0;
      }
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

    if ((web_but_state == 1) && (!but_state)) {
      Motor_On_Up();
    }
    if ((web_but_state == 2) && (!but_state)) {
      DripMode = HIGH;
    }
    if ((web_but_state == 3) && (!but_state)) {
      Motor_On_Down();
    }
    if ((web_but_state == 0) && (!but_state)) {
      DripMode = LOW;
      Motor_Off();
    }
    phase = 0;
  }
}

void setup() {
  Serial.begin(9600);
  pinMode(SENSOR_PIN, INPUT);

  // setup for timer0
  Timer0_cfg = timerBegin(0, 400, true); // Prescaler = 400
  timerAttachInterrupt(Timer0_cfg, &DropSensor,
                       true);              // call the function DropSensor()
  timerAlarmWrite(Timer0_cfg, 1000, true); // Time = 1000*500/80,000,000 = 5ms
  timerAlarmEnable(Timer0_cfg);            // start the interrupt

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
    request->send_P(200, "text/html", index_html, processor);
  });

  // send and update the data of drops
  server.on("/sendValue", [](AsyncWebServerRequest *request) {
    request->send(200, "text/html",
                  "Time for 1 drop is " + time1 +
                      "ms. Time between 2 drop is " + time2 +
                      "ms. <br> 15 seconds is passed, " + no_of_drop +
                      " drops passed and use " + total_time +
                      "ms (not include the first drop). <br><br>");
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
    } else {
      inputMessage = "No message sent";
      // inputParam = "none";
    }
    Serial.println(inputMessage);

    // this page will br created after sending input, but I set that this page
    // will never enter Therefore, can comment it but chrome will give
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
  // printing only
  // for debug use
  if (((time_5ms == 1000) || (time_5ms == 2000)) &&
      print_state) { // 5ms *1000 =5s, print msg every 5s
    // To access your stored values on input1, input2, input3
    String yourInput1 = readFile(SPIFFS, "/input1.txt");
    Serial.print("*** Your input1: ");
    Serial.println(yourInput1);

    String yourInput2 = readFile(SPIFFS, "/input2.txt");
    Serial.print("*** Your input2: ");
    Serial.println(yourInput2);

    String yourInput3 = readFile(SPIFFS, "/input3.txt");
    Serial.print("*** Your input3: ");
    Serial.println(yourInput3);

    Serial.print("Motor Mode = ");
    Serial.println(DripMode); // print digital value on serial monitor
    Serial.print("web_but_state is ");
    Serial.println(web_but_state);

    Serial.print("The time of 1 drop is ");
    Serial.print(time1);
    Serial.println("ms");

    Serial.print("The time between 2 drop is ");
    Serial.print(time2);
    Serial.println("ms");

    print_state = false; // finish print
  }

  if ((time_5ms == 3000) &&
      print_state) { // 5ms *3000 =15s, print msg every 15s
    Serial.print("The time of 1 drop is ");
    Serial.print(time1);
    Serial.println("ms");

    Serial.print("The time between 2 drop is ");
    Serial.print(time2);
    Serial.println("ms");

    Serial.print("15 seconds is passed, ");
    Serial.print(no_of_drop);
    Serial.print(" drop passed and use ");
    Serial.print(total_time);
    Serial.println("ms (not include the first drop)");

    time_5ms = 0;        // init value for next count
    print_state = false; // finish print
  }
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
    Serial.println("The up limit switch: UNTOUCHED -> TOUCHED");
    Motor_Off();
  } else {
    analogWrite(motorCTRL_1,
                (ADCValue / 16)); // analogRead values go from 0 to 4095,
                                  // analogWrite values from 0 to 255
    analogWrite(motorCTRL_2, 0);
  }
}

void Motor_On_Down() {
  if (limitSwitch_Down.isPressed()) {
    Serial.println("The down limit switch: UNTOUCHED -> TOUCHED");
    Motor_Off();
  } else {
    analogWrite(motorCTRL_2,
                (ADCValue / 16)); // analogRead values go from 0 to 4095,
                                  // analogWrite values from 0 to 255
    analogWrite(motorCTRL_1, 0);
  }
}

void Motor_Off() {
  analogWrite(motorCTRL_1, 0);
  analogWrite(motorCTRL_2, 0);
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

void Motor_Mode() {
  if (DripMode == LOW) {
    DripMode = HIGH;
    but_state = true;
  } else {
    DripMode = LOW;
    but_state = false;
    Motor_Off();
  }
}