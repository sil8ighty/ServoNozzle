#include "ui.h"
#include "../config.h"
#include "../servo_ctrl.h"
#include <cstdio>

// Globals for cross-screen refresh (declared extern in ui.cpp)
extern lv_obj_t *g_dualArc1;
extern lv_obj_t *g_dualArc2;
extern lv_obj_t *g_dualAngleLbl1;
extern lv_obj_t *g_dualAngleLbl2;

static int jogStep = JOG_STEP_FINE;

static void updateDisplay1() {
    int a = ServoCtrl::getAngle1();
    if (g_dualArc1) lv_arc_set_value(g_dualArc1, a);
    if (g_dualAngleLbl1) {
        char buf[8];
        snprintf(buf, sizeof(buf), "%d\xc2\xb0", a);
        lv_label_set_text(g_dualAngleLbl1, buf);
    }
}

static void updateDisplay2() {
    int a = ServoCtrl::getAngle2();
    if (g_dualArc2) lv_arc_set_value(g_dualArc2, a);
    if (g_dualAngleLbl2) {
        char buf[8];
        snprintf(buf, sizeof(buf), "%d\xc2\xb0", a);
        lv_label_set_text(g_dualAngleLbl2, buf);
    }
}

// Layout budget per panel (inner): 102 x 162
//   y=0:   Label (14px)
//   y=14:  Arc 86x86 -> bottom 100
//   BOTTOM: Jog buttons 34px

static lv_obj_t* createServoPanel(lv_obj_t *parent, const char *label,
                                   lv_obj_t **arcOut, lv_obj_t **angleLblOut,
                                   int servoIndex) {
    lv_obj_t *panel = lv_obj_create(parent);
    lv_obj_set_size(panel, 110, 170);
    lv_obj_set_style_bg_color(panel, lv_color_hex(COLOR_PANEL), 0);
    lv_obj_set_style_radius(panel, 10, 0);
    lv_obj_set_style_border_width(panel, 0, 0);
    lv_obj_set_style_pad_all(panel, 4, 0);
    lv_obj_clear_flag(panel, LV_OBJ_FLAG_SCROLLABLE);

    // Label
    lv_obj_t *lbl = lv_label_create(panel);
    lv_label_set_text(lbl, label);
    lv_obj_set_style_text_color(lbl, lv_color_hex(COLOR_HIGHLIGHT), 0);
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_12, 0);
    lv_obj_align(lbl, LV_ALIGN_TOP_MID, 0, 0);

    // Arc
    lv_obj_t *arc = lv_arc_create(panel);
    *arcOut = arc;
    lv_obj_set_size(arc, 86, 86);
    lv_arc_set_range(arc, SERVO_MIN_ANGLE, SERVO_MAX_ANGLE);
    lv_arc_set_value(arc, (servoIndex == 0) ? ServoCtrl::getAngle1() : ServoCtrl::getAngle2());
    lv_arc_set_bg_angles(arc, 135, 45);
    lv_obj_set_style_arc_width(arc, 8, LV_PART_MAIN);
    lv_obj_set_style_arc_width(arc, 8, LV_PART_INDICATOR);
    lv_obj_set_style_arc_rounded(arc, true, LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(arc, lv_color_hex(COLOR_PANEL_LITE), LV_PART_MAIN);
    lv_obj_set_style_arc_color(arc, lv_color_hex(COLOR_HIGHLIGHT), LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(arc, lv_color_hex(COLOR_HIGHLIGHT), LV_PART_KNOB);
    lv_obj_set_style_pad_all(arc, 3, LV_PART_KNOB);
    lv_obj_align(arc, LV_ALIGN_TOP_MID, 0, 16);

    // Angle label inside arc
    lv_obj_t *angleLbl = lv_label_create(panel);
    *angleLblOut = angleLbl;
    lv_obj_set_style_text_font(angleLbl, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(angleLbl, lv_color_hex(COLOR_TEXT_PRI), 0);
    {
        int a = (servoIndex == 0) ? ServoCtrl::getAngle1() : ServoCtrl::getAngle2();
        char buf[8];
        snprintf(buf, sizeof(buf), "%d\xc2\xb0", a);
        lv_label_set_text(angleLbl, buf);
    }
    lv_obj_align_to(angleLbl, arc, LV_ALIGN_CENTER, 0, 8);

    // Jog buttons row
    lv_obj_t *btnMinus = lv_btn_create(panel);
    lv_obj_set_size(btnMinus, 44, 34);
    lv_obj_align(btnMinus, LV_ALIGN_BOTTOM_LEFT, 2, 0);
    lv_obj_set_style_bg_color(btnMinus, lv_color_hex(COLOR_ACCENT), 0);
    lv_obj_set_style_bg_color(btnMinus, lv_color_hex(0x1a4a80), LV_STATE_PRESSED);
    lv_obj_set_style_radius(btnMinus, 8, 0);
    lv_obj_t *mLbl = lv_label_create(btnMinus);
    lv_label_set_text(mLbl, LV_SYMBOL_MINUS);
    lv_obj_set_style_text_font(mLbl, &lv_font_montserrat_20, 0);
    lv_obj_center(mLbl);

    lv_obj_t *btnPlus = lv_btn_create(panel);
    lv_obj_set_size(btnPlus, 44, 34);
    lv_obj_align(btnPlus, LV_ALIGN_BOTTOM_RIGHT, -2, 0);
    lv_obj_set_style_bg_color(btnPlus, lv_color_hex(COLOR_ACCENT), 0);
    lv_obj_set_style_bg_color(btnPlus, lv_color_hex(0x1a4a80), LV_STATE_PRESSED);
    lv_obj_set_style_radius(btnPlus, 8, 0);
    lv_obj_t *pLbl = lv_label_create(btnPlus);
    lv_label_set_text(pLbl, LV_SYMBOL_PLUS);
    lv_obj_set_style_text_font(pLbl, &lv_font_montserrat_20, 0);
    lv_obj_center(pLbl);

    // Arc drag callback
    lv_obj_add_event_cb(arc, [](lv_event_t *e) {
        int idx = (int)(intptr_t)lv_event_get_user_data(e);
        lv_obj_t *a = lv_event_get_target(e);
        int val = lv_arc_get_value(a);
        if (idx == 0) { ServoCtrl::setAngle1(val); updateDisplay1(); }
        else          { ServoCtrl::setAngle2(val); updateDisplay2(); }
        UI::notifyJogActivity();
    }, LV_EVENT_VALUE_CHANGED, (void *)(intptr_t)servoIndex);

    lv_obj_add_event_cb(btnMinus, [](lv_event_t *e) {
        int idx = (int)(intptr_t)lv_event_get_user_data(e);
        int cur = (idx == 0) ? ServoCtrl::getAngle1() : ServoCtrl::getAngle2();
        if (idx == 0) { ServoCtrl::setAngle1(cur - jogStep); updateDisplay1(); }
        else          { ServoCtrl::setAngle2(cur - jogStep); updateDisplay2(); }
        UI::notifyJogActivity();
    }, LV_EVENT_CLICKED, (void *)(intptr_t)servoIndex);

    lv_obj_add_event_cb(btnPlus, [](lv_event_t *e) {
        int idx = (int)(intptr_t)lv_event_get_user_data(e);
        int cur = (idx == 0) ? ServoCtrl::getAngle1() : ServoCtrl::getAngle2();
        if (idx == 0) { ServoCtrl::setAngle1(cur + jogStep); updateDisplay1(); }
        else          { ServoCtrl::setAngle2(cur + jogStep); updateDisplay2(); }
        UI::notifyJogActivity();
    }, LV_EVENT_CLICKED, (void *)(intptr_t)servoIndex);

    return panel;
}

// Layout budget (parent inner area): 228 x 232
//   y=0:   Title (18px)
//   y=20:  Two panels 110x170 -> bottom 190
//   BOTTOM-2: Fine/Coarse 28px -> top 202

void UI::createScreenDual(lv_obj_t *parent) {
    // Title
    lv_obj_t *title = lv_label_create(parent);
    lv_label_set_text(title, "DUAL NOZZLE");
    lv_obj_set_style_text_color(title, lv_color_hex(COLOR_HIGHLIGHT), 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_16, 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 0);

    // Two panels side by side
    lv_obj_t *panel1 = createServoPanel(parent, "NOZZLE 1",
                                         &g_dualArc1, &g_dualAngleLbl1, 0);
    lv_obj_align(panel1, LV_ALIGN_TOP_LEFT, 2, 20);

    lv_obj_t *panel2 = createServoPanel(parent, "NOZZLE 2",
                                         &g_dualArc2, &g_dualAngleLbl2, 1);
    lv_obj_align(panel2, LV_ALIGN_TOP_RIGHT, -2, 20);

    // Fine / Coarse toggle (shared for both)
    static const char *stepMap[] = {"Fine (1\xc2\xb0)", "Coarse (5\xc2\xb0)", ""};
    lv_obj_t *stepBtns = lv_btnmatrix_create(parent);
    lv_btnmatrix_set_map(stepBtns, stepMap);
    lv_btnmatrix_set_btn_ctrl_all(stepBtns, LV_BTNMATRIX_CTRL_CHECKABLE);
    lv_btnmatrix_set_one_checked(stepBtns, true);
    lv_btnmatrix_set_btn_ctrl(stepBtns, 0, LV_BTNMATRIX_CTRL_CHECKED);
    UI::styleBtnMatrix(stepBtns);                           // must be before set_size/align
    lv_obj_set_size(stepBtns, 224, 28);
    lv_obj_align(stepBtns, LV_ALIGN_BOTTOM_MID, 0, -2);

    lv_obj_add_event_cb(stepBtns, [](lv_event_t *e) {
        lv_obj_t *obj = lv_event_get_target(e);
        uint16_t id = lv_btnmatrix_get_selected_btn(obj);
        jogStep = (id == 0) ? JOG_STEP_FINE : JOG_STEP_COARSE;
    }, LV_EVENT_VALUE_CHANGED, NULL);
}
