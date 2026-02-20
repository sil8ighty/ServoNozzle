// ============================================================
// CNC Servo Sprayer - Hosyond ESP32-S3 2.8" ILI9341 + FT6336G
// ============================================================
#include <Arduino.h>
#include "config.h"
#include "storage.h"
#include "servo_ctrl.h"
#include "serial_parser.h"
#include "ui/ui.h"

void setup() {
    Serial.begin(115200);
    Serial.println("=== CNC Servo Sprayer ===");

    Storage::init();
    ServoCtrl::init();
    SerialParser::init();
    UI::init();

    Serial.printf("Ready. Listening for T-numbers on UART%d (RX=%d)...\n",
                  CNC_UART_NUM, PIN_CNC_RX);
}

void loop() {
    SerialParser::update();

    if (SerialParser::hasNewTool()) {
        SerialParser::clearNewToolFlag();
        int tool = SerialParser::getActiveTool();
        Serial.printf("Tool change: T%d\n", tool);
        ServoCtrl::stopAllSweeps();

        ToolData td;
        if (Storage::load(tool, td)) {
            if (td.servo1Angle >= 0) ServoCtrl::setAngle1(td.servo1Angle);
            if (td.servo2Angle >= 0) ServoCtrl::setAngle2(td.servo2Angle);
            Serial.printf("  Loaded: S1=%d S2=%d\n", td.servo1Angle, td.servo2Angle);
        } else {
            Serial.println("  No saved position for this tool");
        }

        UI::onToolChanged(tool);
        UI::refreshServoDisplays();
    }

    ServoCtrl::update();
    UI::update();
}
