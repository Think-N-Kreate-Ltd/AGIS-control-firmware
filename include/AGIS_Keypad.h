#ifndef C22DF5A3_1D91_414B_92DD_A70F1FBB8647
#define C22DF5A3_1D91_414B_92DD_A70F1FBB8647

#include <Arduino.h>

#define KEYPAD_ROW_NUM     5
#define KEYPAD_COLUMN_NUM  4
#define SLEEP_TIME         256
#define SCAN_TIME          4
#define DEBOUNCE_TIME      2    // must less than scan time
#define LS_DEBOUNCE_TIME   40   // not important, as we just need the first `HIGH` signal

#define LS_UP_PIN          14
#define LS_DOWN_PIN        15

// extern SX1509 io;
// extern bool keypad_inputs_valid;

void sx1905SetUp();
char getKey();
bool getPinState(uint8_t);

#endif /* C22DF5A3_1D91_414B_92DD_A70F1FBB8647 */
