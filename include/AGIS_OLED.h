#ifndef DFF98014_4C3A_4E76_B495_11BC3D78A92F
#define DFF98014_4C3A_4E76_B495_11BC3D78A92F

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_TEXT_FONT 2

#define OLED_MOSI   17
#define OLED_CLK    47
#define OLED_DC     5
#define OLED_CS     6
#define OLED_RESET  7

extern volatile unsigned long infusedTime;

void oledSetUp();
void tableOledDisplay(int i, int j, int k);
void alertOledDisplay(const char* s);
int getLastDigit(int n);
void OLED_ISR();

#endif /* DFF98014_4C3A_4E76_B495_11BC3D78A92F */
