#include <AGIS_Display.h>

// state for checking the wifibox, 0=waiting, 1=not start, 2=start
volatile uint8_t wifiStart = 0;
// a special state to lock the event change
// TODO: find whether have method to reset the event state (`lv_obj_remove_event_cb` have problem currently)
bool eventReseted;

static lv_disp_draw_buf_t disp_buf; // lv_disp_buf_t cannot use, use this one instead
static lv_color_t buf[TFT_WIDTH * TFT_HEIGHT / 10];

TFT_eSPI tft = TFT_eSPI();

lv_group_t * grp;           /*a group to group all keypad evented object*/
// lv_obj_t * screenTest;      /*a screen object which will hold all other objects for tesing*/
lv_obj_t * screenMain;      /*a screen object which will hold all other objects for input*/
lv_obj_t * screenMonitor;   /*a screen object which will hold all other objects for data display*/
lv_indev_t * keypad_indev;  /*a driver in LVGL and save the created input device object*/
lv_obj_t * vtbi_label;      // testing use

/*Parsed user input variables*/
int32_t keypad_VTBI = -1;

/*var for radio button*/
static lv_style_t style_radio;
static lv_style_t style_radio_chk;
static uint32_t active_index_1 = 0;

void display_init() {
  tft.begin();
  tft.setRotation(3);   // Landscape orientation, flipped
  tft.invertDisplay(false);  // set true to invert the color

  lv_init();

  lv_disp_draw_buf_init(&disp_buf, buf, NULL, TFT_WIDTH * TFT_HEIGHT / 10);

  /*Initialize the display driver*/
  static lv_disp_drv_t disp_drv;
  lv_disp_drv_init(&disp_drv);
  disp_drv.hor_res = TFT_HEIGHT;  /*flipped since we use horizontal view*/
  disp_drv.ver_res = TFT_WIDTH;
  disp_drv.flush_cb = my_disp_flush;
  disp_drv.draw_buf = &disp_buf;
  lv_disp_drv_register(&disp_drv);

  /*Initialize the input device driver, e.g. keypad, touch screen*/
  static lv_indev_drv_t indev_drv;
  lv_indev_drv_init(&indev_drv);
  indev_drv.type = LV_INDEV_TYPE_KEYPAD;
  indev_drv.read_cb = keypad_read;  // function of reading data from keypad
  /*Register the driver in LVGL and save the created input device object*/
  keypad_indev = lv_indev_drv_register(&indev_drv);
  /*set group*/
  grp = lv_group_create();
  lv_group_set_default(grp);  /*let the object create added to this group*/
  lv_indev_set_group(keypad_indev, grp);

  // Call every 500ms
  lv_timer_t * infusion_monitoring_timer = lv_timer_create(infusion_monitoring_cb, 500, NULL);
}

/*writes color information from the “color_p” pointer to the needed “area”*/
void my_disp_flush(lv_disp_drv_t *disp_drv, const lv_area_t *area, lv_color_t *color_p)
{
  uint32_t w = (area->x2 - area->x1 + 1);
  uint32_t h = (area->y2 - area->y1 + 1);
  uint32_t wh = w*h;

  tft.startWrite();
  tft.setAddrWindow(area->x1, area->y1, w, h);
  tft.pushColors(( uint16_t * )&color_p->full, w * h, true);
  tft.endWrite();

  lv_disp_flush_ready(disp_drv);
}

void ask_for_wifi_enable_msgbox() {
  static const char * btns[] = {"Yes", "No", ""};
  lv_obj_t * wifi_box = lv_msgbox_create(screenMain, "Enable WiFi?", NULL, btns, false);
  lv_obj_add_event_cb(wifi_box, wifibox_event_cb, LV_EVENT_VALUE_CHANGED, NULL);
  lv_group_focus_obj(lv_msgbox_get_btns(wifi_box));
  lv_obj_add_state(lv_msgbox_get_btns(wifi_box), LV_STATE_FOCUS_KEY);
  lv_group_focus_freeze(grp, true);

  /*set the position*/
  lv_obj_align(wifi_box, LV_ALIGN_CENTER, 0, 0);
  lv_obj_set_width(wifi_box, 125);

  /*make the background a little bit grey*/
  lv_obj_set_style_bg_opa(screenMain, LV_OPA_70, 0);
  lv_obj_set_style_bg_color(screenMain, lv_palette_main(LV_PALETTE_GREY), 0);

  /*there is no auto close in master version(8) of lvgl
  should close in other function, 
  thus, need to get the object `wifi_box` outside this function, 
  and a state to check whether the msgbox is closed*/
  lv_obj_move_to_index(wifi_box, 1);  // should less then the total no. of child
}

void input_screen() {
  /*a screen object which will hold all other objects*/
  screenMain = lv_obj_create(NULL);

  // /*Text area for VTBI_target (testing)*/
  // lv_obj_t * VTBI_target = lv_textarea_create(screenMain);
  // static int lv_VTBI_id = LV_VTBI_ID;
  // set_textarea(VTBI_target, lv_VTBI_id, 5, 25);

  // /*label for VTBI_target (testing)*/
  // lv_obj_t * vtbi_label = lv_label_create(screenMain);
  // lv_label_set_text(vtbi_label, "VTBI:");
  // lv_obj_align_to(vtbi_label, VTBI_target, LV_ALIGN_OUT_TOP_LEFT, 0, -5);  /*set position*/

  // lv_obj_t * mL_label = lv_label_create(screenMain);
  // lv_label_set_text(mL_label, "mL");
  // lv_obj_align_to(mL_label, VTBI_target, LV_ALIGN_OUT_RIGHT_MID, 5, 0);

  /* The idea is to enable `LV_OBJ_FLAG_EVENT_BUBBLE` on checkboxes and process the
   * `LV_EVENT_CLICKED` on the container.
   * A variable is passed as event user data where the index of the active
   * radiobutton is saved */

  lv_style_init(&style_radio);
  lv_style_set_radius(&style_radio, LV_RADIUS_CIRCLE);

  lv_style_init(&style_radio_chk);
  lv_style_set_bg_img_src(&style_radio_chk, NULL);

  uint8_t dripFactor[4] = {10, 15, 20, 60};
  char buf[16];

  lv_obj_t * cont1 = lv_obj_create(screenMain);
  lv_obj_set_flex_flow(cont1, LV_FLEX_FLOW_COLUMN);
  // lv_obj_set_size(cont1, lv_pct(40), lv_pct(80));
  lv_obj_align(cont1, LV_ALIGN_CENTER, 0, 0);
  lv_obj_add_event_cb(cont1, radio_event_handler, LV_EVENT_CLICKED, &active_index_1);

  for(int i=0; i<sizeof(dripFactor); i++) {
    lv_snprintf(buf, 16, "%d drops/mL", dripFactor[i]);
    radiobutton_create(cont1, buf);
  }

  /*Loads the main screen*/
  lv_disp_load_scr(screenMain);
}

void confirm_msgbox() {
  static const char * btns[] = {"No", "Yes", ""};
  char DR_buf[25];
  sprintf(DR_buf, "Drip Rate: %dTODO:", testing);
  lv_obj_t * confirm_box = lv_msgbox_create(screenMain, DR_buf, "Confirm To Run?", btns, false);
  lv_obj_add_event_cb(confirm_box, confirmbox_event_cb, LV_EVENT_VALUE_CHANGED, NULL);
  lv_group_focus_obj(lv_msgbox_get_btns(confirm_box));
  lv_obj_add_state(lv_msgbox_get_btns(confirm_box), LV_STATE_FOCUS_KEY);
  lv_group_focus_freeze(grp, true);

  /*set the position*/
  lv_obj_align(confirm_box, LV_ALIGN_CENTER, 0, 0);
  lv_obj_set_width(confirm_box, 125);

  /*make the background a little bit grey*/
  lv_obj_set_style_bg_opa(screenMain, LV_OPA_70, 0);
  lv_obj_set_style_bg_color(screenMain, lv_palette_main(LV_PALETTE_GREY), 0);
}

void monitor_screen() {
  /*a screen object which will hold all other objects */
  screenMonitor = lv_obj_create(NULL);

  /*as it is just create after `screenMinitor`, the index must be 0*/
  lv_obj_t * table = lv_table_create(screenMonitor);

  // lv_table_set_col_cnt(table, 2);
  // lv_table_set_row_cnt(table, 5);
  // lv_obj_align(table, LV_ALIGN_CENTER, 0, 0);
  // lv_obj_set_style_border_color(infusion_monitoring_table, lv_color_hex(0x5b5b5b), LV_PART_MAIN);
  lv_obj_set_style_border_opa(table, LV_OPA_TRANSP, LV_PART_MAIN);
  lv_obj_set_style_line_color(table, lv_color_hex(0x5b5b5b), LV_PART_MAIN);
  lv_table_set_col_width(table, 0, /*180*/90);
  lv_obj_set_height(table, 200);

  /*Fill the first column*/
  lv_table_set_cell_value(table, 0, 0, "Name");
  lv_table_set_cell_value(table, 1, 0, "Apple");
  lv_table_set_cell_value(table, 2, 0, "Banana");
  lv_table_set_cell_value(table, 3, 0, "Lemon");

  /*Fill the second column*/
  lv_table_set_cell_value(table, 0, 1, "Price");
  lv_table_set_cell_value(table, 1, 1, "$7");
  lv_table_set_cell_value(table, 2, 1, "$4");
  lv_table_set_cell_value(table, 3, 1, "$6");

  /*Add an event callback to to apply some custom drawing*/
  // lv_obj_add_event(table, draw_part_event_cb, LV_EVENT_DRAW_PART_BEGIN, NULL);
}

/*set the testarea with coordinate x and y*/
void set_textarea(lv_obj_t *& parent, uint16_t id, lv_coord_t x, lv_coord_t y) {
  lv_textarea_set_one_line(parent, true);
  lv_obj_align(parent, LV_ALIGN_TOP_LEFT, x, y);
  lv_obj_set_width(parent, 80);
  lv_textarea_set_placeholder_text(parent, "Pls input");
  lv_obj_set_user_data(parent, &id);
  lv_obj_add_event_cb(parent, textarea_event_cb, LV_EVENT_ALL, parent);

  // /*set style for boarder*/
  // static lv_style_t style;
  //   lv_style_init(&style);

  // /*Set a background color and a radius*/
  // lv_style_set_radius(&style, 10);
  // lv_style_set_bg_opa(&style, LV_OPA_COVER);
  // lv_style_set_bg_color(&style, lv_palette_lighten(LV_PALETTE_GREY, 1));

  // /*Add border to the bottom+right*/
  // lv_style_set_border_color(&style, lv_palette_main(LV_PALETTE_BLUE));
  // lv_style_set_border_width(&style, 5);
  // lv_style_set_border_opa(&style, LV_OPA_50);
  // lv_style_set_border_side(&style, LV_BORDER_SIDE_FULL);

  // lv_obj_add_style(parent, &style, 0);
}

static void radio_event_handler(lv_event_t * e) {
  uint32_t * active_id = (uint32_t *)lv_event_get_user_data(e);
  lv_obj_t * cont = lv_event_get_current_target(e);
  lv_obj_t * act_cb = lv_event_get_target(e);
  lv_obj_t * old_cb = lv_obj_get_child(cont, *active_id);

  /*Do nothing if the container was clicked*/
  if(act_cb == cont) return;
  // {
  //   Serial.println("pressed");
  //   uint32_t index = lv_obj_get_index(cont);
  //   Serial.println(index);
  // };

  // uint32_t index = lv_obj_get_index(cont);
  // Serial.println(index);

  lv_obj_clear_state(old_cb, LV_STATE_CHECKED);   /*Uncheck the previous radio button*/
  lv_obj_add_state(act_cb, LV_STATE_CHECKED);     /*Uncheck the current radio button*/

  *active_id = lv_obj_get_index(act_cb);
}

static void radiobutton_create(lv_obj_t * parent, const char * txt) {
  lv_obj_t * obj = lv_checkbox_create(parent);
  lv_checkbox_set_text(obj, txt);
  lv_obj_add_flag(obj, LV_OBJ_FLAG_EVENT_BUBBLE);
  lv_obj_add_style(obj, &style_radio, LV_PART_INDICATOR);
  lv_obj_add_style(obj, &style_radio_chk, LV_PART_INDICATOR | LV_STATE_CHECKED);
}

static void textarea_event_cb(lv_event_t * event) {
  /*not to print anything in this function*/
  
  // if(event->code == LV_EVENT_KEY && lv_indev_get_key(keypad_indev) == LV_KEY_ENTER) {
  //   lv_obj_t * ta = lv_event_get_target(event);

  //   // Parse the inputted data
  //   const char * data_buf = NULL;
  //   data_buf = lv_textarea_get_text(ta);

  //   // Identify which input
  //   static int32_t * selected_input;
  //   int ta_id = *(int *) lv_obj_get_user_data(ta);
  //   if (ta_id == LV_VTBI_ID) {
  //     selected_input = &keypad_VTBI;
  //   }
  //   else if (ta_id == LV_TOTAL_TIME_HOUR_ID) {
  //     selected_input = &keypad_totalTimeHour;
  //   }
  //   else if (ta_id == LV_TOTAL_TIME_MINUE_ID) {
  //     selected_input = &keypad_totalTimeMinute;
  //   }

  //   if (*data_buf) {  // only parse when we receive input
  //     *selected_input = atoi(data_buf);
  //   }
  //   else {
  //     // reset value of that input
  //     *selected_input = -1;
  //   }

  //   // Stop cursor blinking
  //   lv_obj_clear_state(ta, LV_STATE_FOCUSED);

  //   // Call the function to validate keypad inputs
  //   keypad_inputs_valid = validate_keypad_inputs();
  // }
}

static void wifibox_event_cb(lv_event_t * event) {
  lv_event_code_t code = lv_event_get_code(event);
  lv_obj_t * confirm_box = lv_event_get_current_target(event);

  if(code == LV_EVENT_VALUE_CHANGED && eventReseted) { /*is sent by the buttons if one of them is clicked*/
    const char * txt = lv_msgbox_get_active_btn_text(confirm_box);  /*get the button value*/
    if(txt){
      /*close the msgbox*/
      lv_msgbox_close(confirm_box); 
      lv_group_focus_freeze(grp, false);

      if(txt == "No") {
        /*go back to input screen*/
        lv_group_focus_obj(screenMain);
        lv_obj_scroll_to(screenMain, 0, 0, LV_ANIM_OFF);
        wifiStart = 1;
      } else if (txt == "Yes") {
        /*start wifi & web page enable*/
        wifiStart = 2;
      } else {/*I don't know how to go to this condition*/}
    }
    Serial.println(txt);
    eventReseted = false;

    /*set the background color back, not suggested to do by remove style*/
    lv_obj_set_style_bg_opa(screenMain, LV_OPA_100, 0);
    lv_obj_set_style_bg_color(screenMain, lv_color_hex(0xacacac), LV_PART_MAIN);
  }
}

static void confirmbox_event_cb(lv_event_t * event) {
  lv_event_code_t code = lv_event_get_code(event);
  lv_obj_t * confirm_box = lv_event_get_current_target(event);

  if(code == LV_EVENT_VALUE_CHANGED && eventReseted) { /*is sent by the buttons if one of them is clicked*/
    const char * txt = lv_msgbox_get_active_btn_text(confirm_box);  /*get the button value*/
    if(txt){
      /*close the msgbox*/
      lv_msgbox_close(confirm_box); 
      lv_group_focus_freeze(grp, false);

      if(txt == "No") {
        /*go back to input screen*/
        lv_group_focus_obj(screenMain);
        lv_obj_scroll_to(screenMain, 0, 0, LV_ANIM_OFF);
      } else if (txt == "Yes") {
        /*go to monitor screen, TODO: and start infusion*/
        lv_disp_load_scr(screenMonitor);
      } else {/*I don't know how to go to this condition*/}
    }
    Serial.println(txt);
    eventReseted = false;

    /*set the background color back, not suggested to do by remove style*/
    lv_obj_set_style_bg_opa(screenMain, LV_OPA_100, 0);
    lv_obj_set_style_bg_color(screenMain, lv_color_hex(0xacacac), LV_PART_MAIN);
  }
}

void keypad_read(lv_indev_drv_t * drv, lv_indev_data_t * data){
  uint8_t key = keypad.getKey();
  Serial.write(key);
  if(key) {
    if (key == 'E') {
      data->key = LV_KEY_ENTER;
      Serial.println("pressed enter");
      eventReseted = true;
    }
    else if (key == 'C') {
      // data->key = LV_KEY_ESC;
      // TODO: depending on context, e.g. pause infusion
      // software reset:
      esp_restart();
    }
    else if (key == 'L') {
      data->key = LV_KEY_LEFT;
    }
    else if (key == 'R') {
      data->key = LV_KEY_RIGHT;
    }
    else if (key == 'U') {
      data->key = LV_KEY_PREV;
      buttonState = buttonState_t::UP;
    }
    else if (key == 'D') {
      data->key = LV_KEY_NEXT;
      buttonState = buttonState_t::DOWN;
    }
    else if (key == '#') {
      data->key = LV_KEY_BACKSPACE;
    }
    else if (key == '*') {
      data->key = LV_KEY_DEL;
      buttonState = buttonState_t::ENTER;
    }
    else if (key == 'F') {
      /*convert the screen (go to monitor screen when first pressed)*/
      static bool state;
      if (state) {
        lv_disp_load_scr(screenMain);
      } else {
        lv_disp_load_scr(screenMonitor);
      }
      state = !state;
    }
    // TODO: add `G`, now is missing lots of things here
    else if (key == 'G') {
      confirm_msgbox();
    }
    else {
      data->key = key;  /*possible BUG due to conversion from char to uint32_t(?)*/
    }

    data->state = LV_INDEV_STATE_PRESSED;
  }
  else if (keypad.getState() == 0) {  // when keypad pressing is released
    data->state = LV_INDEV_STATE_RELEASED;
    // stop the motor control
    if (buttonState != buttonState_t::IDLE) {
      buttonState = buttonState_t::IDLE;
    }
  }
}

// update the data on display every 500ms
void infusion_monitoring_cb(lv_timer_t * timer) {
  lv_table_set_cell_value_fmt(lv_obj_get_child(screenMonitor, 0), 1, 1, "%d", testing);
  lv_table_set_cell_value_fmt(lv_obj_get_child(screenMonitor, 0), 1, 0, "%02d:%02d:%02d", testing/3600, testing%3600/60, testing%60);

  // if (infusion_monitoring_table != NULL) {
  //   /*Update LVGL table cells*/
  //   char numDrops_buf[20];
  //   sprintf(numDrops_buf, "%d", *(infusion_monitoring_data_handle.numDrops_p));
  //   lv_table_set_cell_value(infusion_monitoring_table, 0, 1, numDrops_buf);

  //   char dripRate_buf[20];
  //   sprintf(dripRate_buf, "%d", *(infusion_monitoring_data_handle.dripRate_p));
  //   lv_table_set_cell_value(infusion_monitoring_table, 1, 1, dripRate_buf);

  //   // since `infusedVolume_x100` is 100 times larger than actual value in mL,
  //   // we need to divide by 100 before display
  //   char infusedVolume_buf[20];
  //   sprintf(infusedVolume_buf, "%.2f", *(infusion_monitoring_data_handle.infusedVolume_p) / 100.0f);
  //   lv_table_set_cell_value(infusion_monitoring_table, 2, 1, infusedVolume_buf);

  //   char infusedTime_buf[20];
  //   // Convert the infused time from seconds to HH:MM:SS format
  //   unsigned long sec;
  //   uint16_t h, m, s;
  //   sec = *(infusion_monitoring_data_handle.infusedTime_p);
  //   h = sec / 3600;
  //   m = (sec - (h * 3600)) / 60;
  //   s = sec - (h * 3600) - (m * 60);

  //   sprintf(infusedTime_buf, "%02d:%02d:%02d", h, m, s);
  //   lv_table_set_cell_value(infusion_monitoring_table, 3, 1, infusedTime_buf);

  //   const char *infusionState_buf =
  //       getInfusionState(*(infusion_monitoring_data_handle.infusionState_p));
  //   lv_table_set_cell_value(infusion_monitoring_table, 4, 1, infusionState_buf);
  // }
}

void closeWifiBox() {
    lv_msgbox_close(lv_obj_get_child(screenMain, 1)); /*close wifi box*/
}
