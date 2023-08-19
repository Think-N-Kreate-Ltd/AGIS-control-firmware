/**
 * Try to avoid calling LVGL functions from interrupt handlers
 * LVGL is also thread-unsafe / task-unsafe (unsafe on multitasking)
 * If need to use real tasks or threads, need a lock before and after lvgl function
 * (check here https://docs.lvgl.io/master/porting/os.html)
 * HOWEVER, sleep is also so harmful
 * (check here https://stackoverflow.com/questions/8815895/why-is-thread-sleep-so-harmful)
 * thus, I avoid use of it and there are many limitations, it's normal to feel strange on some parts
 * 
 * LVGL cannot use justify content
 * on the other hand, msg box itmes have width=100%
 * thus, we need to set them one by one to custom the position of the text
 * note: can use `lv_obj_get_width()` to find the width
 * 
 * screenMain index: 0-2->input field; 5-6->other use
*/

#include <AGIS_Display.h>

// state for checking the wifibox, false=not enable
volatile bool wifiStart = NULL;
// a special state to lock the event change
// seems no method to reset the event state (`lv_obj_remove_event_cb` have problem and cannot use)
bool enterClicked;
// the state of the msgbox, true=in msgbox. for switching the use of key `L` `R`
// also use for preventing msgbox pop up twice
bool inMsgbox;
// the state of the screen, true=input screen, false=monitor screen
bool screenState = true;
// check for whether all text area are filled
bool allInputs = false;
// an array to store the option of drip factor
// uint8_t dripFactor[4] = {10, 15, 20, 60};
// an array to store keypad input, 0=VTBI, 1=timeHr, 2=timeMin
int32_t keypadInput[3] = {-1, -1, -1};

static lv_disp_draw_buf_t disp_buf;
static lv_color_t buf[TFT_WIDTH * TFT_HEIGHT / 10];

TFT_eSPI tft = TFT_eSPI();

lv_group_t * grp;           /*a group to group all keypad evented object*/
lv_obj_t * screenWifi;      /*a screen object which hold wifi msgbox object*/
lv_obj_t * screenMain;      /*a screen object which will hold all other objects for input*/
lv_obj_t * screenMonitor;   /*a screen object which will hold all other objects for data display*/
lv_indev_t * keypad_indev;  /*a driver in LVGL and save the created input device object*/

/*var for radio button*/
static lv_style_t style_radio;
static lv_style_t style_radio_chk;
static uint32_t active_index_1 = 0;

void display_init() {
  tft.begin();
  tft.setRotation(1);        // Landscape orientation

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

  tft.startWrite();
  tft.setAddrWindow(area->x1, area->y1, w, h);
  tft.pushColors(( uint16_t * )&color_p->full, w * h, true);
  tft.endWrite();

  lv_disp_flush_ready(disp_drv);
}

void ask_for_wifi_enable_msgbox() {
  /*now use a new screen obj to hold msgbox to avoid text overlap*/
  screenWifi = lv_obj_create(NULL);

  static const char * btns[] = {"Yes", "No", ""};
  lv_obj_t * wifi_box = lv_msgbox_create(screenWifi, "Enable WiFi?", NULL, btns, false);
  lv_obj_set_width(lv_obj_get_child(wifi_box, 0), 95);
  lv_obj_add_event_cb(wifi_box, wifibox_event_cb, LV_EVENT_VALUE_CHANGED, NULL);
  lv_group_focus_obj(lv_msgbox_get_btns(wifi_box));
  lv_obj_add_state(lv_msgbox_get_btns(wifi_box), LV_STATE_FOCUS_KEY);
  // lv_group_add_obj(grp, wifi_box);
  lv_group_focus_freeze(grp, true);

  /*set the position*/
  lv_obj_set_flex_align(wifi_box, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_SPACE_AROUND);
  /*set the size to use 99% area, which can avoid `detected modifying dirty areas in render`*/
  lv_obj_set_size(wifi_box, lv_pct(99), lv_pct(99));
  // lv_obj_set_layout(wifi_box, LV_LAYOUT_GRID);

  /*there is no auto close in master version(8) of lvgl
  should close in other function, 
  thus, need to get the object `wifi_box` outside this function, 
  and a state to check whether the msgbox is closed*/
  // lv_obj_move_to_index(wifi_box, 1);  // should less then the total no. of child

  lv_disp_load_scr(screenWifi);
  inMsgbox = true;
}

void input_screen() {
  /*a screen object which will hold all other objects*/
  screenMain = lv_obj_create(NULL);

  /*Text area for VTBI_target*/
  lv_obj_t * VTBI_target = lv_textarea_create(screenMain);
  set_textarea(VTBI_target, VTBI_INDEX, 5, 25);

  /*label for VTBI_target*/
  lv_obj_t * vtbi_label = lv_label_create(screenMain);
  lv_label_set_text(vtbi_label, "VTBI:");
  lv_obj_align_to(vtbi_label, VTBI_target, LV_ALIGN_OUT_TOP_LEFT, 0, -5);

  lv_obj_t * mL_label = lv_label_create(screenMain);
  lv_label_set_text(mL_label, "mL");
  lv_obj_align_to(mL_label, VTBI_target, LV_ALIGN_OUT_RIGHT_MID, 5, 0);

  /*Text area for timeHr_target*/
  lv_obj_t * timeHr_target = lv_textarea_create(screenMain);
  set_textarea(timeHr_target, TOTAL_TIME_HOUR_INDEX, 5, 91);

  /*label for timeHr_target*/
  lv_obj_t * timeHr_label = lv_label_create(screenMain);
  lv_label_set_text(timeHr_label, "Total time:");
  lv_obj_align_to(timeHr_label, timeHr_target, LV_ALIGN_OUT_TOP_LEFT, 0, -5);

  lv_obj_t * hr_label = lv_label_create(screenMain);
  lv_label_set_text(hr_label, "hours");
  lv_obj_align_to(hr_label, timeHr_target, LV_ALIGN_OUT_RIGHT_MID, 5, 0);

  /*Text area for timeMin_target*/
  lv_obj_t * timeMin_target = lv_textarea_create(screenMain);
  set_textarea(timeMin_target, TOTAL_TIME_MINUE_INDEX, 160, 91);

  /*label for timeMin_target*/
  lv_obj_t * min_label = lv_label_create(screenMain);
  lv_label_set_text(min_label, "minutes");
  lv_obj_align_to(min_label, timeMin_target, LV_ALIGN_OUT_RIGHT_MID, 5, 0);

  /* The idea is to enable `LV_OBJ_FLAG_EVENT_BUBBLE` on checkboxes and process the
   * `LV_EVENT_CLICKED` on the container.
   * A variable is passed as event user data where the index of the active
   * radiobutton is saved */

  lv_style_init(&style_radio);
  lv_style_set_radius(&style_radio, LV_RADIUS_CIRCLE);

  lv_style_init(&style_radio_chk);
  lv_style_set_bg_img_src(&style_radio_chk, NULL);

  char buf[16];

  lv_obj_t * cont1 = lv_obj_create(screenMain);
  lv_obj_set_flex_flow(cont1, LV_FLEX_FLOW_COLUMN_WRAP);
  lv_obj_set_style_flex_main_place(cont1, LV_FLEX_ALIGN_SPACE_AROUND, 0);
  lv_obj_set_style_flex_cross_place(cont1, LV_FLEX_ALIGN_CENTER, 0);
  lv_obj_set_style_pad_column(cont1, 15, 0);
  lv_obj_set_size(cont1, lv_pct(96), lv_pct(33));
  lv_obj_align(cont1, LV_ALIGN_BOTTOM_MID, 0, -3);
  lv_obj_add_event_cb(cont1, radio_event_handler, LV_EVENT_CLICKED, &active_index_1);
  lv_obj_set_scrollbar_mode(cont1, LV_SCROLLBAR_MODE_OFF);  /*not show scrollbars*/
  lv_obj_set_scroll_snap_x(cont1, LV_SCROLL_SNAP_NONE);     /*nothing change currently, just keep here as reminder*/
  lv_obj_move_to_index(cont1, 5); /*for scrolling after wifibox close*/

  /*add radio button*/
  for(int i=0; i<lengthOfDF; i++) {
    lv_snprintf(buf, 16, "%d drops/mL  ", dripFactor[i]);
    radiobutton_create(cont1, buf);
  }

  /*label for drop factor*/
  lv_obj_t * drop_factor_label = lv_label_create(screenMain);
  lv_label_set_text(drop_factor_label, "Drop Factor:");
  lv_obj_align_to(drop_factor_label, cont1, LV_ALIGN_OUT_TOP_LEFT, 0, -5);

  /*set background color*/
  lv_obj_set_style_bg_opa(screenMain, LV_OPA_100, 0);
  lv_obj_set_style_bg_color(screenMain, lv_color_hex(0xacacac), LV_PART_MAIN);

  /*Widget for showing derived drip rate*/
  lv_obj_t * DRWidget = lv_obj_create(screenMain);
  lv_obj_set_style_border_color(DRWidget, lv_color_hex(0x5b5b5b), LV_PART_MAIN);
  lv_obj_set_style_radius(DRWidget, 0x00, LV_PART_MAIN);
  lv_obj_set_size(DRWidget, 183, 70);
  lv_obj_align(DRWidget, LV_ALIGN_TOP_RIGHT, -7, 7);

  /*label for derived drip rate widget*/
  lv_obj_t * DR_label1 = lv_label_create(DRWidget);
  lv_label_set_text(DR_label1, "Drip rate (drops/min):");
  lv_obj_align_to(DR_label1, DRWidget, LV_ALIGN_TOP_LEFT, -5, 0);

  lv_obj_t * DR_label2 = lv_label_create(DRWidget);
  lv_label_set_text(DR_label2, "Please fill in all inputs");
  lv_obj_set_style_text_color(DR_label2, lv_color_hex(0xcc0000), LV_PART_MAIN);
  lv_obj_align_to(DR_label2, DR_label1, LV_ALIGN_LEFT_MID, 0, 20);
  /*as we need to change the text of this label, we need to set index*/
  lv_obj_move_to_index(DRWidget, 6);
  /*don't need to set for `DR_label2` as it must be 1*/

  /*Loads the main screen*/
  // lv_disp_load_scr(screenMain);
}

void confirm_msgbox() {
  static const char * btns[] = {"Back", "Yes", ""};
  char DR_buf[25];
  sprintf(DR_buf, "Drip Rate: %d", targetDripRate);
  lv_obj_t * confirm_box = lv_msgbox_create(screenMain, DR_buf, "Confirm To Run?\n", btns, false);
  lv_obj_set_width(lv_obj_get_child(confirm_box, 0), 160);
  lv_obj_set_width(lv_obj_get_child(confirm_box, 1), 150);
  lv_obj_add_event_cb(confirm_box, confirmbox_event_cb, LV_EVENT_VALUE_CHANGED, NULL);
  lv_group_focus_obj(lv_msgbox_get_btns(confirm_box));
  lv_obj_add_state(lv_msgbox_get_btns(confirm_box), LV_STATE_FOCUS_KEY);
  lv_group_focus_freeze(grp, true);

  /*set the position*/
  lv_obj_align(confirm_box, LV_ALIGN_CENTER, 0, 0);
  lv_obj_set_flex_align(confirm_box, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_SPACE_AROUND);
  // lv_obj_set_width(confirm_box, 125);
  lv_obj_set_size(confirm_box, lv_pct(60), lv_pct(50));

  /*make the background a little bit grey*/
  // lv_obj_set_style_bg_opa(screenMain, LV_OPA_70, 0);
  // lv_obj_set_style_bg_color(screenMain, lv_palette_main(LV_PALETTE_GREY), 0);

  inMsgbox = true;
}

void remind_input_msgbox() {
  static const char * btns[] = {"Back", ""};
  lv_obj_t * confirm_box = lv_msgbox_create(screenMain, "Plz check all fields", NULL, btns, false);
  lv_obj_set_width(lv_obj_get_child(confirm_box, 0), 135);
  lv_obj_add_event_cb(confirm_box, confirmbox_event_cb, LV_EVENT_VALUE_CHANGED, NULL);
  lv_group_focus_obj(lv_msgbox_get_btns(confirm_box));
  lv_obj_add_state(lv_msgbox_get_btns(confirm_box), LV_STATE_FOCUS_KEY);
  lv_group_focus_freeze(grp, true);

  /*set the position*/
  lv_obj_align(confirm_box, LV_ALIGN_CENTER, 0, 0);
  lv_obj_set_flex_align(confirm_box, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_SPACE_AROUND);
  // lv_obj_set_width(confirm_box, 125);
  lv_obj_set_size(confirm_box, lv_pct(60), lv_pct(50));

  /*make the background a little bit grey*/
  // lv_obj_set_style_bg_opa(screenMain, LV_OPA_70, 0);
  // lv_obj_set_style_bg_color(screenMain, lv_palette_main(LV_PALETTE_GREY), 0);

  inMsgbox = true;
}

void monitor_screen() {
  /*a screen object which will hold all other objects */
  screenMonitor = lv_obj_create(NULL);

  /*as it is just create after `screenMinitor`, the index must be 0*/
  lv_obj_t * table = lv_table_create(screenMonitor);

  lv_obj_align(table, LV_ALIGN_CENTER, 0, 0);
  // lv_obj_set_style_border_color(table, lv_color_hex(0x5b5b5b), LV_PART_MAIN);
  lv_obj_set_style_border_opa(table, LV_OPA_TRANSP, LV_PART_MAIN);
  lv_obj_set_style_line_color(table, lv_color_hex(0x5b5b5b), LV_PART_MAIN);
  lv_table_set_col_width(table, 0, 180);
  // lv_obj_set_height(table, 200);

  /*Fill the first column*/
  lv_table_set_cell_value(table, 0, 0, "No. of drops:");
  lv_table_set_cell_value(table, 1, 0, "Drip rate (drops/min):");
  lv_table_set_cell_value(table, 2, 0, "Infused volume (mL):");
  lv_table_set_cell_value(table, 3, 0, "Infused time:");
  lv_table_set_cell_value(table, 4, 0, "Infusion state:");

  /*Fill the second column*/
  for (int i=0; i<5; i++) {
    lv_table_set_cell_value(table, i, 1, "Not started");
  }

  /*cannot select table*/
  lv_group_remove_obj(table);

  /*Add an event callback to to apply some custom drawing*/
  // lv_obj_add_event(table, draw_part_event_cb, LV_EVENT_DRAW_PART_BEGIN, NULL);
}

/*set the testarea with coordinate x and y*/
void set_textarea(lv_obj_t *& parent, uint16_t index, lv_coord_t x, lv_coord_t y) {
  lv_textarea_set_one_line(parent, true);
  lv_textarea_set_max_length(parent, 4);  /*Note: container volume commonly from 50ml to 3L*/
  lv_obj_align(parent, LV_ALIGN_TOP_LEFT, x, y);
  lv_obj_set_width(parent, 80); /*Note: width=80, height=36*/
  lv_textarea_set_placeholder_text(parent, "Pls input");
  /*in fact, if we create all textarea obj first, then we don't need to set index*/
  lv_obj_move_to_index(parent, index);
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

static void radiobutton_create(lv_obj_t * parent, const char * txt) {
  lv_obj_t * obj = lv_checkbox_create(parent);
  lv_checkbox_set_text(obj, txt);
  lv_obj_set_width(obj, lv_pct(47));
  lv_obj_add_flag(obj, LV_OBJ_FLAG_EVENT_BUBBLE);
  lv_obj_add_style(obj, &style_radio, LV_PART_INDICATOR);
  lv_obj_add_style(obj, &style_radio_chk, LV_PART_INDICATOR | LV_STATE_CHECKED);
}

static void radio_event_handler(lv_event_t * e) {
  uint32_t * active_id = (uint32_t *)lv_event_get_user_data(e);
  lv_obj_t * cont = lv_event_get_current_target(e);
  lv_obj_t * act_cb = lv_event_get_target(e);
  lv_obj_t * old_cb = lv_obj_get_child(cont, *active_id);

  /*Do nothing if the container was clicked*/
  if(act_cb == cont) return;

  lv_obj_clear_state(old_cb, LV_STATE_CHECKED);   /*Uncheck the previous radio button*/
  lv_obj_add_state(act_cb, LV_STATE_CHECKED);     /*Uncheck the current radio button*/

  *active_id = lv_obj_get_index(act_cb);          /*record the index for unchecked next time*/
  uint16_t i = lv_obj_get_index(act_cb);          /*get the index if button*/
  dropFactor = dripFactor[i];                     /*store the drip factor selected*/

  /*check for whether all inputs are filled*/
  allInputs = validate_keypad_inputs();
}

static void textarea_event_cb(lv_event_t * event) {  
  lv_event_code_t code = lv_event_get_code(event);
  lv_obj_t * ta = lv_event_get_target(event);

  if (code == LV_EVENT_CLICKED || code == LV_EVENT_FOCUSED) {
    /*when text area is clicked*/
    /*do nothing*/
  } else if (code == LV_EVENT_READY) {
    /*get the input and store it*/
    uint16_t i = lv_obj_get_index(ta);
    keypadInput[i] = atoi(lv_textarea_get_text(ta));
    /*change the input text color*/
    lv_obj_set_style_text_color(lv_obj_get_child(screenMain, i), lv_palette_main(LV_PALETTE_GREEN), 0);

    /*check for whether all inputs are filled*/
    allInputs = validate_keypad_inputs();
  }
}

static void wifibox_event_cb(lv_event_t * event) {
  lv_event_code_t code = lv_event_get_code(event);
  lv_obj_t * confirm_box = lv_event_get_current_target(event);

  if(code == LV_EVENT_VALUE_CHANGED && enterClicked) { /*is sent by the buttons if one of them is clicked*/
    const char * txt = lv_msgbox_get_active_btn_text(confirm_box);  /*get the button value*/
    if(txt){
      /*close the msgbox*/
      lv_msgbox_close(confirm_box); 
      lv_group_focus_freeze(grp, false);
      inMsgbox = false;
      /*go back to input screen*/
      lv_disp_load_scr(screenMain);
      lv_obj_scroll_to_view(lv_obj_get_child(lv_obj_get_child(screenMain, 5), 0), LV_ANIM_OFF);
      /*start from looking at top VTBI input field*/
      lv_group_focus_obj(lv_obj_get_child(screenMain, VTBI_INDEX));

      if(txt == "No") {
        /*not to enable wifi*/
        wifiStart = false;
      } else if (txt == "Yes") {
        /*start wifi & web page enable*/
        wifiStart = true;
      } else {/*I don't know how to go to this condition*/}
    }
  }
}

static void confirmbox_event_cb(lv_event_t * event) {
  lv_event_code_t code = lv_event_get_code(event);
  lv_obj_t * confirm_box = lv_event_get_current_target(event);

  if(code == LV_EVENT_VALUE_CHANGED && enterClicked) { /*is sent by the buttons if one of them is clicked*/
    const char * txt = lv_msgbox_get_active_btn_text(confirm_box);  /*get the button value*/
    if(txt){
      /*close the msgbox*/
      lv_msgbox_close(confirm_box); 
      lv_group_focus_freeze(grp, false);
      inMsgbox = false;

      /*set the background color back, not suggested to do by remove style*/
      // lv_obj_set_style_bg_opa(screenMain, LV_OPA_100, 0);
      // lv_obj_set_style_bg_color(screenMain, lv_color_hex(0xacacac), LV_PART_MAIN);

      if(txt == "Back") {
        /*go back to input screen*/
        lv_group_focus_obj(screenMain);
        lv_obj_scroll_to(screenMain, 0, 0, LV_ANIM_OFF);
      } else if (txt == "Yes") {
        Serial.print(txt);
        /*Submit verified inputs to autoControl*/
        targetVTBI = keypadInput[0];
        targetTotalTime = keypadInput[1]*3600 + keypadInput[2]*60;
        // targetDripRate = targetVTBI * dropFactor / (keypadInput[1]*60 + keypadInput[2]);
        targetNumDrops = targetVTBI * dropFactor;
        Serial.print(", drop factor=");
        /*go to monitor screen, and start infusion*/
        lv_disp_load_scr(screenMonitor);
        keypadInfusionConfirmed = true;
        screenState = false;
        /*avoid carshing. In fact, it is better to reduce the workload on INT*/
        Serial.println(dropFactor);
        vTaskDelay(100);
      } else {/*I don't know how to go to this condition*/}
    }
  }
}

void keypad_read(lv_indev_drv_t * drv, lv_indev_data_t * data){
  uint8_t key = keypad.getKey();
  if(key) {
    Serial.write(key);
    if (key == 'E') {
      data->key = LV_KEY_ENTER;
      enterClicked = true;
    }
    else if (key == 'C') {
      // data->key = LV_KEY_ESC;
      // software reset:
      esp_restart();
    }
    else if (key == 'L') {
      if (inMsgbox) {
        data->key = LV_KEY_LEFT;
      } else {
        data->key = LV_KEY_PREV;
      }
    }
    else if (key == 'R') {
      if (inMsgbox) {
        data->key = LV_KEY_RIGHT;
      } else {
        data->key = LV_KEY_NEXT;
      }
    }
    else if (key == 'U') {
      buttonState = buttonState_t::UP;
    }
    else if (key == 'D') {
      buttonState = buttonState_t::DOWN;
    }
    else if (key == '#') {
      data->key = LV_KEY_BACKSPACE;
    }
    else if (key == '*') {
      buttonState = buttonState_t::ENTER;
    }
    else if (key == 'F') {
      /*convert the screen*/
      if (screenState) {
        lv_disp_load_scr(screenMonitor);
      } else {
        lv_disp_load_scr(screenMain);
      }
      screenState = !screenState;
    }
    else if (key == 'G') {
      /*not to pop up the message box if input missed or in monitor screen*/
      /*also avoid pop up the msgbox twice*/
      if (allInputs && screenState && !inMsgbox) {
        if ((targetDripRate >= 20) && (targetDripRate <= 400)) {
          /*pop up a message box to confirm*/
          confirm_msgbox();
        } else {
          /*pop up a message box to alarm the input is so strange*/
          remind_input_msgbox();
        }
      } else {
        /*pop up a message box to ask for inputs*/
        remind_input_msgbox();
      }
    }
    else {
      data->key = key;  /*should not enter here*/
    }

    data->state = LV_INDEV_STATE_PRESSED;
  }
  else if (keypad.getState() == 0) {  // when keypad pressing is released
    data->state = LV_INDEV_STATE_RELEASED;
    enterClicked = false;
    // stop the motor control
    if (buttonState != buttonState_t::IDLE) {
      buttonState = buttonState_t::IDLE;
    }
  }
}

/*update the data on display every 500ms*/
void infusion_monitoring_cb(lv_timer_t * timer) {
  if (infusionState == infusionState_t::NOT_STARTED) {
    /*do nothing here, stop the screen*/
  } else {
    // lv_table_set_cell_value_fmt(lv_obj_get_child(screenMonitor, 0), 0, 1, "%d, %d", testCount, testTime);
    lv_table_set_cell_value_fmt(lv_obj_get_child(screenMonitor, 0), 0, 1, "%d", numDrops);
    lv_table_set_cell_value_fmt(lv_obj_get_child(screenMonitor, 0), 1, 1, "%d", dripRate);
    lv_table_set_cell_value_fmt(lv_obj_get_child(screenMonitor, 0), 2, 1, "%d.%02d", infusedVolume_x100/100, infusedVolume_x100%100);
    lv_table_set_cell_value_fmt(lv_obj_get_child(screenMonitor, 0), 3, 1, "%02d:%02d:%02d", infusedTime/3600, infusedTime%3600/60, infusedTime%60);
    lv_table_set_cell_value(lv_obj_get_child(screenMonitor, 0), 4, 1, getInfusionState(infusionState));
  }
}

/*it is decrecated*/
void closeWifiBox() {
  // static pthread_mutex_t lvgl_mutex;
  // pthread_mutex_lock(&lvgl_mutex);
  vTaskDelay(50);
  lv_msgbox_close(lv_obj_get_child(screenWifi, 0)); /*close wifi box*/
  vTaskDelay(50);
  // lv_obj_del(screenWifi);      /*cannot delete any object*/
  lv_disp_load_scr(screenMain);   /*go back to input screen*/
  lv_group_focus_obj(lv_obj_get_child(screenMain, VTBI_INDEX)); /*focus to input field*/
  enterClicked = false;
  inMsgbox = false;
  // pthread_mutex_unlock(&lvgl_mutex);
  vTaskDelay(50);                 /*avoid crashing, 30 should be enough*/
}

bool validate_keypad_inputs() {
  bool state = true;
  for (int i=0; i<3; i++){
    if (keypadInput[i] == -1) {
      state = false;
    }
  }
  if (dropFactor > 100) {
    state = false;
  }

  if (state) {
    /*get DR and display on top right widget*/
    uint16_t time = keypadInput[1]*60 + keypadInput[2];
    if (time == 0) {  // when user input 0 for time
      state = false;
      lv_label_set_text(lv_obj_get_child(lv_obj_get_child(screenMain, 6), 1), 
                            "Time should not be 0");
    } else {
      targetDripRate = keypadInput[0] * dropFactor / time;
      /*set the text on top right widget*/
      lv_obj_set_style_text_color(lv_obj_get_child(lv_obj_get_child(screenMain, 6), 1), 
                                  lv_color_hex(0x40ce00), LV_PART_MAIN);
      lv_label_set_text_fmt(lv_obj_get_child(lv_obj_get_child(screenMain, 6), 1), 
                            "Drip Rate: %d", targetDripRate);
    }
  }
  return state;
}