#pragma once
#include <cstdint>

namespace SerialParser {
    void init();
    void update();                // call from loop()

    int  getActiveTool();         // -1 if none, 0-99 otherwise
    bool hasNewTool();            // true once after a new tool is detected
    void clearNewToolFlag();
}
