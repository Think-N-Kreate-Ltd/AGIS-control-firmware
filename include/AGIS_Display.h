#ifndef ECFC85B5_FBAD_44F5_B4D7_5DC33680411F
#define ECFC85B5_FBAD_44F5_B4D7_5DC33680411F

#include <TFT_eSPI.h>
// #include <Free_Fonts.h>
#include <lvgl.h>
#include <lv_conf.h>
#include <AGIS_Keypad.h>
#include <AGIS_Types.h>

#define LV_VTBI_ID             100
// #define LV_TOTAL_TIME_HOUR_ID  101
// #define LV_TOTAL_TIME_MINUE_ID 102
// #define LV_DROP_FACTOR_10_ID   0
// #define LV_DROP_FACTOR_15_ID   1
// #define LV_DROP_FACTOR_20_ID   2
// #define LV_DROP_FACTOR_60_ID   3

extern volatile uint8_t wifiStart;  // 0=waiting, 1=not start, 2=start

void display_init();
// void test_screen();
void input_screen();
void ask_for_wifi_enable_msgbox();
void monitor_screen();

void closeWifiBox();

// for testing
extern volatile int testing;

// bool validate_keypad_inputs();
// int32_t calculate_drip_rate(int32_t volume, int32_t time, int32_t dropFactor);
// void infusion_monitoring_cb(lv_timer_t * timer);

/*----------function prototype only use in AGIS_Display.h (private)----------*/
void confirm_msgbox();

void my_disp_flush( lv_disp_drv_t *disp, const lv_area_t *area, lv_color_t *color_p );
// bool validate_keypad_inputs();
// int32_t calculate_drip_rate(int32_t volume, int32_t time, int32_t dropFactor);
/*set the textarea with coordinate x and y*/
void set_textarea(lv_obj_t *& testarea_object, uint16_t id, lv_coord_t x, lv_coord_t y);
static void radio_event_handler(lv_event_t * e);
static void radiobutton_create(lv_obj_t * parent, const char * txt);

// event handler
// static void radio_event_cb(lv_event_t * event);
static void textarea_event_cb(lv_event_t * event);
static void wifibox_event_cb(lv_event_t * event);
static void confirmbox_event_cb(lv_event_t * event);
void keypad_read(lv_indev_drv_t * drv, lv_indev_data_t * data);

// timer for monitoring display
void infusion_monitoring_cb(lv_timer_t * timer);

#endif /* ECFC85B5_FBAD_44F5_B4D7_5DC33680411F */
