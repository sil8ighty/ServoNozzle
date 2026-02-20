#include "servo_ctrl.h"
#include "config.h"
#include <ESP32Servo.h>
#include <Arduino.h>

static Servo servo1;
static Servo servo2;
static int angle1 = 90;
static int angle2 = 90;

struct SweepState {
    bool     active    = false;
    int      minAngle  = 0;
    int      maxAngle  = 180;
    int      direction = 1;       // 1 = increasing, -1 = decreasing
    uint32_t stepMs    = 20;      // ms per degree
    uint32_t lastStep  = 0;
};

static SweepState sweep[2];

// Speed index (0=slow,1=med,2=fast) to ms-per-degree
static uint32_t speedToMs(uint8_t speed) {
    switch (speed) {
        case 0:  return 40;
        case 1:  return 20;
        case 2:  return 8;
        default: return 20;
    }
}

static int clampAngle(int a) {
    if (a < SERVO_MIN_ANGLE) return SERVO_MIN_ANGLE;
    if (a > SERVO_MAX_ANGLE) return SERVO_MAX_ANGLE;
    return a;
}

void ServoCtrl::init() {
    servo1.setPeriodHertz(SERVO_FREQ);
    servo1.attach(PIN_SERVO1, SERVO_MIN_PULSE, SERVO_MAX_PULSE);
    servo1.write(angle1);

    servo2.setPeriodHertz(SERVO_FREQ);
    servo2.attach(PIN_SERVO2, SERVO_MIN_PULSE, SERVO_MAX_PULSE);
    servo2.write(angle2);
}

void ServoCtrl::setAngle1(int angle) {
    angle1 = clampAngle(angle);
    servo1.write(angle1);
}

void ServoCtrl::setAngle2(int angle) {
    angle2 = clampAngle(angle);
    servo2.write(angle2);
}

int ServoCtrl::getAngle1() { return angle1; }
int ServoCtrl::getAngle2() { return angle2; }

void ServoCtrl::startSweep(uint8_t servoIndex, int minAngle, int maxAngle, uint8_t speed) {
    if (servoIndex > 1) return;
    SweepState &s = sweep[servoIndex];
    s.minAngle  = clampAngle(minAngle);
    s.maxAngle  = clampAngle(maxAngle);
    if (s.minAngle >= s.maxAngle) return;
    s.stepMs    = speedToMs(speed);
    s.direction = 1;
    s.lastStep  = millis();
    s.active    = true;

    // Move to start position
    if (servoIndex == 0) setAngle1(s.minAngle);
    else                 setAngle2(s.minAngle);
}

void ServoCtrl::stopSweep(uint8_t servoIndex) {
    if (servoIndex > 1) return;
    sweep[servoIndex].active = false;
}

void ServoCtrl::stopAllSweeps() {
    sweep[0].active = false;
    sweep[1].active = false;
}

bool ServoCtrl::isSweeping(uint8_t servoIndex) {
    if (servoIndex > 1) return false;
    return sweep[servoIndex].active;
}

void ServoCtrl::update() {
    uint32_t now = millis();

    for (uint8_t i = 0; i < 2; i++) {
        SweepState &s = sweep[i];
        if (!s.active) continue;

        if (now - s.lastStep >= s.stepMs) {
            s.lastStep = now;

            int cur = (i == 0) ? angle1 : angle2;
            cur += s.direction;

            if (cur >= s.maxAngle) {
                cur = s.maxAngle;
                s.direction = -1;
            } else if (cur <= s.minAngle) {
                cur = s.minAngle;
                s.direction = 1;
            }

            if (i == 0) setAngle1(cur);
            else        setAngle2(cur);
        }
    }
}
