#include <AGIS_OLED.h>
#include <Wire.h>
#include <AGIS_Commons.h>
#include <AGIS_Logging.h>

/*Variables used to toggle display unit*/
unsigned int numIteration = 0;
bool unitChanged = false;

#ifdef OLED_I2C
/*Set up for OLED I2C*/
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);
#else
/*Set up for OLED SPI*/
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT,
  OLED_MOSI, OLED_CLK, OLED_DC, OLED_RESET, OLED_CS);
#endif

void oledSetUp() {
  // Initialize OLED
#ifdef OLED_I2C
  Wire.setPins(I2C_SDA, I2C_SCL);
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    ESP_LOGE(OLED_TAG, "SSD1306 allocation failed");
    return;
  } else {
    ESP_LOGI(OLED_TAG, "SSD1306 allocation succeeded");
    // clear the original display on the screen
    display.clearDisplay();
    display.display();
  }
#else
  if(!display.begin(SSD1306_SWITCHCAPVCC)) {
    ESP_LOGE(OLED_TAG, "SSD1306 allocation failed");
    return;
  } else {
    // clear the original display on the screen
    display.clearDisplay();
    display.display();
  }
#endif
}

// display the table on the screen
// only one display.display() should be used
void tableOledDisplay(int i, int j, int k) {
  // initialize setting of display
  display.clearDisplay();
  display.setTextSize(OLED_TEXT_FONT);
  display.setTextColor(SSD1306_WHITE);  // draw 'on' pixels

  // display.setCursor(1,16);  // set the position of the first letter
  // display.printf("Drip rate: %d\n", dripRate);

  // display.setCursor(1,24);  // set the position of the first letter
  // display.printf("Infused volume: %d.%d%d\n", i, j, k);

  // display.setCursor(1,32);  // set the position of the first letter
  // // if less than 1hour / 1minute, then not to display them
  // if((infusedTime/3600) >= 1){
  //   display.printf("Infused time: \n%dh %dmin %ds\n", infusedTime/3600, (infusedTime%3600)/60, infusedTime%60);
  // } else if((infusedTime/60) >= 1){
  //   display.printf("Infused time: \n%dmin %ds\n", infusedTime/60, infusedTime%60);
  // } else {
  //   display.printf("Infused time: \n%ds\n", infusedTime%60);
  // }

  display.setCursor(1,16);  // set the position of the first letter
  display.printf("%d.%d%dmL\n", i, j, k);
  if((infusedTime/3600) >= 1){
    display.printf("%dh%dm%ds\n", infusedTime/3600, (infusedTime%3600)/60, infusedTime%60);
  } else if((infusedTime/60) >= 1){
    display.printf("%dm%ds\n", infusedTime/60, infusedTime%60);
  } else {
    display.printf("%ds\n", infusedTime%60);
  }
  
  display.display();  
}

// display the warning message on the screen
void alertOledDisplay(const char* s) {
  display.clearDisplay();
  display.setTextSize(OLED_TEXT_FONT);
  display.setTextColor(SSD1306_WHITE);

  display.setCursor(1,16);
  display.println(F("ALARM: "));
  display.println(F(s));
  display.display();
}

// get the last digit of a number
int getLastDigit(int n) {
  static int j;
  static int k;
  j = (n / 10) * 10;
  k = n - j;
  return k;
}

void OLED_ISR() {
  if (infusionState == infusionState_t::ALARM_COMPLETED) {
    alertOledDisplay("infusion \ncompleted");
  } else if (infusionState == infusionState_t::ALARM_VOLUME_EXCEEDED) {
    alertOledDisplay("volume \nexceeded");
  } else if (infusionState == infusionState_t::ALARM_STOPPED) {
    alertOledDisplay("no recent \ndrop");
  } else {
    tableOledDisplay(numDrops/dropFactor, getLastDigit(numDrops*10/dropFactor), getLastDigit(numDrops*100/dropFactor));
  }
}