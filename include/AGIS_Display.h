#ifndef ECFC85B5_FBAD_44F5_B4D7_5DC33680411F
#define ECFC85B5_FBAD_44F5_B4D7_5DC33680411F

#include <TFT_eSPI.h>
// #include <Free_Fonts.h>
#include <lvgl.h>
#include <lv_conf.h>
#include <AGIS_Keypad.h>
#include <AGIS_Types.h>

#define LV_VTBI_ID             100
#define LV_TOTAL_TIME_HOUR_ID  101
#define LV_TOTAL_TIME_MINUE_ID 102
#define LV_DROP_FACTOR_10_ID   0
#define LV_DROP_FACTOR_15_ID   1
#define LV_DROP_FACTOR_20_ID   2
#define LV_DROP_FACTOR_60_ID   3

// extern lv_disp_draw_buf_t draw_buf;
// extern lv_color_t color_buf[ TFT_WIDTH * TFT_HEIGHT / 5 ];

extern lv_obj_t * input_scr;
// extern lv_obj_t * monitor_scr;
// extern lv_indev_t * keypad_indev;
// extern lv_group_t * input_grp;

// // Use these variables in main.cpp
// extern int32_t keypad_VTBI;
// extern int32_t keypad_totalTimeHour;
// extern int32_t keypad_totalTimeMinute;
// extern int32_t keypad_targetDripRate;
// extern int32_t keypad_dropFactor;

// /*Keypad variables*/
extern Keypad keypad;
// extern bool keypad_inputs_valid;
extern bool keypadInfusionConfirmed;

void display_init();
void my_disp_flush( lv_disp_drv_t *disp, const lv_area_t *area, lv_color_t *color_p );
void input_screen(void);
static void textarea_event_handler(lv_event_t * event);
static void radiobutton_create(lv_obj_t * parent, const char * txt, uint32_t i);
static void radio_event_handler(lv_event_t * event);
// void monitor_screen();
static void keypad_read(lv_indev_drv_t * drv, lv_indev_data_t * data);
bool validate_keypad_inputs();
int32_t calculate_drip_rate(int32_t volume, int32_t time, int32_t dropFactor);
void infusion_monitoring_cb(lv_timer_t * timer);

#endif /* ECFC85B5_FBAD_44F5_B4D7_5DC33680411F */
