#include "ui.h"
#include "../config.h"
#include "../servo_ctrl.h"
#include "../serial_parser.h"
#include "../storage.h"
#include <cstdio>

// ================================================================
// Sweep screen - one nozzle at a time with toggle
// ================================================================

// Per-nozzle settings (kept in memory, synced to sliders on toggle)
static int  sweepMin[2]   = {0, 0};
static int  sweepMax[2]   = {180, 180};
static uint8_t sweepSpd[2] = {1, 1};   // 0=slow, 1=med, 2=fast
static int  activeNozzle   = 0;         // 0 or 1
static bool sweepRunning   = false;

// UI objects
static lv_obj_t *nozzleSel    = nullptr;
static lv_obj_t *sliderMin    = nullptr;
static lv_obj_t *sliderMax    = nullptr;
static lv_obj_t *lblMinVal    = nullptr;
static lv_obj_t *lblMaxVal    = nullptr;
static lv_obj_t *speedBtns    = nullptr;
static lv_obj_t *startBtn     = nullptr;
static lv_obj_t *startBtnLbl  = nullptr;
static lv_obj_t *statusLed    = nullptr;
static lv_obj_t *rangeLabel   = nullptr;

// Speed button map (static lifetime required by LVGL)
static const char *speedMap[] = {"Slow", "Med", "Fast", ""};
static const char *nozzleMap[] = {"NOZZLE 1", "NOZZLE 2", ""};

// ----------------------------------------------------------------
static void updateSliderDisplay() {
    if (!sliderMin || !sliderMax) return;
    lv_slider_set_value(sliderMin, sweepMin[activeNozzle], LV_ANIM_OFF);
    lv_slider_set_value(sliderMax, sweepMax[activeNozzle], LV_ANIM_OFF);

    char buf[8];
    snprintf(buf, sizeof(buf), "%d\xc2\xb0", sweepMin[activeNozzle]);
    lv_label_set_text(lblMinVal, buf);
    snprintf(buf, sizeof(buf), "%d\xc2\xb0", sweepMax[activeNozzle]);
    lv_label_set_text(lblMaxVal, buf);

    // Update range summary
    char rbuf[32];
    snprintf(rbuf, sizeof(rbuf), "%d\xc2\xb0 \xe2\x80\x93 %d\xc2\xb0",
             sweepMin[activeNozzle], sweepMax[activeNozzle]);
    if (rangeLabel) lv_label_set_text(rangeLabel, rbuf);
}

static void updateSpeedDisplay() {
    if (!speedBtns) return;
    lv_btnmatrix_set_btn_ctrl(speedBtns, sweepSpd[activeNozzle],
                               LV_BTNMATRIX_CTRL_CHECKED);
}

static uint8_t getSpeedFromBtns() {
    uint16_t id = lv_btnmatrix_get_selected_btn(speedBtns);
    if (id == 0) return 0;
    if (id == 2) return 2;
    return 1;
}

static void saveSweepSettings() {
    int tool = SerialParser::getActiveTool();
    if (tool < 0) return;
    ToolData td;
    Storage::load(tool, td);
    td.sweepMin1   = sweepMin[0];
    td.sweepMax1   = sweepMax[0];
    td.sweepMin2   = sweepMin[1];
    td.sweepMax2   = sweepMax[1];
    td.sweepSpeed1 = sweepSpd[0];
    td.sweepSpeed2 = sweepSpd[1];
    td.servo1Angle = ServoCtrl::getAngle1();
    td.servo2Angle = ServoCtrl::getAngle2();
    Storage::save(tool, td);
}

// ================================================================
// Build the sweep screen
// ================================================================
void UI::createScreenSweep(lv_obj_t *parent) {
    // ---- Title ----
    lv_obj_t *title = lv_label_create(parent);
    lv_label_set_text(title, "SWEEP MODE");
    lv_obj_set_style_text_color(title, lv_color_hex(COLOR_HIGHLIGHT), 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_16, 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 0);

    // ---- Nozzle selector ----
    nozzleSel = lv_btnmatrix_create(parent);
    lv_btnmatrix_set_map(nozzleSel, nozzleMap);
    lv_btnmatrix_set_btn_ctrl_all(nozzleSel, LV_BTNMATRIX_CTRL_CHECKABLE);
    lv_btnmatrix_set_one_checked(nozzleSel, true);
    lv_btnmatrix_set_btn_ctrl(nozzleSel, 0, LV_BTNMATRIX_CTRL_CHECKED);
    lv_obj_set_size(nozzleSel, 224, 34);
    lv_obj_align(nozzleSel, LV_ALIGN_TOP_MID, 0, 22);

    lv_obj_set_style_bg_color(nozzleSel, lv_color_hex(COLOR_PANEL), LV_PART_MAIN);
    lv_obj_set_style_bg_color(nozzleSel, lv_color_hex(COLOR_ACCENT), LV_PART_ITEMS);
    lv_obj_set_style_bg_color(nozzleSel, lv_color_hex(COLOR_HIGHLIGHT),
                               LV_PART_ITEMS | LV_STATE_CHECKED);
    lv_obj_set_style_text_color(nozzleSel, lv_color_hex(COLOR_TEXT_PRI), LV_PART_ITEMS);
    lv_obj_set_style_text_decor(nozzleSel, LV_TEXT_DECOR_NONE, LV_PART_ITEMS);
    lv_obj_set_style_text_font(nozzleSel, &lv_font_montserrat_14, LV_PART_ITEMS);
    lv_obj_set_style_border_width(nozzleSel, 0, LV_PART_ITEMS);
    lv_obj_set_style_border_width(nozzleSel, 2, LV_PART_ITEMS | LV_STATE_CHECKED);
    lv_obj_set_style_border_side(nozzleSel, LV_BORDER_SIDE_BOTTOM, LV_PART_ITEMS | LV_STATE_CHECKED);
    lv_obj_set_style_border_color(nozzleSel, lv_color_hex(COLOR_TEXT_PRI), LV_PART_ITEMS | LV_STATE_CHECKED);
    lv_obj_set_style_radius(nozzleSel, 6, LV_PART_ITEMS);

    lv_obj_add_event_cb(nozzleSel, [](lv_event_t *e) {
        lv_obj_t *obj = lv_event_get_target(e);
        uint16_t id = lv_btnmatrix_get_selected_btn(obj);
        if (id > 1) return;
        // Save current nozzle settings before switching
        sweepMin[activeNozzle] = lv_slider_get_value(sliderMin);
        sweepMax[activeNozzle] = lv_slider_get_value(sliderMax);
        sweepSpd[activeNozzle] = getSpeedFromBtns();
        // Switch
        activeNozzle = id;
        updateSliderDisplay();
        updateSpeedDisplay();
    }, LV_EVENT_VALUE_CHANGED, NULL);

    // ---- Settings panel ----
    lv_obj_t *panel = lv_obj_create(parent);
    lv_obj_set_size(panel, 228, 120);
    lv_obj_align(panel, LV_ALIGN_TOP_MID, 0, 60);
    lv_obj_set_style_bg_color(panel, lv_color_hex(COLOR_PANEL), 0);
    lv_obj_set_style_radius(panel, 10, 0);
    lv_obj_set_style_border_width(panel, 0, 0);
    lv_obj_set_style_pad_all(panel, 8, 0);
    lv_obj_clear_flag(panel, LV_OBJ_FLAG_SCROLLABLE);

    // Range summary label
    rangeLabel = lv_label_create(panel);
    lv_obj_set_style_text_color(rangeLabel, lv_color_hex(COLOR_TEXT_PRI), 0);
    lv_obj_set_style_text_font(rangeLabel, &lv_font_montserrat_14, 0);
    lv_obj_align(rangeLabel, LV_ALIGN_TOP_MID, 0, 0);

    // Status LED
    statusLed = lv_led_create(panel);
    lv_obj_set_size(statusLed, 10, 10);
    lv_led_set_color(statusLed, lv_color_hex(COLOR_HIGHLIGHT));
    lv_led_off(statusLed);
    lv_obj_align(statusLed, LV_ALIGN_TOP_RIGHT, 0, 2);

    // Min slider row
    lv_obj_t *minTitle = lv_label_create(panel);
    lv_label_set_text(minTitle, "MIN");
    lv_obj_set_style_text_color(minTitle, lv_color_hex(COLOR_TEXT_SEC), 0);
    lv_obj_set_style_text_font(minTitle, &lv_font_montserrat_12, 0);
    lv_obj_set_pos(minTitle, 0, 20);

    sliderMin = lv_slider_create(panel);
    lv_slider_set_range(sliderMin, SERVO_MIN_ANGLE, SERVO_MAX_ANGLE);
    lv_obj_set_size(sliderMin, 140, 12);
    lv_obj_set_pos(sliderMin, 32, 24);
    lv_obj_set_style_bg_color(sliderMin, lv_color_hex(COLOR_PANEL_LITE), LV_PART_MAIN);
    lv_obj_set_style_bg_color(sliderMin, lv_color_hex(COLOR_HIGHLIGHT), LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(sliderMin, lv_color_hex(COLOR_HIGHLIGHT), LV_PART_KNOB);
    lv_obj_set_style_pad_all(sliderMin, 4, LV_PART_KNOB);

    lblMinVal = lv_label_create(panel);
    lv_obj_set_style_text_color(lblMinVal, lv_color_hex(COLOR_TEXT_PRI), 0);
    lv_obj_set_style_text_font(lblMinVal, &lv_font_montserrat_14, 0);
    lv_obj_set_pos(lblMinVal, 180, 20);

    // Max slider row
    lv_obj_t *maxTitle = lv_label_create(panel);
    lv_label_set_text(maxTitle, "MAX");
    lv_obj_set_style_text_color(maxTitle, lv_color_hex(COLOR_TEXT_SEC), 0);
    lv_obj_set_style_text_font(maxTitle, &lv_font_montserrat_12, 0);
    lv_obj_set_pos(maxTitle, 0, 44);

    sliderMax = lv_slider_create(panel);
    lv_slider_set_range(sliderMax, SERVO_MIN_ANGLE, SERVO_MAX_ANGLE);
    lv_obj_set_size(sliderMax, 140, 12);
    lv_obj_set_pos(sliderMax, 32, 48);
    lv_obj_set_style_bg_color(sliderMax, lv_color_hex(COLOR_PANEL_LITE), LV_PART_MAIN);
    lv_obj_set_style_bg_color(sliderMax, lv_color_hex(COLOR_HIGHLIGHT), LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(sliderMax, lv_color_hex(COLOR_HIGHLIGHT), LV_PART_KNOB);
    lv_obj_set_style_pad_all(sliderMax, 4, LV_PART_KNOB);

    lblMaxVal = lv_label_create(panel);
    lv_obj_set_style_text_color(lblMaxVal, lv_color_hex(COLOR_TEXT_PRI), 0);
    lv_obj_set_style_text_font(lblMaxVal, &lv_font_montserrat_14, 0);
    lv_obj_set_pos(lblMaxVal, 180, 44);

    // Speed selector
    lv_obj_t *spdTitle = lv_label_create(panel);
    lv_label_set_text(spdTitle, "SPEED");
    lv_obj_set_style_text_color(spdTitle, lv_color_hex(COLOR_TEXT_SEC), 0);
    lv_obj_set_style_text_font(spdTitle, &lv_font_montserrat_12, 0);
    lv_obj_align(spdTitle, LV_ALIGN_BOTTOM_LEFT, 0, -4);

    speedBtns = lv_btnmatrix_create(panel);
    lv_btnmatrix_set_map(speedBtns, speedMap);
    lv_btnmatrix_set_btn_ctrl_all(speedBtns, LV_BTNMATRIX_CTRL_CHECKABLE);
    lv_btnmatrix_set_one_checked(speedBtns, true);
    lv_btnmatrix_set_btn_ctrl(speedBtns, 1, LV_BTNMATRIX_CTRL_CHECKED);
    lv_obj_set_size(speedBtns, 150, 28);
    lv_obj_align(speedBtns, LV_ALIGN_BOTTOM_RIGHT, 0, 0);
    lv_obj_set_style_bg_color(speedBtns, lv_color_hex(COLOR_PANEL), LV_PART_MAIN);
    lv_obj_set_style_bg_color(speedBtns, lv_color_hex(COLOR_ACCENT), LV_PART_ITEMS);
    lv_obj_set_style_bg_color(speedBtns, lv_color_hex(COLOR_HIGHLIGHT),
                               LV_PART_ITEMS | LV_STATE_CHECKED);
    lv_obj_set_style_text_color(speedBtns, lv_color_hex(COLOR_TEXT_PRI), LV_PART_ITEMS);
    lv_obj_set_style_text_decor(speedBtns, LV_TEXT_DECOR_NONE, LV_PART_ITEMS);
    lv_obj_set_style_text_font(speedBtns, &lv_font_montserrat_12, LV_PART_ITEMS);
    lv_obj_set_style_border_width(speedBtns, 0, LV_PART_ITEMS);
    lv_obj_set_style_border_width(speedBtns, 2, LV_PART_ITEMS | LV_STATE_CHECKED);
    lv_obj_set_style_border_side(speedBtns, LV_BORDER_SIDE_BOTTOM, LV_PART_ITEMS | LV_STATE_CHECKED);
    lv_obj_set_style_border_color(speedBtns, lv_color_hex(COLOR_TEXT_PRI), LV_PART_ITEMS | LV_STATE_CHECKED);
    lv_obj_set_style_radius(speedBtns, 4, LV_PART_ITEMS);

    // Slider callbacks
    lv_obj_add_event_cb(sliderMin, [](lv_event_t *e) {
        int val = lv_slider_get_value(sliderMin);
        sweepMin[activeNozzle] = val;
        char buf[8];
        snprintf(buf, sizeof(buf), "%d\xc2\xb0", val);
        lv_label_set_text(lblMinVal, buf);
        // Update range label
        char rbuf[32];
        snprintf(rbuf, sizeof(rbuf), "%d\xc2\xb0 \xe2\x80\x93 %d\xc2\xb0",
                 sweepMin[activeNozzle], sweepMax[activeNozzle]);
        if (rangeLabel) lv_label_set_text(rangeLabel, rbuf);
    }, LV_EVENT_VALUE_CHANGED, NULL);

    lv_obj_add_event_cb(sliderMax, [](lv_event_t *e) {
        int val = lv_slider_get_value(sliderMax);
        sweepMax[activeNozzle] = val;
        char buf[8];
        snprintf(buf, sizeof(buf), "%d\xc2\xb0", val);
        lv_label_set_text(lblMaxVal, buf);
        char rbuf[32];
        snprintf(rbuf, sizeof(rbuf), "%d\xc2\xb0 \xe2\x80\x93 %d\xc2\xb0",
                 sweepMin[activeNozzle], sweepMax[activeNozzle]);
        if (rangeLabel) lv_label_set_text(rangeLabel, rbuf);
    }, LV_EVENT_VALUE_CHANGED, NULL);

    // Speed callback
    lv_obj_add_event_cb(speedBtns, [](lv_event_t *e) {
        sweepSpd[activeNozzle] = getSpeedFromBtns();
    }, LV_EVENT_VALUE_CHANGED, NULL);

    // Initialize display
    updateSliderDisplay();
    updateSpeedDisplay();

    // ---- START / STOP button ----
    startBtn = lv_btn_create(parent);
    lv_obj_set_size(startBtn, 224, 44);
    lv_obj_align(startBtn, LV_ALIGN_BOTTOM_MID, 0, -2);
    lv_obj_set_style_bg_color(startBtn, lv_color_hex(COLOR_HIGHLIGHT), 0);
    lv_obj_set_style_bg_color(startBtn, lv_color_hex(0x12a37e), LV_STATE_PRESSED);
    lv_obj_set_style_radius(startBtn, 10, 0);
    lv_obj_set_style_shadow_width(startBtn, 10, 0);
    lv_obj_set_style_shadow_color(startBtn, lv_color_hex(0x002020), 0);

    startBtnLbl = lv_label_create(startBtn);
    lv_label_set_text(startBtnLbl, LV_SYMBOL_PLAY "  START SWEEP");
    lv_obj_set_style_text_font(startBtnLbl, &lv_font_montserrat_16, 0);
    lv_obj_center(startBtnLbl);

    lv_obj_add_event_cb(startBtn, [](lv_event_t *e) {
        // Capture current UI state before starting
        sweepMin[activeNozzle] = lv_slider_get_value(sliderMin);
        sweepMax[activeNozzle] = lv_slider_get_value(sliderMax);
        sweepSpd[activeNozzle] = getSpeedFromBtns();

        if (!sweepRunning) {
            ServoCtrl::startSweep(0, sweepMin[0], sweepMax[0], sweepSpd[0]);
            ServoCtrl::startSweep(1, sweepMin[1], sweepMax[1], sweepSpd[1]);
            sweepRunning = true;

            lv_label_set_text(startBtnLbl, LV_SYMBOL_STOP "  STOP SWEEP");
            lv_obj_set_style_bg_color(startBtn, lv_color_hex(COLOR_WARNING), 0);
            lv_obj_set_style_bg_color(startBtn, lv_color_hex(0xc03050), LV_STATE_PRESSED);
            if (statusLed) lv_led_on(statusLed);

            saveSweepSettings();
            UI::setStatusText("Sweeping...");
        } else {
            ServoCtrl::stopAllSweeps();
            sweepRunning = false;

            lv_label_set_text(startBtnLbl, LV_SYMBOL_PLAY "  START SWEEP");
            lv_obj_set_style_bg_color(startBtn, lv_color_hex(COLOR_HIGHLIGHT), 0);
            lv_obj_set_style_bg_color(startBtn, lv_color_hex(0x12a37e), LV_STATE_PRESSED);
            if (statusLed) lv_led_off(statusLed);

            UI::setStatusText("Sweep Stopped");
        }
    }, LV_EVENT_CLICKED, NULL);
}
