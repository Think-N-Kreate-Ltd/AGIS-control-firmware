#ifndef AGIS_INA219_H
#define AGIS_INA219_H

#include <Wire.h>
#include <Adafruit_INA219.h>
// #include <LiquidCrystal_I2C.h>

#define I2C_SCL 41
#define I2C_SDA 40

// Adafruit_INA219 ina219;

extern volatile float current_mA;
extern volatile float busvoltage;
extern volatile float shuntvoltage;
extern volatile float power_mW;
extern volatile float avgCurrent_mA;

void ina219SetUp();
void getIna219Data();

#endif  // AGIS_INA219_H