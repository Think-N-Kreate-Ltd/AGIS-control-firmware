#ifndef DFF98014_4C3A_4E76_B495_11BC3D78A92F
#define DFF98014_4C3A_4E76_B495_11BC3D78A92F

#include <Adafruit_SSD1306.h>
#include <Adafruit_GFX.h>

/*Choose from below OLED displays*/
// #define OLED_SPI    1
#define OLED_I2C 1

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_TEXT_FONT 2

#ifdef OLED_I2C
/*For OLED I2C*/
#define I2C_SCL 41
#define I2C_SDA 40
#define OLED_RESET -1
#define UNIT_SWITCH_PERIOD 15  // change from drops/min to mL/h every 15 seconds
#else
/*For OLED SPI*/
#define OLED_MOSI 17
#define OLED_CLK 47
#define OLED_DC 5
#define OLED_CS 6
#define OLED_RESET 7
#endif

extern unsigned int numIteration;
extern bool unitChanged;

extern Adafruit_SSD1306 display;
extern volatile unsigned long infusedTime;

void oledSetUp();
void tableOledDisplay(int i, int j, int k);
void alertOledDisplay(const char* s);
int getLastDigit(int n);
void OLED_ISR();

#endif /* DFF98014_4C3A_4E76_B495_11BC3D78A92F */
