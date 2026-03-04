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
    float    minAngle  = 0;
    float    maxAngle  = 180;
    bool     atMax     = false;     // false=at min, true=at max
    uint32_t moveTime  = 0;        // when the last move command was sent
    uint32_t dwellMs   = 0;        // wait time before issuing next move
};

static SweepState sweep[2];

// Servo travel speed estimate: ~6ms per degree (standard servos ~0.15-0.2s per 60°)
// Plus a settle buffer so we never send a new command mid-travel
#define SERVO_MS_PER_DEG  6
#define SETTLE_MS         100

static uint32_t calcDwell(float angleDelta) {
    return (uint32_t)(angleDelta * SERVO_MS_PER_DEG) + SETTLE_MS;
}

static int clampAngle(int a) {
    if (a < SERVO_MIN_ANGLE) return SERVO_MIN_ANGLE;
    if (a > SERVO_MAX_ANGLE) return SERVO_MAX_ANGLE;
    return a;
}

// Map float degrees to microseconds and write directly
static void servoWriteUs(Servo &srv, float angleDeg) {
    float frac = angleDeg / (float)SERVO_MAX_ANGLE;
    int us = (int)(SERVO_MIN_PULSE + frac * (SERVO_MAX_PULSE - SERVO_MIN_PULSE) + 0.5f);
    if (us < SERVO_MIN_PULSE) us = SERVO_MIN_PULSE;
    if (us > SERVO_MAX_PULSE) us = SERVO_MAX_PULSE;
    srv.writeMicroseconds(us);
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
    if (sweep[0].active) return;
    angle1 = clampAngle(angle);
    servo1.write(angle1);
}

void ServoCtrl::setAngle2(int angle) {
    if (sweep[1].active) return;
    angle2 = clampAngle(angle);
    servo2.write(angle2);
}

int ServoCtrl::getAngle1() { return angle1; }
int ServoCtrl::getAngle2() { return angle2; }

void ServoCtrl::startSweep(uint8_t servoIndex, int minAngle, int maxAngle) {
    if (servoIndex > 1) return;
    SweepState &s = sweep[servoIndex];
    s.minAngle = (float)clampAngle(minAngle);
    s.maxAngle = (float)clampAngle(maxAngle);
    if (s.minAngle >= s.maxAngle) return;

    // Move to min position, compute dwell for the first travel
    s.atMax    = false;
    s.moveTime = millis();
    s.dwellMs  = calcDwell(s.maxAngle - s.minAngle);
    s.active   = true;

    int startAngle = (int)(s.minAngle + 0.5f);
    if (servoIndex == 0) angle1 = startAngle;
    else                 angle2 = startAngle;
    servoWriteUs((servoIndex == 0) ? servo1 : servo2, s.minAngle);
}

void ServoCtrl::updateSweepRange(uint8_t servoIndex, int minAngle, int maxAngle) {
    if (servoIndex > 1) return;
    SweepState &s = sweep[servoIndex];
    if (!s.active) return;
    s.minAngle = (float)clampAngle(minAngle);
    s.maxAngle = (float)clampAngle(maxAngle);
    if (s.minAngle >= s.maxAngle) { s.active = false; return; }
    // Recalculate dwell for the new range
    s.dwellMs = calcDwell(s.maxAngle - s.minAngle);
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

        // Wait until servo has finished traveling + settled
        if (now - s.moveTime < s.dwellMs) continue;

        // Flip direction: go to the other end
        s.atMax = !s.atMax;
        float target = s.atMax ? s.maxAngle : s.minAngle;

        // Send one command and let the servo handle the motion
        servoWriteUs((i == 0) ? servo1 : servo2, target);

        // Update angle for UI
        int intAngle = (int)(target + 0.5f);
        if (i == 0) angle1 = intAngle;
        else        angle2 = intAngle;

        // Dwell = travel time for full range + settle
        s.dwellMs  = calcDwell(s.maxAngle - s.minAngle);
        s.moveTime = now;
    }
}
