#include "ui.h"
#include "../config.h"
#include "../servo_ctrl.h"
#include "../serial_parser.h"
#include "../storage.h"

#include <Wire.h>
#include <TFT_eSPI.h>
#include <lvgl.h>

// ---------- Hardware ----------
static TFT_eSPI tft = TFT_eSPI();

// ---------- FT6336G I2C Touch ----------
#define FT6336_ADDR 0x38

// ---------- LVGL draw buffer ----------
#define BUF_PIXELS (SCREEN_WIDTH * 32)
static lv_disp_draw_buf_t drawBuf;
static lv_color_t buf1[BUF_PIXELS];

// ---------- Shared UI objects ----------
static lv_obj_t *toolLabel    = nullptr;
static lv_obj_t *statusLabel  = nullptr;
static lv_obj_t *tabview      = nullptr;

// ---------- Auto-save state ----------
static uint32_t lastJogTime = 0;
static bool     jogDirty    = false;

// ---------- External screen-refresh pointers (set by screen_*.cpp) ----------
// Single screen
lv_obj_t *g_singleArc     = nullptr;
lv_obj_t *g_singleAngleLbl = nullptr;
// Dual screen
lv_obj_t *g_dualArc1      = nullptr;
lv_obj_t *g_dualArc2      = nullptr;
lv_obj_t *g_dualAngleLbl1 = nullptr;
lv_obj_t *g_dualAngleLbl2 = nullptr;

// ================================================================
// Display flush callback
// ================================================================
static void dispFlush(lv_disp_drv_t *disp, const lv_area_t *area, lv_color_t *color_p) {
    uint32_t w = area->x2 - area->x1 + 1;
    uint32_t h = area->y2 - area->y1 + 1;
    tft.startWrite();
    tft.setAddrWindow(area->x1, area->y1, w, h);
    tft.pushPixels((uint16_t *)&color_p->full, w * h);
    tft.endWrite();
    lv_disp_flush_ready(disp);
}

// ================================================================
// Touchpad read callback (FT6336G capacitive touch via I2C)
// ================================================================
static void touchRead(lv_indev_drv_t *drv, lv_indev_data_t *data) {
    Wire.beginTransmission(FT6336_ADDR);
    Wire.write(0x02);  // TD_STATUS register (number of touch points)
    Wire.endTransmission(false);
    Wire.requestFrom((uint8_t)FT6336_ADDR, (uint8_t)5);

    if (Wire.available() >= 5) {
        uint8_t touches = Wire.read();       // 0x02: touch count
        uint8_t xHi     = Wire.read();       // 0x03: event + X[11:8]
        uint8_t xLo     = Wire.read();       // 0x04: X[7:0]
        uint8_t yHi     = Wire.read();       // 0x05: ID + Y[11:8]
        uint8_t yLo     = Wire.read();       // 0x06: Y[7:0]

        if (touches > 0 && touches < 3) {
            int x = ((xHi & 0x0F) << 8) | xLo;
            int y = ((yHi & 0x0F) << 8) | yLo;
            data->point.x = constrain(x, 0, SCREEN_WIDTH  - 1);
            data->point.y = constrain(y, 0, SCREEN_HEIGHT - 1);
            data->state = LV_INDEV_STATE_PRESSED;
        } else {
            data->state = LV_INDEV_STATE_RELEASED;
        }
    } else {
        data->state = LV_INDEV_STATE_RELEASED;
    }
}

// ================================================================
// Theme / style helpers
// ================================================================
static void applyTheme() {
    lv_theme_t *th = lv_theme_default_init(
        lv_disp_get_default(),
        lv_color_hex(COLOR_ACCENT),
        lv_color_hex(COLOR_WARNING),
        true,                              // dark mode
        &lv_font_montserrat_14
    );
    lv_disp_set_theme(lv_disp_get_default(), th);
}

// ================================================================
// Build top status bar and tabview skeleton
// ================================================================
static void buildUI() {
    lv_obj_t *scr = lv_scr_act();
    lv_obj_set_style_bg_color(scr, lv_color_hex(COLOR_BG), 0);
    lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);

    // ---- Top status bar (32px) ----
    lv_obj_t *bar = lv_obj_create(scr);
    lv_obj_set_size(bar, SCREEN_WIDTH, 32);
    lv_obj_set_pos(bar, 0, 0);
    lv_obj_set_style_bg_color(bar, lv_color_hex(COLOR_PANEL), 0);
    lv_obj_set_style_border_width(bar, 0, 0);
    lv_obj_set_style_radius(bar, 0, 0);
    lv_obj_set_style_pad_left(bar, 8, 0);
    lv_obj_set_style_pad_right(bar, 8, 0);
    lv_obj_clear_flag(bar, LV_OBJ_FLAG_SCROLLABLE);

    // Tool indicator (left)
    toolLabel = lv_label_create(bar);
    lv_label_set_text(toolLabel, LV_SYMBOL_SETTINGS "  No Tool");
    lv_obj_set_style_text_color(toolLabel, lv_color_hex(COLOR_HIGHLIGHT), 0);
    lv_obj_set_style_text_font(toolLabel, &lv_font_montserrat_16, 0);
    lv_obj_align(toolLabel, LV_ALIGN_LEFT_MID, 0, 0);

    // Status text (right)
    statusLabel = lv_label_create(bar);
    lv_label_set_text(statusLabel, "Jog Mode");
    lv_obj_set_style_text_color(statusLabel, lv_color_hex(COLOR_TEXT_SEC), 0);
    lv_obj_set_style_text_font(statusLabel, &lv_font_montserrat_12, 0);
    lv_obj_align(statusLabel, LV_ALIGN_RIGHT_MID, 0, 0);

    // ---- Tabview (fill remaining space, tabs at bottom) ----
    tabview = lv_tabview_create(scr, LV_DIR_BOTTOM, 44);
    lv_obj_set_size(tabview, SCREEN_WIDTH, SCREEN_HEIGHT - 32);
    lv_obj_set_pos(tabview, 0, 32);
    lv_obj_set_style_bg_color(tabview, lv_color_hex(COLOR_BG), 0);
    lv_obj_set_style_border_width(tabview, 0, 0);

    // Style tab buttons
    lv_obj_t *tabBtns = lv_tabview_get_tab_btns(tabview);
    lv_obj_set_style_bg_color(tabBtns, lv_color_hex(COLOR_PANEL), 0);
    lv_obj_set_style_text_color(tabBtns, lv_color_hex(COLOR_TEXT_SEC), 0);
    lv_obj_set_style_text_color(tabBtns, lv_color_hex(COLOR_HIGHLIGHT),
                                 LV_PART_ITEMS | LV_STATE_CHECKED);
    lv_obj_set_style_border_side(tabBtns, LV_BORDER_SIDE_TOP, LV_PART_ITEMS | LV_STATE_CHECKED);
    lv_obj_set_style_border_color(tabBtns, lv_color_hex(COLOR_HIGHLIGHT),
                                   LV_PART_ITEMS | LV_STATE_CHECKED);
    lv_obj_set_style_border_width(tabBtns, 2, LV_PART_ITEMS | LV_STATE_CHECKED);
    lv_obj_set_style_text_font(tabBtns, &lv_font_montserrat_14, 0);

    // Disable swipe on tab content (prevents conflict with arc drag)
    lv_obj_t *tabContent = lv_tabview_get_content(tabview);
    lv_obj_clear_flag(tabContent, LV_OBJ_FLAG_SCROLLABLE);

    // Create tabs
    lv_obj_t *tab1 = lv_tabview_add_tab(tabview, LV_SYMBOL_GPS " Single");
    lv_obj_t *tab2 = lv_tabview_add_tab(tabview, LV_SYMBOL_SHUFFLE " Dual");
    lv_obj_t *tab3 = lv_tabview_add_tab(tabview, LV_SYMBOL_REFRESH " Sweep");

    // Style tab content areas
    for (auto *tab : {tab1, tab2, tab3}) {
        lv_obj_set_style_bg_color(tab, lv_color_hex(COLOR_BG), 0);
        lv_obj_set_style_pad_all(tab, 6, 0);
        lv_obj_clear_flag(tab, LV_OBJ_FLAG_SCROLLABLE);
    }

    // Build screen contents
    UI::createScreenSingle(tab1);
    UI::createScreenDual(tab2);
    UI::createScreenSweep(tab3);
}

// ================================================================
// Public API
// ================================================================

void UI::init() {
    Serial.println("UI: TFT init...");

    // Backlight on
    pinMode(PIN_TFT_BL, OUTPUT);
    digitalWrite(PIN_TFT_BL, HIGH);

    // Display init (TFT_eSPI manages its own SPI bus)
    tft.init();
    tft.setRotation(0);  // portrait
    tft.setSwapBytes(true);   // ILI9341 expects big-endian RGB565
    tft.invertDisplay(true);  // ILI9341V panel needs inversion enabled
    tft.fillScreen(TFT_BLACK);
    Serial.println("UI: TFT OK");

    // FT6336G capacitive touch init (I2C)
    pinMode(PIN_TOUCH_RST, OUTPUT);
    digitalWrite(PIN_TOUCH_RST, LOW);
    delay(10);
    digitalWrite(PIN_TOUCH_RST, HIGH);
    delay(300);
    Wire.begin(PIN_TOUCH_SDA, PIN_TOUCH_SCL);
    Serial.println("UI: Touch OK (FT6336G I2C)");

    // LVGL init
    lv_init();

    // Display driver
    lv_disp_draw_buf_init(&drawBuf, buf1, NULL, BUF_PIXELS);
    static lv_disp_drv_t dispDrv;
    lv_disp_drv_init(&dispDrv);
    dispDrv.hor_res  = SCREEN_WIDTH;
    dispDrv.ver_res  = SCREEN_HEIGHT;
    dispDrv.flush_cb = dispFlush;
    dispDrv.draw_buf = &drawBuf;
    lv_disp_drv_register(&dispDrv);

    // Touch input driver
    static lv_indev_drv_t indevDrv;
    lv_indev_drv_init(&indevDrv);
    indevDrv.type    = LV_INDEV_TYPE_POINTER;
    indevDrv.read_cb = touchRead;
    lv_indev_drv_register(&indevDrv);

    applyTheme();
    buildUI();
}

void UI::update() {
    lv_timer_handler();

    // Auto-save logic
    if (jogDirty && (millis() - lastJogTime >= AUTOSAVE_DELAY_MS)) {
        jogDirty = false;
        int tool = SerialParser::getActiveTool();
        if (tool >= 0) {
            ToolData td;
            // Load existing to preserve sweep settings
            Storage::load(tool, td);
            td.servo1Angle = ServoCtrl::getAngle1();
            td.servo2Angle = ServoCtrl::getAngle2();
            Storage::save(tool, td);

            char buf[24];
            snprintf(buf, sizeof(buf), "T%d Saved", tool);
            setStatusText(buf);
        }
    }
}

void UI::notifyJogActivity() {
    lastJogTime = millis();
    jogDirty = true;

    int tool = SerialParser::getActiveTool();
    if (tool >= 0) {
        setStatusText("Modified...");
    } else {
        setStatusText("Jog Mode");
    }
}

void UI::onToolChanged(int toolNum) {
    refreshToolIndicator(toolNum);

    char buf[24];
    ToolData td;
    if (Storage::load(toolNum, td) && td.servo1Angle >= 0) {
        snprintf(buf, sizeof(buf), "T%d Loaded", toolNum);
    } else {
        snprintf(buf, sizeof(buf), "T%d (new)", toolNum);
    }
    setStatusText(buf);
}

void UI::refreshToolIndicator(int toolNum) {
    if (!toolLabel) return;
    if (toolNum < 0) {
        lv_label_set_text(toolLabel, LV_SYMBOL_SETTINGS "  No Tool");
    } else {
        char buf[24];
        snprintf(buf, sizeof(buf), LV_SYMBOL_SETTINGS "  T%d", toolNum);
        lv_label_set_text(toolLabel, buf);
    }
}

void UI::setStatusText(const char *text) {
    if (statusLabel) {
        lv_label_set_text(statusLabel, text);
    }
}

void UI::styleBtnMatrix(lv_obj_t *obj) {
    // Remove ALL theme-applied styles (this kills the strikethrough)
    lv_obj_remove_style_all(obj);

    // Main background
    lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_bg_color(obj, lv_color_hex(COLOR_PANEL), LV_PART_MAIN);
    lv_obj_set_style_border_width(obj, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_column(obj, 4, LV_PART_MAIN);
    lv_obj_set_style_pad_row(obj, 4, LV_PART_MAIN);
    lv_obj_set_style_pad_all(obj, 2, LV_PART_MAIN);

    // Items - default state
    lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, LV_PART_ITEMS);
    lv_obj_set_style_bg_color(obj, lv_color_hex(COLOR_ACCENT), LV_PART_ITEMS);
    lv_obj_set_style_text_color(obj, lv_color_hex(COLOR_TEXT_SEC), LV_PART_ITEMS);
    lv_obj_set_style_text_font(obj, &lv_font_montserrat_14, LV_PART_ITEMS);
    lv_obj_set_style_radius(obj, 6, LV_PART_ITEMS);
    lv_obj_set_style_border_width(obj, 0, LV_PART_ITEMS);
    lv_obj_set_style_shadow_width(obj, 0, LV_PART_ITEMS);

    // Items - checked (selected) state
    lv_obj_set_style_bg_color(obj, lv_color_hex(COLOR_HIGHLIGHT), LV_PART_ITEMS | LV_STATE_CHECKED);
    lv_obj_set_style_text_color(obj, lv_color_hex(0xFFFFFF), LV_PART_ITEMS | LV_STATE_CHECKED);

    // Items - pressed feedback
    lv_obj_set_style_bg_color(obj, lv_color_hex(0x1a4a80), LV_PART_ITEMS | LV_STATE_PRESSED);
}

void UI::refreshServoDisplays() {
    int a1 = ServoCtrl::getAngle1();
    int a2 = ServoCtrl::getAngle2();
    char buf[8];

    // Single screen
    if (g_singleArc) {
        lv_arc_set_value(g_singleArc, a1);
    }
    if (g_singleAngleLbl) {
        snprintf(buf, sizeof(buf), "%d\xc2\xb0", a1);
        lv_label_set_text(g_singleAngleLbl, buf);
    }

    // Dual screen
    if (g_dualArc1) lv_arc_set_value(g_dualArc1, a1);
    if (g_dualArc2) lv_arc_set_value(g_dualArc2, a2);
    if (g_dualAngleLbl1) {
        snprintf(buf, sizeof(buf), "%d\xc2\xb0", a1);
        lv_label_set_text(g_dualAngleLbl1, buf);
    }
    if (g_dualAngleLbl2) {
        snprintf(buf, sizeof(buf), "%d\xc2\xb0", a2);
        lv_label_set_text(g_dualAngleLbl2, buf);
    }
}
