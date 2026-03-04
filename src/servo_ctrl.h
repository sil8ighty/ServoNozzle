#pragma once
#include <cstdint>

namespace ServoCtrl {
    void init();

    void setAngle1(int angle);
    void setAngle2(int angle);
    int  getAngle1();
    int  getAngle2();

    // Sweep control
    void startSweep(uint8_t servoIndex, int minAngle, int maxAngle);
    void updateSweepRange(uint8_t servoIndex, int minAngle, int maxAngle);
    void stopSweep(uint8_t servoIndex);
    void stopAllSweeps();
    bool isSweeping(uint8_t servoIndex);

    // Call from loop() to drive sweep animation
    void update();
}
