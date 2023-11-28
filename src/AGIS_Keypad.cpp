#include <AGIS_Keypad.h>
#include <Wire.h>
#include <SparkFunSX1509.h>
#include <AGIS_Commons.h>

SX1509 io;

/*Keypad variables*/
// U: Up, D: Down, L: Left, R: Right, C: Cancel, E: Enter
char keyMap[KEYPAD_ROW_NUM][KEYPAD_COLUMN_NUM] = {
  {'F', 'G', '#', '*'},
  {'1', '2', '3', 'U'},
  {'4', '5', '6', 'D'},
  {'7', '8', '9', 'C'},
  {'L', '0', 'R', 'E'}
};

void sx1905SetUp() {
  // I2C have already init-ed in INA219
  if (io.begin(0x3E) == false) {
    ESP_LOGE(KEYPAD_TAG, "Fail to find SX1905");
    return;
  }

  // setting on keypad
  io.keypad(KEYPAD_ROW_NUM, KEYPAD_COLUMN_NUM, SLEEP_TIME, SCAN_TIME, DEBOUNCE_TIME);

  // setting on limited SW
  // io.pinMode(LS_UP_PIN, INPUT);
  // io.pinMode(LS_DOWN_PIN, INPUT);

  // io.debounceTime(LS_DEBOUNCE_TIME);
  // io.debouncePin(LS_UP_PIN);
  // io.debouncePin(LS_DOWN_PIN);
}

// get the key if keypad is pressing, and NULL if not
char getKey() {
  uint16_t keyData = io.readKeyData();  // get raw data
  char key; // var to store the key, use to return (can remove)
  static char lastKey;    // use for error checking
  static bool pressState; // use for error checking
  if (keyData) {  // in fact, this is a double check, not in need
    // decode the raw data by ourselves, to reduce the time spend
    uint8_t rowRaw = keyData % 256;
    uint8_t row = 0;
    while (rowRaw >= 2) { // same as !=1
      rowRaw = rowRaw >> 1;
      ++row;
    }
    uint8_t colRaw = (keyData / 256);
    uint8_t col = 0;
    while (colRaw >= 2) { // same as !=1
      colRaw = colRaw >> 1;
      ++col;
    }

    // get the key from matrix (2 dimensional array)
    key = keyMap[row][col];

    // as there are still lots of noise, we should do the error check by ourselves
    // in fact, we just need the first return value is accurate
    if (key != lastKey) {
      lastKey = key;
      // we want the first key be null to prevent error
      // we also want don't want the error key set to null, as it will stop keypad state
      if (!pressState) {
        key = '\0';
      } else {
        key = lastKey;
      }
      Serial.println("skip");
    } else {
      // only count as pressing when get the correct key
      pressState = true;
    }
    Serial.print(key);
  } else {
    if (lastKey == '\0') { // to reset once only
      key = '\0';
      pressState = false;
    }
    lastKey = '\0'; // as there is no key pressed last time
  }
  return key;
}

// get the state (HIGH/LOW) for input pin in SX1905
// without debouncing currently 
// as motor should stop if touched, so there is no meaning for doing debouncing
bool getPinState(uint8_t pinNum) {
  if (io.digitalRead(pinNum) == HIGH) {
    return true;
  } else {
    return false;
  }
}