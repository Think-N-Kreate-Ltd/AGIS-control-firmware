#include <AGIS_INA219.h>
#include <AGIS_Commons.h>
#include <AGIS_Utilities.h>

#define ARRAYLENGTH 25

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

  if (!ina219.begin()) {
    Serial.println("Failed to find INA219 chip");
    while (1) { delay(10); }
  }
}

void getIna219Data() {
  for (int x=0; x<ARRAYLENGTH; x++) { // collect data with 5 times 1 set
    vTaskDelay(30);         // wait for I2C response

    // get the data from INA219
    current_mA = ina219.getCurrent_mA();
    busvoltage = ina219.getBusVoltage_V();
    shuntvoltage = ina219.getShuntVoltage_mV();
    power_mW = ina219.getPower_mW();

    // calculate the average current in mA
    static float current[ARRAYLENGTH];  // save data with `ARRAYLENGTH` times 1 set
    static float total_current;
    total_current -= current[x];  // delete the value 5 times before
    current[x] = current_mA;
    total_current += current[x];  // update the total value
    avgCurrent_mA = total_current/ARRAYLENGTH;  // calculate the average value
  }
  // Serial.printf("avg current is %3.3f, bus voltage is %3.3f\n", avgCurrent_mA, busvoltage);
}