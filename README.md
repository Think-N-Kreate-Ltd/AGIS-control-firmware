# AGIS-control-firmware

## Description
This is the firmware for AGIS control board.

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

- Keypad wiring:

| **Keypad pin** | **ESP32 devkit pin** |
|:--------------:|:--------------------:|
|        1       |           5          |
|        2       |           6          |
|        3       |           7          |
|        4       |          17          |
|        5       |          18          |
|        6       |           1          |
|        7       |           2          |
|        8       |          47          |
|        9       |          48          |


## IDE setup
+ [Visual Studio Code](https://code.visualstudio.com/)
+ PlatformIO extension in VS Code

## How to build

## IMPORTANT: make sure below conditions satisfy before making PR
+ Physical switch can control the motor with/without automatic control. i.e. highest priority
+ Automatic control can be issued from the website multiple times
+ Can download the log file after infusion has completed
+ Analyze the data from the log file to maintain drip rate accuracy

## Troubleshooting
Q: example issue

A: example solution
