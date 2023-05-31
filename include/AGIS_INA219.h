#ifndef AGIS_INA219_H
#define AGIS_INA219_H

#include <Wire.h>
#include <Adafruit_INA219.h>
// #include <LiquidCrystal_I2C.h>

#define I2C_SCL 41
#define I2C_SDA 40

Adafruit_INA219 ina219;

void ina219SetUp();
void lcdDisplay(void * arg);

#endif