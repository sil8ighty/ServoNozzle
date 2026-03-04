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
static int  activeNozzle   = 0;         // 0 or 1
static bool sweepRunning   = false;

// UI objects
static lv_obj_t *nozzleSel    = nullptr;
static lv_obj_t *sliderMin    = nullptr;
static lv_obj_t *sliderMax    = nullptr;
static lv_obj_t *lblMinVal    = nullptr;
static lv_obj_t *lblMaxVal    = nullptr;
static lv_obj_t *startBtn     = nullptr;
static lv_obj_t *startBtnLbl  = nullptr;
static lv_obj_t *statusLed    = nullptr;
static lv_obj_t *rangeLabel   = nullptr;

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

static void saveSweepSettings() {
    int tool = SerialParser::getActiveTool();
    if (tool < 0) return;
    ToolData td;
    Storage::load(tool, td);
    td.sweepMin1   = sweepMin[0];
    td.sweepMax1   = sweepMax[0];
    td.sweepMin2   = sweepMin[1];
    td.sweepMax2   = sweepMax[1];
    td.servo1Angle = ServoCtrl::getAngle1();
    td.servo2Angle = ServoCtrl::getAngle2();
    Storage::save(tool, td);
}

// Layout budget (parent inner area): 228 x 232
//   y=0:   Title (18px)
//   y=20:  Nozzle selector 34px -> bottom 54
//   y=58:  Settings panel 130px -> bottom 188
//   BOTTOM-2: Start button 42px -> top 188
//
// Panel inner (130 - 16 pad = 114px):
//   y=0:   Range summary (20px font) + status LED
//   y=26:  "MIN ANGLE" label + value       (14px)
//   y=42:  Min slider full width           (14px)
//   y=66:  "MAX ANGLE" label + value       (14px)
//   y=82:  Max slider full width           (14px)

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
    UI::styleBtnMatrix(nozzleSel);
    lv_obj_set_size(nozzleSel, 224, 34);
    lv_obj_align(nozzleSel, LV_ALIGN_TOP_MID, 0, 20);

    lv_obj_add_event_cb(nozzleSel, [](lv_event_t *e) {
        lv_obj_t *obj = lv_event_get_target(e);
        uint16_t id = lv_btnmatrix_get_selected_btn(obj);
        if (id > 1) return;
        sweepMin[activeNozzle] = lv_slider_get_value(sliderMin);
        sweepMax[activeNozzle] = lv_slider_get_value(sliderMax);
        activeNozzle = id;
        updateSliderDisplay();
    }, LV_EVENT_VALUE_CHANGED, NULL);

    // ---- Settings panel ----
    lv_obj_t *panel = lv_obj_create(parent);
    lv_obj_set_size(panel, 228, 130);
    lv_obj_align(panel, LV_ALIGN_TOP_MID, 0, 58);
    lv_obj_set_style_bg_color(panel, lv_color_hex(COLOR_PANEL), 0);
    lv_obj_set_style_radius(panel, 10, 0);
    lv_obj_set_style_border_width(panel, 0, 0);
    lv_obj_set_style_pad_all(panel, 8, 0);
    lv_obj_clear_flag(panel, LV_OBJ_FLAG_SCROLLABLE);

    // Range summary label (large, centered)
    rangeLabel = lv_label_create(panel);
    lv_obj_set_style_text_color(rangeLabel, lv_color_hex(COLOR_TEXT_PRI), 0);
    lv_obj_set_style_text_font(rangeLabel, &lv_font_montserrat_20, 0);
    lv_obj_align(rangeLabel, LV_ALIGN_TOP_MID, 0, 0);

    // Status LED
    statusLed = lv_led_create(panel);
    lv_obj_set_size(statusLed, 10, 10);
    lv_led_set_color(statusLed, lv_color_hex(COLOR_HIGHLIGHT));
    lv_led_off(statusLed);
    lv_obj_align(statusLed, LV_ALIGN_TOP_RIGHT, 0, 6);

    // ---- MIN row ----
    lv_obj_t *minTitle = lv_label_create(panel);
    lv_label_set_text(minTitle, "MIN ANGLE");
    lv_obj_set_style_text_color(minTitle, lv_color_hex(COLOR_TEXT_SEC), 0);
    lv_obj_set_style_text_font(minTitle, &lv_font_montserrat_12, 0);
    lv_obj_set_pos(minTitle, 0, 26);

    lblMinVal = lv_label_create(panel);
    lv_obj_set_style_text_color(lblMinVal, lv_color_hex(COLOR_HIGHLIGHT), 0);
    lv_obj_set_style_text_font(lblMinVal, &lv_font_montserrat_16, 0);
    lv_obj_set_pos(lblMinVal, 166, 24);

    sliderMin = lv_slider_create(panel);
    lv_slider_set_range(sliderMin, SERVO_MIN_ANGLE, SERVO_MAX_ANGLE);
    lv_obj_set_size(sliderMin, 196, 14);
    lv_obj_set_pos(sliderMin, 0, 42);
    lv_obj_set_style_bg_color(sliderMin, lv_color_hex(COLOR_PANEL_LITE), LV_PART_MAIN);
    lv_obj_set_style_bg_color(sliderMin, lv_color_hex(COLOR_HIGHLIGHT), LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(sliderMin, lv_color_hex(COLOR_HIGHLIGHT), LV_PART_KNOB);
    lv_obj_set_style_pad_all(sliderMin, 5, LV_PART_KNOB);

    // ---- MAX row ----
    lv_obj_t *maxTitle = lv_label_create(panel);
    lv_label_set_text(maxTitle, "MAX ANGLE");
    lv_obj_set_style_text_color(maxTitle, lv_color_hex(COLOR_TEXT_SEC), 0);
    lv_obj_set_style_text_font(maxTitle, &lv_font_montserrat_12, 0);
    lv_obj_set_pos(maxTitle, 0, 66);

    lblMaxVal = lv_label_create(panel);
    lv_obj_set_style_text_color(lblMaxVal, lv_color_hex(COLOR_HIGHLIGHT), 0);
    lv_obj_set_style_text_font(lblMaxVal, &lv_font_montserrat_16, 0);
    lv_obj_set_pos(lblMaxVal, 166, 64);

    sliderMax = lv_slider_create(panel);
    lv_slider_set_range(sliderMax, SERVO_MIN_ANGLE, SERVO_MAX_ANGLE);
    lv_obj_set_size(sliderMax, 196, 14);
    lv_obj_set_pos(sliderMax, 0, 82);
    lv_obj_set_style_bg_color(sliderMax, lv_color_hex(COLOR_PANEL_LITE), LV_PART_MAIN);
    lv_obj_set_style_bg_color(sliderMax, lv_color_hex(COLOR_HIGHLIGHT), LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(sliderMax, lv_color_hex(COLOR_HIGHLIGHT), LV_PART_KNOB);
    lv_obj_set_style_pad_all(sliderMax, 5, LV_PART_KNOB);

    // ---- Slider callbacks ----
    lv_obj_add_event_cb(sliderMin, [](lv_event_t *e) {
        int val = lv_slider_get_value(sliderMin);
        sweepMin[activeNozzle] = val;
        char buf[8];
        snprintf(buf, sizeof(buf), "%d\xc2\xb0", val);
        lv_label_set_text(lblMinVal, buf);
        char rbuf[32];
        snprintf(rbuf, sizeof(rbuf), "%d\xc2\xb0 \xe2\x80\x93 %d\xc2\xb0",
                 sweepMin[activeNozzle], sweepMax[activeNozzle]);
        if (rangeLabel) lv_label_set_text(rangeLabel, rbuf);
        if (sweepRunning)
            ServoCtrl::updateSweepRange(activeNozzle, sweepMin[activeNozzle], sweepMax[activeNozzle]);
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
        if (sweepRunning)
            ServoCtrl::updateSweepRange(activeNozzle, sweepMin[activeNozzle], sweepMax[activeNozzle]);
    }, LV_EVENT_VALUE_CHANGED, NULL);

    // Initialize display
    updateSliderDisplay();

    // ---- START / STOP button ----
    startBtn = lv_btn_create(parent);
    lv_obj_set_size(startBtn, 224, 42);
    lv_obj_align(startBtn, LV_ALIGN_BOTTOM_MID, 0, -2);
    lv_obj_set_style_bg_color(startBtn, lv_color_hex(COLOR_HIGHLIGHT), 0);
    lv_obj_set_style_bg_color(startBtn, lv_color_hex(0x12a37e), LV_STATE_PRESSED);
    lv_obj_set_style_radius(startBtn, 10, 0);
    lv_obj_set_style_shadow_width(startBtn, 8, 0);
    lv_obj_set_style_shadow_color(startBtn, lv_color_hex(0x002020), 0);

    startBtnLbl = lv_label_create(startBtn);
    lv_label_set_text(startBtnLbl, LV_SYMBOL_PLAY "  START SWEEP");
    lv_obj_set_style_text_font(startBtnLbl, &lv_font_montserrat_16, 0);
    lv_obj_center(startBtnLbl);

    lv_obj_add_event_cb(startBtn, [](lv_event_t *e) {
        sweepMin[activeNozzle] = lv_slider_get_value(sliderMin);
        sweepMax[activeNozzle] = lv_slider_get_value(sliderMax);

        if (!sweepRunning) {
            ServoCtrl::startSweep(0, sweepMin[0], sweepMax[0]);
            ServoCtrl::startSweep(1, sweepMin[1], sweepMax[1]);
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
