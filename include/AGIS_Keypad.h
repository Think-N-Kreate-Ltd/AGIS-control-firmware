#ifndef C22DF5A3_1D91_414B_92DD_A70F1FBB8647
#define C22DF5A3_1D91_414B_92DD_A70F1FBB8647

#include <Arduino.h>
#include <Wire.h>
#include <SparkFunSX1509.h>

#define KEYPAD_ROW_NUM     5
#define KEYPAD_COLUMN_NUM  4
#define SLEEP_TIME         256
#define SCAN_TIME          2
#define DEBOUNCE_TIME      1    // must less than scan time

extern SX1509 io;
// extern bool keypad_inputs_valid;

void sx1905SetUp();
char getKey();

#endif /* C22DF5A3_1D91_414B_92DD_A70F1FBB8647 */
