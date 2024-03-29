#ifndef ECFC85B5_FBAD_44F5_B4D7_5DC33680411F
#define ECFC85B5_FBAD_44F5_B4D7_5DC33680411F

#include <TFT_eSPI.h>
// #include <pthread.h>
// #include <Free_Fonts.h>
#include <lvgl.h>
#include <lv_conf.h>
#include <AGIS_Keypad.h>
#include <AGIS_Types.h>
#include <AGIS_Commons.h>
#include <AGIS_Utilities.h>

// this follow the index in array `keypadInput`
#define VTBI_INDEX             0
#define TOTAL_TIME_HOUR_INDEX  1
#define TOTAL_TIME_MINUE_INDEX 2

extern volatile bool wifiStart;
extern bool keypadInfusionConfirmed;

void display_init();
// void test_screen();
void input_screen();
void ask_for_wifi_enable_msgbox();
void monitor_screen();

/*it is decrecated*/
void closeWifiBox();

/*----------function prototype only use in AGIS_Display.h (private)----------*/
void confirm_msgbox();
void remind_input_msgbox();

void my_disp_flush( lv_disp_drv_t *disp, const lv_area_t *area, lv_color_t *color_p );
bool validate_keypad_inputs();
/*set the textarea with coordinate x and y*/
void set_textarea(lv_obj_t *& testarea_object, uint16_t id, lv_coord_t x, lv_coord_t y);
static void radiobutton_create(lv_obj_t * parent, const char * txt);

// event handler
static void radio_event_handler(lv_event_t * e);
static void textarea_event_cb(lv_event_t * event);
static void wifibox_event_cb(lv_event_t * event);
static void confirmbox_event_cb(lv_event_t * event);
static void complete_event_cb(lv_event_t * event);
void keypad_read(lv_indev_drv_t * drv, lv_indev_data_t * data);

// timer for monitoring display
void infusion_monitoring_cb(lv_timer_t * timer);

#endif /* ECFC85B5_FBAD_44F5_B4D7_5DC33680411F */
