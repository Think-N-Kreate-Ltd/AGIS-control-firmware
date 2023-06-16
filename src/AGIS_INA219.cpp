#include <AGIS_INA219.h>
#include <AGIS_Commons.h>
#include <AGIS_Utilities.h>

Adafruit_INA219 ina219;

volatile float current_mA;
volatile float busvoltage;
volatile float shuntvoltage;
volatile float power_mW;
volatile float avgCurrent_mA;

void ina219SetUp() {
  // ina219.begin will also set up the I2C
  // thus, use setPins instead of begin and avoid setup twice
  // don't know why, it also solve the problem of WD triggered
  Wire.setPins(I2C_SDA, I2C_SCL);
  while (!Serial) {
      // will pause Zero, Leonardo, etc until serial console opens
      delay(1);
  }

  // Wire.beginTransmission(64);

  if (!ina219.begin(&Wire)) {
    Serial.println("Failed to find INA219 chip");
    while (1) { delay(10); }
  }

  // Serial.println("INA219 connected");
}

// LiquidCrystal_I2C lcd(0x27,16,2);  // set the LCD address to 0x27 for a 16 chars and 2 line display

// void lcdSetUp() {
//   lcd.init();
//   lcd.clear();         
//   lcd.backlight();      // Make sure backlight is on

//   // Print a message on first line
//   // as this msg not change here, it is place in the setup currently
//   lcd.setCursor(2,0);   // Set cursor to character 2 on line 0
//   lcd.print("Current:");
// }

void getIna219Data() {
  for (int x=0; x<5; x++) { // collect data with 5 times 1 set
    vTaskDelay(40);         // wait for I2C response

    // get the data from INA219
    current_mA = ina219.getCurrent_mA();
    busvoltage = ina219.getBusVoltage_V();
    shuntvoltage = ina219.getShuntVoltage_mV();
    power_mW = ina219.getPower_mW();

    // calculate the average current in mA
    static float current[5] = {0, 0, 0, 0, 0};  // save data with 5 times 1 set
    static float total_current;
    total_current -= current[x];  // delete the value 5 times before
    current[x] = current_mA;
    total_current += current[x];  // update the total value
    avgCurrent_mA = total_current/5;  // calculate the average value

    // print to LCD (debug use)
    // lcd.setCursor(2,1);   // Move cursor to character 2 on line 1
    // lcd.print(x);
    // vTaskDelay(100);      // the number change too fast would make it hard to see
  }
}