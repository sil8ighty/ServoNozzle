#pragma once

// ===== Hosyond ESP32-S3 2.8" Board Pin Map =====
// Source: https://www.lcdwiki.com/2.8inch_ESP32-S3_Display

// ===== TFT Display (ILI9341V, SPI) =====
#define PIN_TFT_MOSI   11
#define PIN_TFT_MISO   13
#define PIN_TFT_SCLK   12
#define PIN_TFT_CS      10
#define PIN_TFT_DC      46
#define PIN_TFT_RST     -1       // Tied to ESP32-S3 reset
#define PIN_TFT_BL      45

// ===== Capacitive Touch (FT6336G, I2C) =====
#define PIN_TOUCH_SDA   16
#define PIN_TOUCH_SCL   15
#define PIN_TOUCH_INT   17
#define PIN_TOUCH_RST   18

// ===== Servo Pins (using expandable GPIO) =====
#define PIN_SERVO1       2
#define PIN_SERVO2      21

// ===== CNC UART =====
#define CNC_UART_NUM     1       // Use UART1 (UART2 pins may conflict)
#define PIN_CNC_RX      14       // Expandable GPIO on this board
#define PIN_CNC_TX      -1       // TX not needed (receive only)
#define CNC_BAUD_RATE   38400

// ===== Servo Limits =====
#define SERVO_MIN_ANGLE    0
#define SERVO_MAX_ANGLE  180
#define SERVO_MIN_PULSE  500      // microseconds
#define SERVO_MAX_PULSE 2500      // microseconds
#define SERVO_FREQ        50      // Hz

// ===== Jog Settings =====
#define JOG_STEP_FINE      1      // degrees
#define JOG_STEP_COARSE    5      // degrees
#define JOG_REPEAT_MS    150      // auto-repeat interval

// ===== Auto-Save =====
#define AUTOSAVE_DELAY_MS 1500    // ms after last jog to auto-save

// ===== Tool Numbers =====
#define TOOL_MIN           0
#define TOOL_MAX          99

// ===== Display =====
#define SCREEN_WIDTH     240
#define SCREEN_HEIGHT    320

// ===== NVS =====
#define NVS_NAMESPACE    "servospray"

// ===== UI Colors (hex for lv_color_hex()) =====
#define COLOR_BG          0x1a1a2e
#define COLOR_ACCENT      0x0f3460
#define COLOR_HIGHLIGHT   0x16c79a
#define COLOR_WARNING     0xe94560
#define COLOR_TEXT_PRI    0xFFFFFF
#define COLOR_TEXT_SEC    0xA0A0B0
#define COLOR_PANEL       0x252545
#define COLOR_PANEL_LITE  0x353560
