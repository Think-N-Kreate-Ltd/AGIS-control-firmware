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
  GPIO2 -> reading sensor data (Change in L28)
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

#define SENSOR_PIN 36 // input pin for geting output from sensor

#define motorCTRL_1 15 // Motorl Control Board PWM 1
#define motorCTRL_2 16 // Motorl Control Board PWM 2
#define ADCPin 4       // input pin for the potentiometer

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
volatile int drop_rate = 0;           // for calculating the drop rate
String time1 = "not started"; // for storing the time of 1 drop
String time2 = "not started"; // for storing the time between 2 drop
volatile int int_time2=1;

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
int web_auto_state = 0; // that state that shows the condition of auto control

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
const char *PARAM_AUTO_1 = "auto1";

// Function prototypes
int check_state();
int check_t();
void Motor_On_Up();
void Motor_On_Down();
void Motor_Off();
void Motor_Run();
void Motor_Mode();

// HTML web page to handle 3 input fields (input1, input2, input3)
const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML>
<html>

  <head>
    <title>ESP Input Form</title>
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <script src="https://code.highcharts.com/highcharts.js"></script>
    <script src="https://code.highcharts.com/modules/exporting.js"></script>
    <script src="https://code.highcharts.com/modules/export-data.js"></script>
    <script src="https://code.highcharts.com/modules/accessibility.js"></script>
    <style>
      .switch {position: relative; display: inline-block; width: 120px; height: 68px} 
      .switch input {display: none}
      .slider {position: absolute; top: 0; left: 0; right: 0; bottom: 0; background-color: #ccc; border-radius: 6px}
      .slider:before {position: absolute; content: ""; height: 52px; width: 52px; left: 8px; bottom: 8px; background-color: #fff; -webkit-transition: .4s; transition: .4s; border-radius: 3px}
      input:checked+.slider {background-color: #b30000}
      input:checked+.slider:before {-webkit-transform: translateX(52px); -ms-transform: translateX(52px); transform: translateX(52px)}
    </style>
    <script>
    	var state_auto = false;
      var DR = 0;
      var drop_rate = 0;
      var t = "0";
      setInterval(function(){ refreshtime1(); refreshtime2(); refreshno_of_drop(); refreshtotal_time(); refreshdrop_rate();}, 1000);
      var tot_time = 0;
      var no_drop = 99;
      var drop_rate = 0;
      function refreshtime1(){
        var xhttp = new XMLHttpRequest();
        xhttp.onreadystatechange = function() {
            document.getElementById("sendtime1").innerHTML = this.responseText;
        };
        xhttp.open("GET", "sendtime1", true);
        xhttp.send();
      }
      function refreshtime2(){
        var xhttp = new XMLHttpRequest();
        xhttp.onreadystatechange = function() {
            document.getElementById("sendtime2").innerHTML = this.responseText;
            if(((this.responseText) === "not started") || ((this.responseText) === "no drop appears currently")){
              drop_rate = 0;
              console.log("equal")
            }
            else{
              drop_rate = 60000 / parseInt(this.responseText);
              console.log("not equal")
            }
        };
        xhttp.open("GET", "sendtime2", true);
        xhttp.send();
      }
      function refreshno_of_drop(){
        var xhttp = new XMLHttpRequest();
        xhttp.onreadystatechange = function() {
            document.getElementById("sendno_of_drop").innerHTML = this.responseText;
            no_drop = parseInt(this.responseText);
        };
        xhttp.open("GET", "sendno_of_drop", true);
        xhttp.send();
      }
      function refreshtotal_time(){
        var xhttp = new XMLHttpRequest();
        xhttp.onreadystatechange = function() {
            document.getElementById("sendtotal_time").innerHTML = (parseFloat(this.responseText) / 1000).toString();
        };
        xhttp.open("GET", "sendtotal_time", true);
        xhttp.send();
      }
      function refreshdrop_rate(){
        var xhttp = new XMLHttpRequest();
        xhttp.onreadystatechange = function() {
            document.getElementById("senddrop_rate").innerHTML = drop_rate;
        };
        xhttp.open("GET", "senddrop_rate", true);
        xhttp.send();
      }
      function getValue() {
        setTimeout(function() {
          document.location.reload(false);
        }, 10);
      }
      function sendInput(element){
        var xhr = new XMLHttpRequest();
        if(element.checked){xhr.open("GET", "/get?input1=" + element.id, true);}
        else{xhr.open("GET", "/get?input1=STOP", true);}
        xhr.send();
      }
      function getDR(){
        DR = (document.getElementById("AGIS1").value).toString();
        state_auto = true;
        alert("clicked");
        var xhr = new XMLHttpRequest();
        xhr.open("GET", "/get?auto1=" + DR, true);
        xhr.send
      }
      function autoControl(){
        var xhttp = new XMLHttpRequest();
        xhttp.onreadystatechange = function() {
          if(state_auto){
            if(DR < drop_rate){
              t = "down";
              document.getElementById("test").innerHTML += "down";
          	}
            if(DR < (drop_rate - 5)){
          	  t = "moredown";
              document.getElementById("test").innerHTML += "downnn";
          	}
            if(DR > drop_rate){
              t = "up";
              document.getElementById("test").innerHTML += "up";
          	}
            if(DR > (drop_rate + 5)){
          	  t = "moreup";
              document.getElementById("test").innerHTML += "uppp";
          	}
            if(DR == drop_rate){
              t = "0";
              document.getElementById("test").innerHTML = "stop";
            }
          }
        };
        xhttp.open("GET", "/get?auto1=" + t, true);
        xhttp.send();
      }

    </script>
  </head>

  <body>
    AGIS 1 UP<label class='switch'><input type='checkbox' onchange='sendInput(this)' id='Up'><span class='slider'></span></label>
    AGIS 1 Up and Down<label class='switch'><input type='checkbox' onchange='sendInput(this)' id='Up_and_Down'><span class='slider'></span></label>
    AGIS 1 Down<label class='switch'><input type='checkbox' onchange='sendInput(this)' id='Down'><span class='slider'></span></label>
    <br>
    <form action='/get' target='hidden-form'> 
    AGIS 1 (current value %input1%): 
    <input name="input1" type='submit' value='Up' onchange='getValue()'> 
    <input name="input1" type='submit' value='Up_and_Down' onclick='getValue()'> 
    <input name="input1" type='submit' value='Down' onclick='getValue()'> 
    <input name="input1" type='submit' value='STOP' onclick='getValue()'> 
    </form><br>

    <label for="AGIS1">Enter Drip Rate:</label>
    <input type="text" id="AGIS1" name="AGIS1">
    <input type="submit" value="AGIS1" onclick="getDR()">
    <br>testing value: <div id = "test">testing</div>
    
    <table>
      <tr>
        <td >Time for 1 drop: </td>
        <td><div id='sendtime1'>%time1%</div></td>
      </tr><tr>
        <td>Time between 2 drop: </td>
        <td><div id='sendtime2'>%time2%</div></td>
      </tr><tr>
        <td>No. of drop: </td>
        <td><div id='sendno_of_drop'>%no_of_drop%</div></td>
      </tr><tr>
        <td>Total time: </td>
        <td><div id='sendtotal_time'>%total_time%</div></td>
      </tr>
      <tr>
        <td>Drip rate: </td>
        <td><div id='senddrop_rate'>%drop_rate%</div></td>
      </tr>
    </table>
    <br><br>
  
    
    <form action='/get' target='hidden-form'>
    AGIS 2 (current value %input2%): 
    <input name='input2' type='submit' value='Up' onclick='getValue()'> 
    <input name='input2' type='submit' value='Up_and_Down' onclick='getValue()'> 
    <input name='input2' type='submit' value='Down' onclick='getValue()'> 
    <input name='input2' type='submit' value='STOP' onclick='getValue()'> 
    </form><br>
    
    <form action='/get' target='hidden-form'> AGIS 3 (current value %input3%): 
    <input name='input3' type='submit' value='Up' onclick='getValue()'>
    <input name='input3' type='submit' value='Up_and_Down' onclick='getValue()'> 
    <input name='input3' type='submit' value='Down' onclick='getValue()'> 
    <input name='input3' type='submit' value='STOP' onclick='getValue()'> 
    </form><br>
    
    <iframe style="display:none" name="hidden-form"></iframe> 

    <div id='sendsensor'>testing value</div>
    <figure class="highcharts-figure">
      <div id="chart-reading" class="container">testing</div>
    </figure>

  </body>
  <script>
      var chartT = new Highcharts.Chart({
        chart:{ renderTo :'chart-reading', marginRight: 10},
        time: { useUTC: false },
        title: { text:'Sensor Reading' },
        plotOptions: {
          line: { animation: false,
            dataLabels: { enabled: false }
          }
        },
        accessibility: {
            announceNewData: {
                enabled: true,
                minAnnounceInterval: 15000,
                announcementFormatter: function (allSeries, newSeries, newPoint) {
                    if (newPoint) {
                        return 'New point added. Value: ' + newPoint.y;
                    }
                    return false;
                }
            }
        },
        xAxis: { 
          type:'datetime', tickPixelInterval: 150
        },
        yAxis: {
          title: { text:'Drop detected' }, 
          plotLines:[{ value: 0, width: 1, color:'#808080'}]
        },
        credits: { enabled: false },
        legend: { enabled: true },
        exporting: { enabled: false },
        series: [{
          name:'AGIS1',
          data: ( function(){
            var data = [], time = (new Date()).getTime(), i;
            for ( i=-19; i<=0; i+=1){
              data.push({ x: time + i * 1000,
                          y: 0
              });
            }
            return data;
          }())
        }]
      });
      var series = chartT.series[0];
      setInterval(function(){ plotGraph();}, 500);
      function plotGraph(){
       var xhttp = new XMLHttpRequest();
       xhttp.onreadystatechange = function() {
          var sensorreading = parseInt(this.responseText);
          var x = (new Date()).getTime(),
              y = sensorreading;
          chartT.series[0].addPoint([x, y], true, true);
          document.getElementById("sendsensor").innerHTML = sensorreading;
        };
        xhttp.open("GET", "sendsensor", true);
        xhttp.send();
      }
    </script>

</html>
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
  // TODO: drop_rate is not used in this function anymore
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
    return String(drop_rate);
  }
  return String();
}

hw_timer_t *Timer0_cfg = NULL; // create a pointer for timer0
hw_timer_t *Timer1_cfg = NULL; // create a pointer for timer1
hw_timer_t *Timer2_cfg = NULL; // create a pointer for timer2

void IRAM_ATTR DropSensor() { // timer0 interrupt, for sensor detected drops and
  // measure the time
  static int phase;
  static bool occur_state = false; // true when obstacle detected
  static int time_for_no_drop;
  if (phase == 0) {
    occur = digitalRead(SENSOR_PIN); // read the sensor value
    phase++;
  }

  if (phase == 1) {
    if (occur == 1) {
      time_for_no_drop = 0;
      no_drop_with_20s = false;
      if (!occur_state) { // check for the drop is just detected
        occur_state = true;
        next_time = millis();
        total_time += (next_time - start_time);
        no_of_drop++;
        // drop_rate = total_time / no_of_drop;
        int_time2 = next_time - start_time;
        time2 = String(int_time2) + "ms";
        start_time = millis();
        // if (total_time >= 15000) { // 15 seconds passed, delete old data
        //   total_time = 0;
        //   no_of_drop = 0;
        // }
      }
    }
    if (occur == 0) {
      time_for_no_drop++;
      if (occur_state) {
        leave_time = millis();
        occur_state = false;
        time1 = String(leave_time - start_time) + "ms";
      }
    }
    phase++;
  }
  if (phase == 2) {
    if(time_for_no_drop >= 20000){  // call when no drop appears within 20s, reset all data
      time1 = "no drop appears currently";
      time2 = "no drop appears currently";
      no_of_drop = 0;
      total_time = 0;
      no_drop_with_20s = true;
    }
    if(no_of_drop >= 500){
      volume_exceed = true;
    }
    phase = 0;
  }
  time_1ms++;         // count for 1ms
  print_state = true; // start printing
  phase_change_for_timer1 = true; // allow timer1 1 INT phase counting
}

void IRAM_ATTR AutoControl() { // timer1 interrupt, for auto control motor

    if ((no_drop_with_20s) || (time2=="not started")){
      drop_rate = 0;
      web_auto_state = 0;
    } else{
      drop_rate = 60000 / int_time2;
    }
    
    if ((web_auto_state > drop_rate) && (!but_state)){
      analogWrite(motorCTRL_1, (ADCValue / 16));
      analogWrite(motorCTRL_2, 0); 
    }
    if ((web_auto_state < drop_rate) && (!but_state)){
      analogWrite(motorCTRL_2, (ADCValue / 16));
      analogWrite(motorCTRL_1, 0);
    }
    if ((web_auto_state == drop_rate) && (!but_state)){
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
      // Motor_Off();
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
    request->send_P(200, "text/html", index_html, processor);
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
    request->send(200, "text/html", String(drop_rate));
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
      web_auto_state = inputMessage.toInt(); // convert the input from AGIS1 to integer,
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
  Serial.println(inputMessage);

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

  //   Serial.print("Web auto state is ");
  //   Serial.println(web_auto_state);

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

// check the condition of the auto control
// convert inputMessage(String) to integer
int check_t() {
  static int state;
  if (inputMessage == "moreup") {
    state = 2;
  } else if (inputMessage == "up") {
    state = 1;
  } else if (inputMessage == "moredown") {
    state = -2;
  } else if (inputMessage == "down") {
    state = -1;
  }  else if (inputMessage == "0") {
    state = 0;
  }
  return state;
}

void Motor_On_Up() {
  if (limitSwitch_Up.isPressed()) {
    // Serial.println("The up limit switch: UNTOUCHED -> TOUCHED");
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
    // Serial.println("The down limit switch: UNTOUCHED -> TOUCHED");
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