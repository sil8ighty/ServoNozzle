#include "ui.h"
#include "../config.h"
#include "../servo_ctrl.h"
#include <cstdio>

// Globals for cross-screen refresh (declared extern in ui.cpp)
extern lv_obj_t *g_singleArc;
extern lv_obj_t *g_singleAngleLbl;

static int jogStep = JOG_STEP_FINE;

static void updateAngleDisplay() {
    int a = ServoCtrl::getAngle1();
    if (g_singleArc) lv_arc_set_value(g_singleArc, a);
    if (g_singleAngleLbl) {
        char buf[8];
        snprintf(buf, sizeof(buf), "%d\xc2\xb0", a);
        lv_label_set_text(g_singleAngleLbl, buf);
    }
}

// Layout budget (parent inner area): 228 x 232
//   y=0:   Title (18px)
//   y=18:  Arc 130x130 -> bottom 148
//   y=160: Fine/Coarse 28px -> bottom 188  (BOTTOM -44)
//   y=190: Jog buttons 40px -> bottom 230  (BOTTOM -2)

void UI::createScreenSingle(lv_obj_t *parent) {
    // ---- Title ----
    lv_obj_t *title = lv_label_create(parent);
    lv_label_set_text(title, "SINGLE NOZZLE");
    lv_obj_set_style_text_color(title, lv_color_hex(COLOR_HIGHLIGHT), 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_16, 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 0);

    // ---- Arc gauge ----
    lv_obj_t *arc = lv_arc_create(parent);
    g_singleArc = arc;
    lv_obj_set_size(arc, 130, 130);
    lv_arc_set_range(arc, SERVO_MIN_ANGLE, SERVO_MAX_ANGLE);
    lv_arc_set_value(arc, ServoCtrl::getAngle1());
    lv_arc_set_bg_angles(arc, 135, 45);
    lv_obj_set_style_arc_width(arc, 12, LV_PART_MAIN);
    lv_obj_set_style_arc_width(arc, 12, LV_PART_INDICATOR);
    lv_obj_set_style_arc_rounded(arc, true, LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(arc, lv_color_hex(COLOR_PANEL_LITE), LV_PART_MAIN);
    lv_obj_set_style_arc_color(arc, lv_color_hex(COLOR_HIGHLIGHT), LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(arc, lv_color_hex(COLOR_HIGHLIGHT), LV_PART_KNOB);
    lv_obj_set_style_pad_all(arc, 4, LV_PART_KNOB);
    lv_obj_align(arc, LV_ALIGN_TOP_MID, 0, 18);

    // ---- Angle label inside arc ----
    lv_obj_t *angleLbl = lv_label_create(parent);
    g_singleAngleLbl = angleLbl;
    lv_obj_set_style_text_font(angleLbl, &lv_font_montserrat_36, 0);
    lv_obj_set_style_text_color(angleLbl, lv_color_hex(COLOR_TEXT_PRI), 0);
    {
        char buf[8];
        snprintf(buf, sizeof(buf), "%d\xc2\xb0", ServoCtrl::getAngle1());
        lv_label_set_text(angleLbl, buf);
    }
    lv_obj_align_to(angleLbl, arc, LV_ALIGN_CENTER, 0, 8);

    // "SERVO 1" subtitle under arc
    lv_obj_t *subLbl = lv_label_create(parent);
    lv_label_set_text(subLbl, "SERVO 1");
    lv_obj_set_style_text_color(subLbl, lv_color_hex(COLOR_TEXT_SEC), 0);
    lv_obj_set_style_text_font(subLbl, &lv_font_montserrat_12, 0);
    lv_obj_align_to(subLbl, arc, LV_ALIGN_OUT_BOTTOM_MID, 0, -12);

    // ---- Arc drag event ----
    lv_obj_add_event_cb(arc, [](lv_event_t *e) {
        lv_obj_t *a = lv_event_get_target(e);
        ServoCtrl::setAngle1(lv_arc_get_value(a));
        updateAngleDisplay();
        UI::notifyJogActivity();
    }, LV_EVENT_VALUE_CHANGED, NULL);

    // ---- Fine / Coarse toggle ----
    static const char *stepMap[] = {"Fine (1\xc2\xb0)", "Coarse (5\xc2\xb0)", ""};
    lv_obj_t *stepBtns = lv_btnmatrix_create(parent);
    lv_btnmatrix_set_map(stepBtns, stepMap);
    lv_btnmatrix_set_btn_ctrl_all(stepBtns, LV_BTNMATRIX_CTRL_CHECKABLE);
    lv_btnmatrix_set_one_checked(stepBtns, true);
    lv_btnmatrix_set_btn_ctrl(stepBtns, 0, LV_BTNMATRIX_CTRL_CHECKED);
    UI::styleBtnMatrix(stepBtns);                           // must be before set_size/align
    lv_obj_set_size(stepBtns, 224, 28);
    lv_obj_align(stepBtns, LV_ALIGN_BOTTOM_MID, 0, -44);

    lv_obj_add_event_cb(stepBtns, [](lv_event_t *e) {
        lv_obj_t *obj = lv_event_get_target(e);
        uint16_t id = lv_btnmatrix_get_selected_btn(obj);
        jogStep = (id == 0) ? JOG_STEP_FINE : JOG_STEP_COARSE;
    }, LV_EVENT_VALUE_CHANGED, NULL);

    // ---- Jog buttons ----
    lv_obj_t *btnMinus = lv_btn_create(parent);
    lv_obj_set_size(btnMinus, 100, 40);
    lv_obj_align(btnMinus, LV_ALIGN_BOTTOM_LEFT, 4, -2);
    lv_obj_set_style_bg_color(btnMinus, lv_color_hex(COLOR_ACCENT), 0);
    lv_obj_set_style_bg_color(btnMinus, lv_color_hex(0x1a4a80), LV_STATE_PRESSED);
    lv_obj_set_style_radius(btnMinus, 8, 0);

    lv_obj_t *m = lv_label_create(btnMinus);
    lv_label_set_text(m, LV_SYMBOL_MINUS);
    lv_obj_set_style_text_font(m, &lv_font_montserrat_24, 0);
    lv_obj_center(m);

    lv_obj_t *btnPlus = lv_btn_create(parent);
    lv_obj_set_size(btnPlus, 100, 40);
    lv_obj_align(btnPlus, LV_ALIGN_BOTTOM_RIGHT, -4, -2);
    lv_obj_set_style_bg_color(btnPlus, lv_color_hex(COLOR_ACCENT), 0);
    lv_obj_set_style_bg_color(btnPlus, lv_color_hex(0x1a4a80), LV_STATE_PRESSED);
    lv_obj_set_style_radius(btnPlus, 8, 0);

    lv_obj_t *p = lv_label_create(btnPlus);
    lv_label_set_text(p, LV_SYMBOL_PLUS);
    lv_obj_set_style_text_font(p, &lv_font_montserrat_24, 0);
    lv_obj_center(p);

    // Jog callbacks
    lv_obj_add_event_cb(btnMinus, [](lv_event_t *e) {
        ServoCtrl::setAngle1(ServoCtrl::getAngle1() - jogStep);
        updateAngleDisplay();
        UI::notifyJogActivity();
    }, LV_EVENT_CLICKED, NULL);

    lv_obj_add_event_cb(btnPlus, [](lv_event_t *e) {
        ServoCtrl::setAngle1(ServoCtrl::getAngle1() + jogStep);
        updateAngleDisplay();
        UI::notifyJogActivity();
    }, LV_EVENT_CLICKED, NULL);
}
