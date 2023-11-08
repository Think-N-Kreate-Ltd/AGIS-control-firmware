#include <AGIS_Keypad.h>
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
  io.pinMode(LS_UP_PIN, INPUT);
  io.pinMode(LS_DOWN_PIN, INPUT);

  io.debounceTime(LS_DEBOUNCE_TIME);
  io.debouncePin(LS_UP_PIN);
  io.debouncePin(LS_DOWN_PIN);
}

// get the key if keypad is pressing, and NULL if not
char getKey() {
  uint16_t keyData = io.readKeypad();
  char key;
  if (keyData) {
    byte row = io.getRow(keyData);
    byte col = io.getCol(keyData);

    key = keyMap[row][col];
    Serial.print(key);
  } else {
    key = '\0';
    // Serial.println("there is no key pressed");
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

bool keypadInfusionConfirmed = false;