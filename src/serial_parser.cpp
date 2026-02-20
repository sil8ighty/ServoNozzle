#include "serial_parser.h"
#include "config.h"
#include <HardwareSerial.h>

static HardwareSerial cncSerial(CNC_UART_NUM);

static int  activeTool   = -1;
static bool newToolFlag  = false;

// State machine for T-number parsing
enum ParseState { IDLE, SAW_T };
static ParseState state = IDLE;
static int  digitBuf     = 0;
static int  digitCount   = 0;

void SerialParser::init() {
    cncSerial.begin(CNC_BAUD_RATE, SERIAL_8N1, PIN_CNC_RX, PIN_CNC_TX);
}

void SerialParser::update() {
    while (cncSerial.available()) {
        char c = (char)cncSerial.read();

        switch (state) {
        case IDLE:
            if (c == 'T') {
                state = SAW_T;
                digitBuf = 0;
                digitCount = 0;
            }
            break;

        case SAW_T:
            if (c >= '0' && c <= '9' && digitCount < 2) {
                digitBuf = digitBuf * 10 + (c - '0');
                digitCount++;
            } else {
                // End of digits (or non-digit after T)
                if (digitCount > 0 && digitBuf >= TOOL_MIN && digitBuf <= TOOL_MAX) {
                    if (digitBuf != activeTool) {
                        activeTool = digitBuf;
                        newToolFlag = true;
                    }
                }
                state = IDLE;
                // Re-check this char in case it's another 'T'
                if (c == 'T') {
                    state = SAW_T;
                    digitBuf = 0;
                    digitCount = 0;
                }
            }
            break;
        }
    }

    // Flush any pending T-number at end of available data
    if (state == SAW_T && digitCount > 0) {
        if (digitBuf >= TOOL_MIN && digitBuf <= TOOL_MAX) {
            if (digitBuf != activeTool) {
                activeTool = digitBuf;
                newToolFlag = true;
            }
        }
        state = IDLE;
    }
}

int SerialParser::getActiveTool() {
    return activeTool;
}

bool SerialParser::hasNewTool() {
    return newToolFlag;
}

void SerialParser::clearNewToolFlag() {
    newToolFlag = false;
}
