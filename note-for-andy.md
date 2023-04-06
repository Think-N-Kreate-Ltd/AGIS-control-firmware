# task working on
 - WiFi
    - ~~soft wifi AP (use other device to connect WiFi)~~
 - web page
    - press the button to control, release to stop 
    - ~~use 3 toggle SW~~
    - ~~use only one page~~
    - ~~higher priority of the button~~
    - ~~auto refresh some part(time) of the page~~
    - ~~auto refresh the value when any value change~~
    - better UI
 - IRDA sensor
    - ~~read the value of sensor~~
    - ~~measure the time~~
    - ~~display the value on web page~~
    - update data when drop detected
    - ~~plot the graph with real time~~
    - ~~update the sensor reading frequently and cancel old data~~
    - ***need another method to plot graph as the above method is not practical***
    - use SPI & SD card to store the data
 - display on OLED
    - ~~LCD display sensor value~~
    - use OLED to display sensor value
    - display two graph: current & drop count(later)
	  - INA219 (current sensor?)
	  - the a, b, c of the square wave
 - auto system
    - compare the drip rate
    - auto move
    - set an alarm on web 
    - set an alarm on the board
 - other/overall
    - ~~put all the thing in INT (nothing in amin loop)~~
    - everyone can accese the web page
    - OTA (sorry I forget what it is use for)
    - ~~test the motor~~

======================================
# note for the code
 - wifi ssid:AutoConnectAP, password:password.
 - GPIO2 -> reading sensor data (Change in L40)
 - timer0 -> read sensor & time measure
 - timer1 -> not used currently
 - timer2 -> control the motor
 - **don't use delay**

======================================
# learning/note
- 4x4 keypad matrix
- ~~use interrupt more (use phase change)~~
- any GPIO can do SPI, dont use colored in gray
- mostly will use 1*SPI, 1*UART, 1*I2C
- printf()
- soldering

======================================
# Resourses

ESP start-up in ide: 
 - https://dronebotworkshop.com/esp32-intro/

WiFiManager:
 - https://dronebotworkshop.com/wifimanager/

LCD:
 - https://lastminuteengineers.com/esp32-i2c-lcd-tutorial/

AsyncTCP-master & ESPAsyncWebServer-master: 
 - https://randomnerdtutorials.com/esp32-esp8266-input-data-html-form/
 - https://randomnerdtutorials.com/esp32-wi-fi-manager-asyncwebserver/
 - https://microcontrollerslab.com/esp32-asynchronous-web-server-espasyncwebserver-library/

add image by using SPIFFS: 
 - https://randomnerdtutorials.com/display-images-esp32-esp8266-web-server/

minify the html code: 
 - https://www.willpeavy.com/tools/minifier/

static: 
 - https://hackaday.com/2015/08/04/embed-with-elliot-the-static-keyword-you-dont-fully-understand/
 - a way to define var
 - only work in the function but will ont init everytimme call the function
 - other function cannot use that var

volatile
 - https://hackaday.com/2015/08/18/embed-with-elliot-the-volatile-keyword/
 - Memory-mapped Hardware Registers don't knot what it says

may use for button
 - https://www.w3schools.com/howto/howto_css_animate_buttons.asp
 - https://www.w3schools.com/howto/howto_css_switch.asp
 - https://linuxhint.com/toggle-button-javascript/

plot graph by javascript:
 - https://www.highcharts.com/demo

I2C: 
 - https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/api-reference/peripherals/i2c.html
 - https://esp32developer.com/programming-in-c-c/i2c-programming-in-c-c/our-i2c-master-universal-code

OLED: 
 - https://www.electronicshub.org/esp32-oled-display/

======================================
# web page code
<!DOCTYPE HTML>
<html>

  <head>
    <title>ESP Input Form</title>
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <style>
      .switch {position: relative; display: inline-block; width: 120px; height: 68px} 
      .switch input {display: none}
      .slider {position: absolute; top: 0; left: 0; right: 0; bottom: 0; background-color: #ccc; border-radius: 6px}
      .slider:before {position: absolute; content: ""; height: 52px; width: 52px; left: 8px; bottom: 8px; background-color: #fff; -webkit-transition: .4s; transition: .4s; border-radius: 3px}
      input:checked+.slider {background-color: #b30000}
      input:checked+.slider:before {-webkit-transform: translateX(52px); -ms-transform: translateX(52px); transform: translateX(52px)}
    </style>
    <script src="https://code.highcharts.com/highcharts.js">
      var chartT = new Highcharts.Chart({
         chart:{ renderTo : 'chart-reading' },
         title: { text: 'Sensor Reading' },
         series: [{
            showInLegend: false,
            data: []
         }],
         plotOptions: {
            line: { animation: false,
               dataLabels: { enabled: true }
            }
         },
         xAxis: { type: 'datetime',
            dateTimeLabelFormats: { second: '%H:%M:%S' }
         },
         yAxis: {
            title: { text: 'Reading' }
         },
         credits: { enabled: false }
      });
      setInterval(function(){ plotGraph();refreshtime1(); refreshtime2(); refreshno_of_drop(); refreshtotal_time();}, 3000);
      function plotGraph(){
         var xhttp = new XMLHttpRequest();
         xhttp.onreadystatechange = function() {
            var x = (new Date()).getTime(),
               y = parseFloat(this.responseText);
            if(chartT.series[0].data.length > 40) {
               chartT.series[0].addPoint([x, y], true, true, true);
            } else {
               chartT.series[0].addPoint([x, y], true, false, true);
            }
        };
        xhttp.open("GET", "sendsensor", true);
        xhttp.send();
      }
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
        };
        xhttp.open("GET", "sendtime2", true);
        xhttp.send();
      }
      function refreshno_of_drop(){
        var xhttp = new XMLHttpRequest();
        xhttp.onreadystatechange = function() {
            document.getElementById("sendno_of_drop").innerHTML = this.responseText;
        };
        xhttp.open("GET", "sendno_of_drop", true);
        xhttp.send();
      }
      function refreshtotal_time(){
        var xhttp = new XMLHttpRequest();
        xhttp.onreadystatechange = function() {
            document.getElementById("sendtotal_time").innerHTML = this.responseText;
        };
        xhttp.open("GET", "sendtotal_time", true);
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
    <table>
      <tr>
        <td >Time for 1 drop: </td>
        <td><div id='sendtime1'>%time1%ms</div></td>
      </tr><tr>
        <td>Time between 2 drop: </td>
        <td><div id='sendtime2'>%time2%ms</div></td>
      </tr><tr>
        <td>No. of drop: </td>
        <td><div id='sendno_of_drop'>%no_of_drop%</div></td>
      </tr><tr>
        <td>Total time: </td>
        <td><div id='sendtotal_time'>%total_time%ms</div></td>
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

    <div id="chart-reading" class="container"></div>

  </body>

</html>

===========================
# result for test_i2c
ESP-ROM:esp32s3-20210327
Build:Mar 27 2021
rst:0x1 (POWERON),boot:0x8 (SPI_FAST_FLASH_BOOT)
SPIWP:0xee
mode:DIO, clock div:1
load:0x3fce3808,len:0x44c
load:0x403c9700,len:0xbe4
load:0x403cc700,len:0x2a38
entry 0x403c98d4

I2C Scanner
Scanning...
I2C device found at address 0x27
done