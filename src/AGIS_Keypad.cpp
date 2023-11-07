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
}

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

bool keypadInfusionConfirmed = false;