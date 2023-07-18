# AGIS-control-firmware

## Description
This is the firmware for AGIS control board.

## Keypad key functions:
+ number keys 0->9: numbers for input fields
+ `Ent`: confirm each input field
+ arrows UP and DOWN: motor move up or down
+ arrows Left and Right: navigate between input fields / bottons
+ `F1`: toggle between input screen and monitor screen
+ `F2`: start infusion after all input fields are confirmed
+ `Esc`: reset program, equivalent to reset button

## I2C address:
+ OLED: 0x3C
+ INA219: 0x40

## Wiring

- OLED & INA219 display wiring:

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

## IMPORTANT: make sure below conditions satisfy before making PR
+ Physical switch can control the motor with/without automatic control. i.e. highest priority
+ Automatic control can be issued from the website multiple times
+ Can download the log file after infusion has completed
+ Analyze the data from the log file to maintain drip rate accuracy
+ Can use keypad to fill in inputs and start automatic control
+ Display can show monitoring information during infusion

## Best practices when writing software:
+ Use `log` instead of `Serial.printf()`. Logging makes debugging a lot easier.

  Different level of verbosity can be set from this line in `platformio.ini` file:
  ```
  -D CORE_DEBUG_LEVEL=5  ; 1: Error, 2: Warning, 3: Info, 4: Debug, 5: Verbose
  ```
+ Group functions and variables related to a feature into the same source/header file

## Troubleshooting
Q: example issue

A: example solution
