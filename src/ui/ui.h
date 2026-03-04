#pragma once
#include <lvgl.h>

namespace UI {
    void init();
    void update();                            // call from loop()

    // Called when a new tool number arrives from CNC serial
    void onToolChanged(int toolNum);

    // Called by screen jog controls to trigger auto-save timer
    void notifyJogActivity();

    // Screen builders (called once during init)
    void createScreenSingle(lv_obj_t *parent);
    void createScreenDual(lv_obj_t *parent);
    void createScreenSweep(lv_obj_t *parent);

    // Refresh arc values and angle labels on all screens
    void refreshServoDisplays();

    // Update tool indicator text on top status bar
    void refreshToolIndicator(int toolNum);

    // Get the current save-status label for updating from screens
    void setStatusText(const char *text);

    // Strip default theme from a btnmatrix and apply clean custom styling
    void styleBtnMatrix(lv_obj_t *obj);
}
