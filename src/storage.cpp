#include "storage.h"
#include "config.h"
#include <Preferences.h>
#include <cstdio>

static Preferences prefs;

static void makeKey(uint8_t toolNum, char *buf, size_t len) {
    snprintf(buf, len, "t%02u", toolNum);
}

void Storage::init() {
    prefs.begin(NVS_NAMESPACE, false);
    prefs.end();
}

bool Storage::load(uint8_t toolNum, ToolData &data) {
    if (toolNum > TOOL_MAX) return false;

    char key[8];
    makeKey(toolNum, key, sizeof(key));

    prefs.begin(NVS_NAMESPACE, true);
    size_t len = prefs.getBytesLength(key);
    bool ok = false;
    if (len == sizeof(ToolData)) {
        prefs.getBytes(key, &data, sizeof(ToolData));
        ok = true;
    }
    prefs.end();
    return ok;
}

void Storage::save(uint8_t toolNum, const ToolData &data) {
    if (toolNum > TOOL_MAX) return;

    char key[8];
    makeKey(toolNum, key, sizeof(key));

    prefs.begin(NVS_NAMESPACE, false);
    prefs.putBytes(key, &data, sizeof(ToolData));
    prefs.end();
}

void Storage::clearTool(uint8_t toolNum) {
    if (toolNum > TOOL_MAX) return;

    char key[8];
    makeKey(toolNum, key, sizeof(key));

    prefs.begin(NVS_NAMESPACE, false);
    prefs.remove(key);
    prefs.end();
}

void Storage::clearAll() {
    prefs.begin(NVS_NAMESPACE, false);
    prefs.clear();
    prefs.end();
}
