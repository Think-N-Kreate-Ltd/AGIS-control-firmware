#include <AGIS_Display.h>
#include <AGIS_Commons.h>
#include <AGIS_Utilities.h>

/*TFT_eSPI variables*/
TFT_eSPI tft_display = TFT_eSPI();

/*LVGL variables*/
lv_disp_draw_buf_t draw_buf;
lv_color_t color_buf[ TFT_WIDTH * TFT_HEIGHT / 5 ];

lv_obj_t * input_scr = NULL;
lv_obj_t * monitor_scr = NULL;
lv_indev_t * keypad_indev;
lv_group_t * input_grp;
lv_obj_t * derivedDripRateValue_label;
lv_obj_t * infusion_monitoring_table = NULL;

static lv_style_t style_radio;
static lv_style_t style_radio_chk;
static uint32_t active_radiobtn_id = 0;

/*Parsed user input variables*/
int32_t keypad_VTBI = -1;
int32_t keypad_totalTimeHour = -1;
int32_t keypad_totalTimeMinute = -1;
int32_t keypad_targetDripRate = -1;
int32_t keypad_dropFactor = -1;

bool keypad_check = false;

/*Infusion monitoring variables*/
lv_timer_t * infusion_monitoring_timer;
infusion_monitoring_data_handle_t infusion_monitoring_data_handle;

void display_init() {
  /*TFT display setup*/
  tft_display.init();
  tft_display.setRotation(1);  // view display horizontally

  /*Initialize LVGL*/
  lv_init();
  lv_disp_draw_buf_init( &draw_buf, color_buf, NULL, TFT_WIDTH * TFT_HEIGHT / 5 );

  /*Initialize LVGL display driver*/
  static lv_disp_drv_t disp_drv;
  lv_disp_drv_init( &disp_drv );
  /*Change the following line to your display resolution*/
  disp_drv.hor_res = TFT_HEIGHT;  // flipped since we use horizontal view
  disp_drv.ver_res = TFT_WIDTH;
  disp_drv.flush_cb = my_disp_flush;
  disp_drv.draw_buf = &draw_buf;
  lv_disp_drv_register( &disp_drv );

  /*Inittialize LVGL screens*/
  input_scr = lv_obj_create(NULL);

  /*Initialize LVGL input devices, i.e. keypad*/
  static lv_indev_drv_t keypad_drv;  // This needs to be static or global variable
  lv_indev_drv_init(&keypad_drv);
  keypad_drv.type = LV_INDEV_TYPE_KEYPAD;
  keypad_drv.read_cb = keypad_read;
  /*Register the driver in LVGL and save the created input device object*/
  keypad_indev = lv_indev_drv_register(&keypad_drv);

  /*Init LVGL groups*/
  input_grp = lv_group_create();
  lv_indev_set_group(keypad_indev, input_grp);

  /*Register LVGL timers*/
  infusion_monitoring_data_handle.numDrops_p = &numDrops;
  infusion_monitoring_data_handle.dripRate_p = &dripRate;
  infusion_monitoring_data_handle.infusedVolume_p = &infusedVolume;
  infusion_monitoring_data_handle.infusedTime_p = &infusedTime;
  infusion_monitoring_data_handle.infusionState_p = &infusionState;
  // Call every 500ms
  infusion_monitoring_timer = lv_timer_create(infusion_monitoring_cb, 500, NULL);
}

/* Display flushing */
void my_disp_flush( lv_disp_drv_t *disp, const lv_area_t *area, lv_color_t *color_p ) {
    uint32_t w = ( area->x2 - area->x1 + 1 );
    uint32_t h = ( area->y2 - area->y1 + 1 );

    tft_display.startWrite();
    tft_display.setAddrWindow( area->x1, area->y1, w, h );
    tft_display.pushColors( ( uint16_t * )&color_p->full, w * h, true );
    tft_display.endWrite();

    lv_disp_flush_ready( disp );
}

void input_screen(void) {
  /*Change the active screen's background color*/
  lv_obj_set_style_bg_color(lv_scr_act(), lv_color_hex(0xacacac), LV_PART_MAIN);

  /*Text area for `VTBI_ta`*/
  lv_obj_t * VTBI_ta = lv_textarea_create(lv_scr_act());
  lv_textarea_set_one_line(VTBI_ta, true);
  lv_obj_align(VTBI_ta, LV_ALIGN_TOP_LEFT, 5, 25);
  lv_obj_set_width(VTBI_ta, 80);
  lv_textarea_set_placeholder_text(VTBI_ta, "Pls input");
  static int lv_VTBI_id = LV_VTBI_ID;
  lv_obj_set_user_data(VTBI_ta, &lv_VTBI_id);
  lv_obj_add_event_cb(VTBI_ta, textarea_event_handler, LV_EVENT_ALL, VTBI_ta);
  lv_group_add_obj(input_grp, VTBI_ta);

  /*Labels for `VTBI_ta`*/
  lv_obj_t * VTBI_label = lv_label_create(lv_scr_act());
  lv_label_set_text(VTBI_label, "VTBI:");
  lv_obj_align_to(VTBI_label, VTBI_ta, LV_ALIGN_OUT_TOP_LEFT, 0, -5);

  lv_obj_t * mL_label = lv_label_create(lv_scr_act());
  lv_label_set_text(mL_label, "mL");
  lv_obj_align_to(mL_label, VTBI_ta, LV_ALIGN_OUT_RIGHT_MID, 5, 0);

  /*Text area for `totalTimeHour_ta`*/
  lv_obj_t * totalTimeHour_ta = lv_textarea_create(lv_scr_act());
  lv_textarea_set_one_line(totalTimeHour_ta, true);
  lv_obj_align_to(totalTimeHour_ta, VTBI_ta, LV_ALIGN_OUT_BOTTOM_LEFT, 0, 30); // vertical text area spacing
  lv_obj_set_width(totalTimeHour_ta, 80);
  lv_textarea_set_placeholder_text(totalTimeHour_ta, "Pls input");
  static int lv_totalTimeHour_id = LV_TOTAL_TIME_HOUR_ID;
  lv_obj_set_user_data(totalTimeHour_ta, &lv_totalTimeHour_id);
  lv_obj_add_event_cb(totalTimeHour_ta, textarea_event_handler, LV_EVENT_ALL, totalTimeHour_ta);
  lv_group_add_obj(input_grp, totalTimeHour_ta);

  /*Labels for `totalTimeHour_ta`*/
  lv_obj_t * totalTime_label = lv_label_create(lv_scr_act());
  lv_label_set_text(totalTime_label, "Total time:");
  lv_obj_align_to(totalTime_label, totalTimeHour_ta, LV_ALIGN_OUT_TOP_LEFT, 0, -5);

  lv_obj_t * totalTimeHour_label = lv_label_create(lv_scr_act());
  lv_label_set_text(totalTimeHour_label, "hours");
  lv_obj_align_to(totalTimeHour_label, totalTimeHour_ta, LV_ALIGN_OUT_RIGHT_MID, 5, 0);

  /*Text area for `totalTimeMinute_ta`*/
  lv_obj_t * totalTimeMinute_ta = lv_textarea_create(lv_scr_act());
  lv_textarea_set_one_line(totalTimeMinute_ta, true);
  lv_obj_align_to(totalTimeMinute_ta, totalTimeHour_label, LV_ALIGN_OUT_RIGHT_MID, 20, 0); // horizontal text area spacing
  lv_obj_set_width(totalTimeMinute_ta, 80);
  lv_textarea_set_placeholder_text(totalTimeMinute_ta, "Pls input");
  static int lv_totalTimeMinute_id = LV_TOTAL_TIME_MINUE_ID;
  lv_obj_set_user_data(totalTimeMinute_ta, &lv_totalTimeMinute_id);
  lv_obj_add_event_cb(totalTimeMinute_ta, textarea_event_handler, LV_EVENT_ALL, totalTimeMinute_ta);
  lv_group_add_obj(input_grp, totalTimeMinute_ta);

  /*Labels for `totalTimeMinute_ta`*/
  lv_obj_t * totalTimeMinute_label = lv_label_create(lv_scr_act());
  lv_label_set_text(totalTimeMinute_label, "minutes");
  lv_obj_align_to(totalTimeMinute_label, totalTimeMinute_ta, LV_ALIGN_OUT_RIGHT_MID, 5, 0);

  /*Radio buttons for drop factor*/
  /* The idea is to enable `LV_OBJ_FLAG_EVENT_BUBBLE` on checkboxes and process the
    * `LV_EVENT_CLICKED` on the container.
    * A variable is passed as event user data where the index of the active
    * radiobutton is saved */

  lv_style_init(&style_radio);
  lv_style_set_radius(&style_radio, LV_RADIUS_CIRCLE);

  lv_style_init(&style_radio_chk);
  lv_style_set_bg_img_src(&style_radio_chk, NULL);

  /*Grid of radio buttons*/
  lv_obj_t * dropFactor_cont = lv_obj_create(lv_scr_act());
  static lv_coord_t col_dsc[] = {130, 130, LV_GRID_TEMPLATE_LAST};
  static lv_coord_t row_dsc[] = {20, 20, LV_GRID_TEMPLATE_LAST};

  lv_obj_set_style_grid_column_dsc_array(dropFactor_cont, col_dsc, 0);
  lv_obj_set_style_grid_row_dsc_array(dropFactor_cont, row_dsc, 0);
  lv_obj_set_size(dropFactor_cont, lv_pct(95), lv_pct(32));
  lv_obj_align_to(dropFactor_cont, totalTimeHour_ta, LV_ALIGN_OUT_BOTTOM_LEFT, 0, 30);
  lv_obj_set_layout(dropFactor_cont, LV_LAYOUT_GRID);

  uint32_t i;
  char text_buf[32];
  for(i = 0; i < 4; i++) {

    switch (i) {
    case 0:
      lv_snprintf(text_buf, sizeof(text_buf), "10 drops/mL");
      break;
    case 1:
      lv_snprintf(text_buf, sizeof(text_buf), "15 drops/mL");
      break;
    case 2:
      lv_snprintf(text_buf, sizeof(text_buf), "20 drops/mL");
      break;
    case 3:
      lv_snprintf(text_buf, sizeof(text_buf), "60 drops/mL");
      break;
    default:
      break;
    }

    radiobutton_create(dropFactor_cont, text_buf, i);
  }

  /*Labels for `dropFactor_cont`*/
  lv_obj_t * dropFactor_label = lv_label_create(lv_scr_act());
  lv_label_set_text(dropFactor_label, "Drop factor:");
  lv_obj_align_to(dropFactor_label, dropFactor_cont, LV_ALIGN_OUT_TOP_LEFT, 0, -5);
  lv_obj_add_event_cb(dropFactor_cont, radio_event_handler, LV_EVENT_CLICKED, &active_radiobtn_id);

  /*Widget to show derived drip rate*/
  lv_obj_t * derivedDripRate_cont;
  derivedDripRate_cont = lv_obj_create(lv_scr_act());
  lv_obj_set_style_border_color(derivedDripRate_cont, lv_color_hex(0x5b5b5b), LV_PART_MAIN);
  lv_obj_set_style_radius(derivedDripRate_cont, 0x00, LV_PART_MAIN);
  lv_obj_set_size(derivedDripRate_cont, 180, 70);
  lv_obj_align(derivedDripRate_cont, LV_ALIGN_TOP_RIGHT, -5, 5);

  /*label*/
  lv_obj_t * derivedDripRate_label = lv_label_create(lv_scr_act());
  lv_label_set_text(derivedDripRate_label, "Drip rate (drops/min): ");
  lv_obj_align_to(derivedDripRate_label, derivedDripRate_cont, LV_ALIGN_TOP_LEFT, -5, 0);

  derivedDripRateValue_label = lv_label_create(lv_scr_act());
  lv_label_set_text(derivedDripRateValue_label, "Please fill in all inputs");
  lv_obj_set_style_text_color(derivedDripRateValue_label, lv_color_hex(0xcc0000), LV_PART_MAIN);
  lv_obj_align_to(derivedDripRateValue_label, derivedDripRate_label, LV_ALIGN_LEFT_MID, 0, 20);
}

static void textarea_event_handler(lv_event_t * event) {
  // NOTE: do not print anything inside this function
  if(event->code == LV_EVENT_KEY && lv_indev_get_key(keypad_indev) == LV_KEY_ENTER) {
    lv_obj_t * ta = lv_event_get_target(event);

    // Parse the inputted data
    const char * data_buf = NULL;
    data_buf = lv_textarea_get_text(ta);

    // Identify which input
    static int32_t * selected_input;
    int ta_id = *(int *) lv_obj_get_user_data(ta);
    if (ta_id == LV_VTBI_ID) {
      selected_input = &keypad_VTBI;
    }
    else if (ta_id == LV_TOTAL_TIME_HOUR_ID) {
      selected_input = &keypad_totalTimeHour;
    }
    else if (ta_id == LV_TOTAL_TIME_MINUE_ID) {
      selected_input = &keypad_totalTimeMinute;
    }

    if (*data_buf) {  // only parse when we receive input
      *selected_input = atoi(data_buf);
    }
    else {
      // reset value of that input
      *selected_input = -1;
    }

    // Stop cursor blinking
    lv_obj_clear_state(ta, LV_STATE_FOCUSED);

    // Call the function to validate keypad inputs
    keypad_check = validate_keypad_inputs();
  }
}

static void radiobutton_create(lv_obj_t * parent, const char * txt, uint32_t i) {
  lv_obj_t * obj = lv_checkbox_create(parent);
  lv_checkbox_set_text(obj, txt);
  lv_obj_add_flag(obj, LV_OBJ_FLAG_EVENT_BUBBLE);
  lv_obj_add_style(obj, &style_radio, LV_PART_INDICATOR);
  lv_obj_add_style(obj, &style_radio_chk, LV_PART_INDICATOR | LV_STATE_CHECKED);
  lv_style_set_pad_all(&style_radio, LV_STATE_DEFAULT);

  // obj = lv_checkbox_create(dropFactor_cont);
  /*Stretch the cell horizontally and vertically too
  *Set span to 1 to make the cell 1 column/row sized*/
  uint8_t col = i % 2;
  uint8_t row = i / 2;
  lv_obj_set_grid_cell(obj, LV_GRID_ALIGN_SPACE_EVENLY, col, 1,
                        LV_GRID_ALIGN_SPACE_EVENLY, row, 1);
  lv_group_add_obj(input_grp, obj);
}

static void radio_event_handler(lv_event_t * event) {
  // NOTE: do not print anything inside this function
  uint32_t * active_id = (uint32_t *)lv_event_get_user_data(event);
  lv_obj_t * cont = lv_event_get_current_target(event);
  lv_obj_t * act_cb = lv_event_get_target(event);
  lv_obj_t * old_cb = lv_obj_get_child(cont, *active_id);

  /*Do nothing if the container was clicked*/
  if(act_cb == cont) return;

  lv_obj_clear_state(old_cb, LV_STATE_CHECKED);   /*Uncheck the previous radio button*/
  lv_obj_add_state(act_cb, LV_STATE_CHECKED);     /*Uncheck the current radio button*/

  *active_id = lv_obj_get_index(act_cb);

  // Parse selected radio button
  if (*active_id == LV_DROP_FACTOR_10_ID) {
    keypad_dropFactor = 10;
  }
  else if (*active_id == LV_DROP_FACTOR_15_ID) {
    keypad_dropFactor = 15;
  }
  else if (*active_id == LV_DROP_FACTOR_20_ID) {
    keypad_dropFactor = 20;
  }
  else if (*active_id == LV_DROP_FACTOR_60_ID) {
    keypad_dropFactor = 60;
  }
  
  // Call the function to validate keypad inputs
  keypad_check = validate_keypad_inputs();
}

void monitor_screen() {
  /*Use labels*/
  // lv_obj_t *numOfDrops_label = lv_label_create(lv_scr_act());
  // lv_label_set_text(numOfDrops_label, "No. of drops:");
  // lv_obj_align(numOfDrops_label, LV_ALIGN_TOP_LEFT, 0, 0 );

  // lv_obj_t *dripRate_label = lv_label_create(lv_scr_act());
  // lv_label_set_text(dripRate_label, "Drip rate (drops/min):");
  // lv_obj_align_to(dripRate_label, numOfDrops_label, LV_ALIGN_TOP_LEFT, 0, 20);

  // lv_obj_t *infusedVolume_label = lv_label_create(lv_scr_act());
  // lv_label_set_text(infusedVolume_label, "Infused volume (mL):");
  // lv_obj_align_to(infusedVolume_label,dripRate_label, LV_ALIGN_TOP_LEFT, 0, 20);

  /*Use table*/
  infusion_monitoring_table = lv_table_create(lv_scr_act());
  lv_table_set_col_cnt(infusion_monitoring_table, 2);
  lv_table_set_row_cnt(infusion_monitoring_table, 5);
  lv_obj_align(infusion_monitoring_table, LV_ALIGN_CENTER, 0, 0);
  // lv_obj_set_style_border_color(infusion_monitoring_table, lv_color_hex(0x5b5b5b), LV_PART_MAIN);
  lv_obj_set_style_border_opa(infusion_monitoring_table, LV_OPA_TRANSP, LV_PART_MAIN);
  lv_obj_set_style_line_color(infusion_monitoring_table, lv_color_hex(0x5b5b5b), LV_PART_MAIN);
  lv_table_set_col_width(infusion_monitoring_table, 0, 180);

  /*Fill the first column*/
  lv_table_set_cell_value(infusion_monitoring_table, 0, 0, "No. of drops:");
  lv_table_set_cell_value(infusion_monitoring_table, 1, 0, "Drip rate (drops/min):");
  lv_table_set_cell_value(infusion_monitoring_table, 2, 0, "Infused volume (mL):");
  lv_table_set_cell_value(infusion_monitoring_table, 3, 0, "Infused time:");
  lv_table_set_cell_value(infusion_monitoring_table, 4, 0, "Infusion state:");

  /*Fill the second column*/
  lv_table_set_cell_value(infusion_monitoring_table, 0, 1, "Not started");
  lv_table_set_cell_value(infusion_monitoring_table, 1, 1, "Not started");
  lv_table_set_cell_value(infusion_monitoring_table, 2, 1, "Not started");
  lv_table_set_cell_value(infusion_monitoring_table, 3, 1, "Not started");
  lv_table_set_cell_value(infusion_monitoring_table, 4, 1, "Not started");

}

static void keypad_read(lv_indev_drv_t * drv, lv_indev_data_t * data){
  // NOTE: do not print anything inside this function
  uint32_t key = keypad.getKey();
  if(key) {
    if (key == 'E') {
      data->key = LV_KEY_ENTER;
    }
    else if (key == 'C') {
      data->key = LV_KEY_ESC;
    }
    else if (key == 'L') {
      data->key = LV_KEY_LEFT;
    }
    else if (key == 'R') {
      data->key = LV_KEY_RIGHT;
    }
    else if (key == 'U') {
      data->key = LV_KEY_PREV;
    }
    else if (key == 'D') {
      data->key = LV_KEY_NEXT;
    }
    else if (key == '#') {
      data->key = LV_KEY_BACKSPACE;
    }
    else if (key == '*') {
      data->key = LV_KEY_DEL;
    }
    else if (key == 'F') {
      // Test: display another screen
      if (monitor_scr == NULL) {
        monitor_scr = lv_obj_create(NULL);
        lv_scr_load(monitor_scr);
        monitor_screen();
      }

      // TODO: check how to properly delete unused screen?
      if (input_scr != NULL) {
        lv_obj_del(input_scr);
        input_scr = NULL;
      }
    }
    // else if (key == 'G') {
    //   // TODO: send the keypad inputs to autoControl
    //   // and display the monitoring info
    // }
    else {
      data->key = key;// possible BUG due to conversion from char to uint32_t
    }

    data->state = LV_INDEV_STATE_PRESSED;
  }
  else {
    data->state = LV_INDEV_STATE_RELEASED;
  }
}

// #if LV_USE_LOG != 0
// /* Serial debugging */
// void my_print( lv_log_level_t level, const char * buf )
// {
//     LV_UNUSED(level);
//     Serial.flush();
// }
// #endif

// Check if all inputs are provided,
// and satisfy the requirements
bool validate_keypad_inputs() {
  if ((keypad_VTBI != -1) &&
      (keypad_totalTimeHour != -1) &&
      (keypad_totalTimeMinute != -1) &&
      (keypad_dropFactor != -1)) {
    
    // TODO: some checks to verify that inputs are valid

    keypad_targetDripRate = calculate_drip_rate(
        keypad_VTBI, keypad_totalTimeHour * 60 + keypad_totalTimeMinute,
        keypad_dropFactor);

    // Update text on the display
    char buf[20];
    sprintf(buf, "%d", keypad_targetDripRate);
    lv_obj_set_style_text_color(derivedDripRateValue_label, lv_color_hex(0x40ce00), LV_PART_MAIN);
    lv_label_set_text(derivedDripRateValue_label, buf);

    // Submit verified inputs to autoControl
    targetVTBI = (unsigned int)keypad_VTBI;
    targetDripRate = (unsigned int)keypad_targetDripRate; 
    targetTotalTime = keypad_totalTimeHour * 3600 +
                      keypad_totalTimeMinute * 60;
    targetNumDrops = targetVTBI / (1.0f / keypad_dropFactor); // TODO: use rounded function

    return true;
  }
  else {
    lv_obj_set_style_text_color(derivedDripRateValue_label, lv_color_hex(0xcc0000), LV_PART_MAIN);
    lv_label_set_text(derivedDripRateValue_label, "Please fill in all inputs");
    return false;
  }
}

/**
 * Calculate the drip rate based on the inputs, rounded to the nearest integer
 * @param volume Volume to be infused, in mL
 * @param time Time of infusion, in minutes
 * @param dropFactor Drop factor of the tubbing set
 * @return Calculated drip rate, in drops/min
 */
int32_t calculate_drip_rate(int32_t volume, int32_t time, int32_t dropFactor) {
  int32_t numerator = volume * dropFactor;
  int32_t denominator = time;

  return (numerator + denominator / 2) / denominator;
}

/**
 * The callback used to update the infusion monitoring data
 * @param timer The pointer to the calling LVGL timer 
 */
void infusion_monitoring_cb(lv_timer_t * timer) {

  if (infusion_monitoring_table != NULL) {
    /*Update LVGL table cells*/
    char numDrops_buf[20];
    sprintf(numDrops_buf, "%d", *(infusion_monitoring_data_handle.numDrops_p));
    lv_table_set_cell_value(infusion_monitoring_table, 0, 1, numDrops_buf);

    char dripRate_buf[20];
    sprintf(dripRate_buf, "%d", *(infusion_monitoring_data_handle.dripRate_p));
    lv_table_set_cell_value(infusion_monitoring_table, 1, 1, dripRate_buf);

    char infusedVolume_buf[20];
    sprintf(infusedVolume_buf, "%.2f", *(infusion_monitoring_data_handle.infusedVolume_p));
    lv_table_set_cell_value(infusion_monitoring_table, 2, 1, infusedVolume_buf);

    char infusedTime_buf[20];
    sprintf(infusedTime_buf, "%d", *(infusion_monitoring_data_handle.infusedTime_p));
    lv_table_set_cell_value(infusion_monitoring_table, 3, 1, infusedTime_buf);

    const char *infusionState_buf =
        getInfusionState(*(infusion_monitoring_data_handle.infusionState_p));
    lv_table_set_cell_value(infusion_monitoring_table, 4, 1, infusionState_buf);
  }
}