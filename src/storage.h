#pragma once
#include <cstdint>

struct ToolData {
    int16_t servo1Angle  = -1;   // -1 = not set
    int16_t servo2Angle  = -1;
    int16_t sweepMin1    = 0;
    int16_t sweepMax1    = 180;
    int16_t sweepMin2    = 0;
    int16_t sweepMax2    = 180;
    uint8_t sweepSpeed1  = 1;    // 0=slow, 1=medium, 2=fast
    uint8_t sweepSpeed2  = 1;
};

namespace Storage {
    void init();
    bool load(uint8_t toolNum, ToolData &data);
    void save(uint8_t toolNum, const ToolData &data);
    void clearTool(uint8_t toolNum);
    void clearAll();
}
