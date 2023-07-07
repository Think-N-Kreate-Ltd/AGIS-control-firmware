# AGIS-control-firmware

## Description
This is the firmware for AGIS control board.

## Keypad key functions:
+ number keys 0->9: numbers for input fields
+ `Ent`: confirm each input field
+ arrows UP and DOWN: navigate between input fields
+ `F2`: start infusion after all input fields are confirmed
+ `Esc`: reset program, equivalent to reset button

## Wiring

- TFT display wiring:

| **TFT pin** |                  **ESP32 devkit pin**                  |
|:-----------:|:------------------------------------------------------:|
|     LED     | 3V3 (can change to GPIO pin to allow toggle backlight) |
|     SCK     |                           12                           |
|     MOSI    |                           11                           |
|      DC     |                           13                           |
|    RESET    |                           14                           |
|      CS     |                           10                           |
|     GND     |                           GND                          |
|     VCC     |                           3V3                          |

- OLED display wiring:

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
