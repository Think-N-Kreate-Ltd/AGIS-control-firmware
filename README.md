# AGIS-control-firmware

## Description
- This is the firmware for AGIS control board.
- wifi ssid:AutoConnectAP, password:password.
- go to http://<IPAddress>/update for OTA update
- upload firmware.bin for main, spiffs.bin for SPIFFS files
- GPIO36 -> EXT interrupt for reading sensor data
- timer0 -> INT interrupt for auto control
- timer1 -> INT interrupt for motor control by keypad

## Keypad key functions:
+ number keys 0->9: numbers for input fields
+ `#`: backspace for input
+ `Ent`: confirm each input field
+ arrows UP and DOWN: motor move up or down
+ arrows Left and Right: navigate between input fields / bottons
+ `*`: pause / resume / complusory complete / restart
+ `F1`: toggle between input screen and monitor screen
+ `F2`: start infusion after all input fields are confirmed
+ `Esc`: reset program, equivalent to reset button

## I2C address:
+ OLED: 0x3C
+ INA219: 0x40

## Wiring
- I2C wiring,
- including OLED display & INA219:

| **OLED pin** | **ESP32 devkit pin** |
|:------------:|:--------------------:|
|      VCC     |          3V3         |
|      GND     |          GND         |
|      SCL     |          41          |
|      SDA     |          40          |

- Keypad wiring:

| **Keypad pin** | **ESP32 devkit pin**                                 |
|:--------------:|:----------------------------------------------------:|
|        1       |           5                                          |
|        2       |           6                                          |
|        3       |           7                                          |
|        4       |          17                                          |
|        5       |          18                                          |
|        6       |           1                                          |
|        7       |           2                                          |
|        8       |          47 (remove 0 Ohm resistor)                  |
|        9       |          48 (remove 0 Ohm resistor and flash)        |

| **SPI pin** |                  **ESP32 devkit pin**                  |
|:-----------:|:------------------------------------------------------:|
|     MISO    |                           21                           |
|     SCK     |                           12                           |
|     MOSI    |                           11                           |
|    TFT CS   |                           10                           |
|    SD CS    |                            9                           |
|      DC     |                           13                           |
|    RESET    |                           14                           |
|     LED     | 3V3 (can change to GPIO pin to allow toggle backlight) |

## IDE setup
+ [Visual Studio Code](https://code.visualstudio.com/)
+ PlatformIO extension in VS Code

## How to build
+ if using OTA, should use keypad to enable wifi first
+ go to http://`IPAddress`/update for OTA update
+ build to get the .bin file and upload by OTA

## IMPORTANT: make sure below conditions satisfy before making PR <- OUTDATED
+ Physical switch can control the motor with/without automatic control. i.e. highest priority
+ Automatic control can be issued from the website multiple times
+ Can download the log file after infusion has completed
+ Analyze the data from the log file to maintain drip rate accuracy
+ Can use keypad to fill in inputs and start automatic control
+ Display can show monitoring information during infusion

## UPDATE on the above important
As there are too many things should work with now, there are too many conditions should satisfy right now. However, it will spend too much time for testing. Thus, only satisfy conditions which related to the changed code is fine. See below for details.
- INA219:
  - check the log file, can get all data expected
- SD logging:
  - connect to serial monitor, see logging initialized & data logging done
  - make sure error on file.open have not appear
  - pause and resume the infusion once while auto controlling
  - make sure the log file can get all expected data, including paused and resume
  - can download log file (don't need to test, cause that function should not be touched)
- Auto control / motor control / drop sensor:
  - remove the sensor for 20s, ensure the state go to stopped and have not reset values
  - can pause / resume / complete the infusion by `*`
  - can control the motor when infusion paused (also before start, but don't need to test it)
  - can reset all values and state go to not started after no drop 20s (not including the case when doing auto control and completed by `*`)
  - after that, it can run the second time
  - ensure it can go to all state
- Enable wifi:
  - connect to serial monitor, see homing completed & old directory removed
  - can open the webpage
- TFT display:
  - can switch the screen
  - can move the motor before start auto control
  - able the start the infusion, which include
    1. show target Drip rate on msgbox
    2. directly go to monitor screen after press `Yes`
    3. the motor start move
  - can switch back to input screen while auto controlling
  - monitor screen can update data before auto controlling
- Other little works:
  - the LED on sensor can blink / turn on, also in infusion
  - start the infusion and the state can change to in progress

Also, remember to test the newly added function

## Best practices when writing software:
+ `log` for long term debugging will be better, while `print` is better on finding reason of unexpected result
+ Try to use `static` more. Instead of using global var, it is more clear, and is an effective way to reduce the var name size
+ Group the vars and put them in a suitable place. Also, add some description on vars and functions plz!
+ Don't play LVGL before understanding how to use it
+ ONLY update the state whenever it changes (don't frequently update). Otherwise, it is hard to find out the reason when unexpected result occur

## Warning and modified library
+ `FS.h` -> Rename `FILE_READ` to `FS_FILE_READ` ...etc to solve the redefine warning
+ `AsyncElegantOTA.h` -> L4 commented, that warning is to remind not to use `.loop()` only
+ `WiFiManger` -> No Need, just keep it
  - That warning is because S3 cannot use temp sensor before
  - In idf can enable temp sensor. However, in ide, it is just a hardcode warning
  - Can comment that line but i think the next update version of the library will remove it.
+ the below modify is not that important and usually, it will not be seen, can just ignore them
  - `lv_obj.c` -> add `const` on L143 <- not fixed
  - `lv_tabview.c` & `lv_checkbox` -> also about const, give up fixing it

## Troubleshooting
Q: example issue

A: example solution
