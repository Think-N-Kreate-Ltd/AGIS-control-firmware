# modified library

1. FS.h -> rename define `FILE_READ` to `FS_FILE_READ` ...etc
2. AsyncElegantOTA.h -> L4 commented, remember not to use `.loop()`
3. WiFiManger -> not yet
4. lv_obj.c -> add `const` on L143
5. lv_tabview.c & lv_checkbox -> also about const, give up fixing it

=======================================================
# things to work on

 - main:
     - move all function protype to header file
     - ~~check and test for `no drop 20s`~~
     - ~~reset value for tft after finish once and pass 20s, or use a button to do~~
     - ~~enter state = exceeded~~
     - ~~remove volume when drop factor is not known~~
     - ~~get the accurate volume~~
     - solve the problem about cannot start auto-con when state=started
     - ~~solve the problem about can only start auto-con when last value get is DF~~
     - ~~find the reason why need 100s to init <= is beacuse the time is using counting~~
     - ~~use calculation to measure 20s~~
     - try to reset and connect to wifi
 - lvgl:
     - background color for comfirm msgbox
     - ~~monitor screen update data~~
     - when run by web, tft display switch screen to monitor screen
     - ~~signal error on keypad~~
     - ~~wifibox timeout close problem~~
     - when click `F2` but not input filled in, do sth?
     - place the wifibox text in center
     - ~~cannot press the `F2` when msgbox is here~~
     - ~~`*` do the enter? or pause the infusion <- both done~~
     - ~~can run bt tft more than 1 times~~
 - INA219
     - ~~check the value(with motor)~~, seems strange sometimes
 - other (warning)
     - ~~Bus already started in Master Mode~~
     - WM_NOTEMP <- cannot remove it, as it is hardcoded in the code

=======================================================
# LVGI note

~~To handle the tasks of LVGL you need to call lv_timer_handler() periodically in one of the following:~~

 - while(1) of main() function
 - timer interrupt periodically (lower priority than lv_tick_inc())
 - an OS task periodically

~~The timing is not critical but it should be about 5 milliseconds to keep the system responsive.~~
comes from: https://docs.lvgl.io/8/porting/task-handler.html
=======================================================

If you need to use real tasks or threads, you need a mutex which should be invoked before the call of lv_timer_handler() and released after it. Also, you have to use the same mutex in other tasks and threads around every LVGL (lv_...) related function call and code. This way you can use LVGL in a real multitasking environment. Just make use of a mutex to avoid the concurrent calling of LVGL functions.

Try to avoid calling LVGL functions from interrupt handlers (except lv_tick_inc() and lv_disp_flush_ready()). But if you need to do this you have to disable the interrupt which uses LVGL functions while lv_timer_handler() is running.

It's a better approach to simply set a flag or some value in the interrupt, and periodically check it in an LVGL timer (which is run by lv_timer_handler()).

https://docs.lvgl.io/master/porting/os.html#tasks-and-threads
https://docs.lvgl.io/master/overview/event.html#events

set up drivers
https://docs.lvgl.io/master/get-started/platforms/arduino.html

start up
this is a outdated version but provide better information
https://daumemo.com/how-to-use-lvgl-library-on-arduino-with-an-esp-32-and-spi-lcd/
this is the new version(v8.3) example
https://github.com/lvgl/lvgl/blob/release/v8.3/examples/arduino/LVGL_Arduino/LVGL_Arduino.ino
this is coding guide
https://docs.lvgl.io/master/CODING_STYLE.html#naming-conventions

example with using grid(span)
https://github.com/lvgl/lvgl/blob/c4d91ca1bc70bcaedbaf034177f51c8d1a5df026/examples/layouts/grid/lv_example_grid_2.c

=======================================================
# Sharing the SPI bus among SD card and other SPI devices

https://github.com/espressif/esp-idf/blob/5cc4bceb2a46b5f29e7b867150bdc7288f77b8bf/docs/en/api-reference/peripherals/sdspi_share.rst

Sharing the SPI bus among SD card and other SPI devices
=======================================================
When it comes to use SD cards on MCU SD card slots, my five cents:

 - some SD cards, esp. newer, want to run in lower voltage mode: The SD card driver will query the card and figure out its type, e.g. SDHC vs. SDXC. If your FW cannot adjust the power voltage for SD card - you might be limited to which card is working
 - I have seen also issues, when my SD Card driver and FatFS cannot see the card: often, this happens on cards with large capacities, e.g. 512 GB or even 1 TB. Try to use a smaller capacity, e.g. 4 or 8 GB. I think, due to 32bit limitation, e.g. in FAT32, an SD card larger as 32 GB will never work. It would need a different file system (NTFS, eFAT) which might not be supported by your driver.
 - I have seen also issues where one type of card was working but not another one: lowering the SD Card clock helped here to make it working. Check with which speed you run the SD Card peripheral.
 - the other mentioned topic as "power consumption" can be true: if the power supply for your board is weak and almost all power is needed just to run the MCU additional current drawn by a "power hungry" SD Card can generate trouble also for the MCU (e.g. power drops, "brown-outs"). Even I have not realized such a dramatic effect - but a powerful power-supply for entire system makes sense

Most of the time is the SD Card device speed, the capacity of the SD card (too large) or even the type (e.g. a newer one just operating with a lower voltage on adapter).

=======================================================

The SD card has a SPI mode, which allows it to be communicated to as a SPI device. But there are some restrictions that we need to pay attention to.

Pin loading of other devices
----------------------------

When adding more devices onto the same bus, the overall pin loading increases. The loading consists of AC loading (pin capacitor) and DC loading (pull-ups).

AC loading
^^^^^^^^^^

SD cards, which are designed for high-speed communications, have small pin capacitors (AC loading) to work until 50MHz. However, the other attached devices will increase the pin's AC loading.

Heavy AC loading of a pin may prevent the pin from being toggled quickly. By using an oscilloscope, you will see the edges of the pin become smoother and not ideal any more (the gradient of the edge is smaller). The setup timing requirements of an SD card may be violoated when the card is connected to such bus. Even worse, the clock from the host may not be recognized by the SD card and other SPI devices on the same bus.

This issue may be more obvious if other attached devices are not designed to work at the same frequency as the SD card, because they may have larger pin capacitors.

To see if your pin AC loading is too heavy, you can try the following tests:

(Terminology: **launch edge**: at which clock edge the data start to toggle; **latch edge**: at which clock edge the data is supposed to be sampled by the receiver, for SD cad, it's the rising edge.)

1. Use an oscilloscope to see the clock and compare the data line to the clock. 
   - If you see the clock is not fast enough (for example, the rising/falling edge is longer than 1/4 of the clock cycle), it means the clock is skewed too much.
   - If you see the data line unstable before the latch edge of the clock, it means the load of the data line is too large.

   You may also observed the corresponding phenomenon (data delayed largely from launching edge of clock) with logic analyzers. But it's not as obvious as with an oscilloscope.

2. Try to use slower clock frequency.

   If the lower frequency can work while the higher frequency can't, it's an indication of the AC loading on the pins is too large.

If the AC loading of the pins is too large, you can either use other faster devices (with lower pin load) or slow down the clock speed.

DC loading
^^^^^^^^^^

The pull-ups required by SD cards are usually around 10 kOhm to 50 kOhm, which may be too strong for some other SPI devices. 

Check the specification of your device about its DC output current , it should be larger than 700uA, otherwise the device output may not be read correctly.

Initialization sequence
-----------------------

.. note::

  If you see any problem in the following steps, please make sure the timing is correct first. You can try to slow down the clock speed (SDMMC_FREQ_PROBING = 400 KHz for SD card) to avoid the influence of pin AC loading (see above section).

When using ab SD card with other SPI devices on the same SPI bus, due to the restrictions of the SD card startup flow, the following initialization sequence should be followed: (See also :example:`storage/sd_card`)

1. Initialize the SPI bus properly by `spi_bus_initialize`.

2. Tie the CS lines of all other devices than the SD card to high. This is to avoid conflicts to the SD card in the following step.

   You can do this by either:

   1. Attach devices to the SPI bus by calling `spi_bus_add_device`. This function will initialize the GPIO that is used as CS to the idle level: high.

   2. Initialize GPIO on the CS pin that needs to be tied up before actually adding a new device.

   3. Rely on the internal/external pull-up (not recommended) to pull-up all the CS pins when the GPIOs of ESP are not initialized yet. You need to check carefull the pull-up is strong enough and there are no other pull-downs that will influence the pull-up (For example, internal pull-down should be enabled).

3. Mount the card to the filesystem by calling `esp_vfs_fat_sdspi_mount`. 

   This step will put the SD card into the SPI mode, which SHOULD be done before all other SPI communications on the same bus. Otherwise the card will stay in the SD mode, in which mode it may randomly respond to any SPI communications on the bus, even when its CS line is not addressed. 

   If you want to test this behavior, please also note that, once the card is put into SPI mode, it will not return to SD mode before next power cycle, i.e. powered down and powered up again.

4. Now you can talk to other SPI devices freely!

======================================
# improvement working on
 - ~~change SPIFFS to LittleFs~~
 - ~~can try SD card~~ ~~(maybe FAT)~~ <- ~~trying mySD.h~~
    - update:
    - SanDisk 32GB cannot use SD.h
      - 8/16GB would not produce any more
    - use SdFat, but ESPAsyncWebServer don't support
    - add function to download file
    - **FINISHED**
 - buzzer test
 - everyone can accese the web page(?)
 - ~~check for first aid kit item(?) <- there is nothing~~
 - ~~INA219~~
 - ~~add INA219 and SdFat together~~
    - ~~sometimes WD triggered after download file~~
# Keypad and TFT
 - ~~keypad on timer INT~~

======================================
# the below task is outdated
 - button can not to change, just use toggle switch as they are not in used
 - don't need to polt graph
 - SD card connecting with ESP32-S3 have problem
 - alarm function is waiting for buzzer and test only
 - seems no ppl remember "everyone can accese the web page"

======================================
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
    - ~~refresh data when drop detected~~
    - ~~plot the graph with real time~~
    - ~~update the sensor reading frequently and cancel old data~~
    - ~~keep counting no of drop~~
    - ***need another method to plot graph as the above method is not practical***
    - use SPI & SD card to store the data
 - display on OLED
    - ~~LCD display sensor value~~
    - ~~use OLED to display sensor value~~
    - display two graph: current & drop count(later)
	  - INA219 (current sensor?)
	  - ~~the a, b, c of the square wave~~
 - auto system
    - ~~calculate target drip rate~~
    - ~~compare the drip rate~~
    - ~~add a field and the user input the drip he want~~
    - ~~auto move the motor~~
    - ~~it just run non-stop~~
    - ~~set an alarm on web ~~
    - set an alarm on the board
 - other/overall
    - ~~put all the thing in INT (nothing in amin loop)~~
    - everyone can accese the web page
    - ~~OTA for update~~
    - ~~test the motor~~

======================================
# note for the code
 - wifi ssid:AutoConnectAP, password:password
 - go to http://<IPAddress>/update for OTA update
 - upload firmware.bin for main, spiffs.bin for SPIFFS files
 - GPIO36 -> EXT interrupt for reading sensor data
 - timer0 -> INT interrupt for read sensor & time measure
 - timer1 -> INT interrupt for auto control
 - timer3 -> INT interrupt for control the motor

======================================
# learning/note
- 4x4 keypad matrix
- ~~use interrupt more (use phase change)~~
- any GPIO can do SPI, dont use colored in gray
- mostly will use 1xSPI, 1xUART, 1xI2C
- ~~printf()~~
- soldering

# Meaning of markers in VSC
- A - Added (This is a new file that has been added to the repository)
- M - Modified (An existing file has been changed)
- D - Deleted (a file has been deleted)
- U - Untracked (The file is new or has been changed but has not been added to the repository yet)
- C - Conflict (There is a conflict in the file)
- R - Renamed (The file has been renamed)
- S - Submodule (In repository exists another subrepository)

# Meaning of strftime
- %a Abbreviated weekday name 
- %A Full weekday name 
- %b Abbreviated month name 
- %B Full month name 
- %c Date and time representation for your locale 
- %d Day of month as a decimal number (01-31) 
- %H Hour in 24-hour format (00-23) 
- %I Hour in 12-hour format (01-12) 
- %j Day of year as decimal number (001-366) 
- %m Month as decimal number (01-12) 
- %M Minute as decimal number (00-59) 
- %p Current locale’s A.M./P.M. indicator for 12-hour clock 
- %S Second as decimal number (00-59) 
- %U Week of year as decimal number,  Sunday as first day of week (00-51) 
- %w Weekday as decimal number (0-6; Sunday is 0) 
- %W Week of year as decimal number, Monday as first day of week (00-51) 
- %x Date representation for current locale 
- %X Time representation for current locale 
- %y Year without century, as decimal number (00-99) 
- %Y Year with century, as decimal number 
- %z %Z Time-zone name or abbreviation, (no characters if time zone is unknown) 
- %% Percent sign 

# meaning of format specifiers
- d Signed decimal integer	392
- i	Signed decimal integer	392
- u	Unsigned decimal integer	7235
- o	Unsigned octal	610
- x	Unsigned hexadecimal integer	7fa
- X	Unsigned hexadecimal integer (uppercase)	7FA
- f	Decimal floating point, lowercase	392.65
- F	Decimal floating point, uppercase	392.65
- e	Scientific notation (mantissa/exponent), lowercase	3.9265e+2
- E	Scientific notation (mantissa/exponent), uppercase	3.9265E+2
- g	Use the shortest representation: %e or %f	392.65
- G	Use the shortest representation: %E or %F	392.65
- a	Hexadecimal floating point, lowercase	-0xc.90fep-2
- A	Hexadecimal floating point, uppercase	-0XC.90FEP-2
- c	Character	a
- s	String of characters	sample
- p	Pointer address	b8000000
- n	Nothing printed.
- The corresponding argument must be a pointer to a signed int.
- The number of characters written so far is stored in the pointed location.	
- %	A % followed by another % character will write a single % to the stream.	%

# combine char*
const char * weekday = "Thu";
char * time = "120000";
char combined_char[99];

strcpy(combined_char, weekday);
strcat(combined_char, ": ");
strcat(combined_char, time);

result: combined_char = Thu: 120000

======================================
# Resourses

ESP-IDF Programming Guide:
 - https://docs.espressif.com/projects/esp-idf/en/v5.0.2/esp32s3/api-reference/peripherals/clk_tree.html

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
 - https://www.base64-image.de/

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

struct
 - https://stackoverflow.com/questions/330793/how-to-initialize-a-struct-in-accordance-with-c-programming-language-standards
 - use for conveniently change value for many vars at the same time

pointer and array
 - https://www.w3schools.com/c/c_pointers_arrays.php

array size and length
 - https://stackoverflow.com/questions/37538/how-do-i-determine-the-size-of-my-array-in-c

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

OTA:
 - https://randomnerdtutorials.com/esp32-ota-over-the-air-vs-code/

LVGL:
 - https://pcbartists.com/firmware/notes-using-lvgl-with-esp32/

SD card logo and meaning
 - https://www.the-digital-picture.com/News/News-Post.aspx?News=30207&Title=What-do-the-Numbers-and-Symbols-on-SD-SDHC-and-SDXC-Memory-Cards-mean

FS delete files in folder
 - https://gist.github.com/jenschr/5713c927c3fb8663d662

SdFat web server download file
 - https://github.com/me-no-dev/ESPAsyncWebServer/issues/124

Real time web application
 - https://codeburst.io/polling-vs-sse-vs-websocket-how-to-choose-the-right-one-1859e4e13bd9

naming for git:
 - https://dev.to/varbsan/a-simplified-convention-for-naming-branches-and-commits-in-git-il4
 - https://dev.to/couchcamote/git-branching-name-convention-cch

======================================
# web page code
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
      var state_change = false;
    	var state_auto = false;
      var DR = 0;
      var drop_rate = 30;
      function getDR(){
        DR = parseInt(document.getElementById("AGIS1").value);
        state_auto = true;
        alert("clicked");
      }
      setInterval(function(){stateControl(); autoControl();}, 10000);
      function stateControl(){
      	state_change = true;
      }
      function autoControl(){
        var xhttp = new XMLHttpRequest();
        xhttp.onreadystatechange = function() {
          if((state_auto) && (state_change)){
          	if(DR < drop_rate){
          	  drop_rate -= 2;
          	}
          	if(DR < (drop_rate - 5)){
          	  drop_rate -= 2;
          	}
          	if(DR > drop_rate){
          	  drop_rate += 2;
          	}
          	if(DR > (drop_rate + 5)){
          	  drop_rate += 2;
          	}
            state_change = false;
          }
          document.getElementById("senddrop_rate").innerHTML = drop_rate;
        };
        xhttp.open("GET", "a", true);
        xhttp.send();
      }
      setInterval(function(){ refreshtime1(); refreshtime2(); refreshno_of_drop(); refreshtotal_time(); refreshdrop_rate();}, 3000);
      var tot_time = 0;
      var no_drop = 99;
      var drop_rate = tot_time / no_drop;
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
            no_drop = parseInt(this.responseText);
        };
        xhttp.open("GET", "sendno_of_drop", true);
        xhttp.send();
      }
      function refreshtotal_time(){
        var xhttp = new XMLHttpRequest();
        xhttp.onreadystatechange = function() {
            document.getElementById("sendtotal_time").innerHTML = this.responseText;
            tot_time = parseInt(this.responseText) / 1000;
        };
        xhttp.open("GET", "sendtotal_time", true);
        xhttp.send();
      }
      function refreshdrop_rate(){
        var xhttp = new XMLHttpRequest();
        xhttp.onreadystatechange = function() {
            document.getElementById("senddrop_rate").innerHTML = no_drop * 60 / tot_time;
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
      <tr>
        <td>Drip rate: </td>
        <td><div id='senddrop_rate'>drop_rate</div></td>
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

===========================
# result for test_i2c
 - the address of I2C can be found by the code provided in the example
 - if there is no 2 things using the same, better not to change it

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
**I2C device found at address 0x27**
done