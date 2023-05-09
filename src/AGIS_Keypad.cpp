#include <AGIS_Keypad.h>

#define KEYPAD_ROW_NUM     5
#define KEYPAD_COLUMN_NUM  4

/*Keypad variables*/
// U: Up, D: Down, L: Left, R: Right, C: Cancel, E: Enter
char keys[KEYPAD_ROW_NUM][KEYPAD_COLUMN_NUM] = {
  {'F', 'G', '#', '*'},
  {'1', '2', '3', 'U'},
  {'4', '5', '6', 'D'},
  {'7', '8', '9', 'C'},
  {'L', '0', 'R', 'E'}
};
byte pin_rows[KEYPAD_ROW_NUM] = {8, 18, 17, 16, 15};  // pin for R1, R2, R3, R4, R5
byte pin_column[KEYPAD_COLUMN_NUM] = {4, 5, 6, 7};    // pin for C1, C2, C3, C4
Keypad keypad = Keypad( makeKeymap(keys), pin_rows, pin_column, KEYPAD_ROW_NUM, KEYPAD_COLUMN_NUM );