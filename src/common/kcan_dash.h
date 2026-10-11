#pragma once
#include <Arduino.h>
#include "can_bus.h"

// Dashboard values decoded from K-CAN (100 kbit/s, listen-only).
//
// The formulas in kcan_dash.cpp are UNVERIFIED community decodings for the
// E9x. The dashboard shows the raw bytes next to each value so every formula
// can be checked against the car (and the logger) before it is trusted.
// If a value is wrong, change its entry in kcan_dash.cpp only.

enum DashField : uint8_t {
    DASH_SPEED = 0,  // km/h
    DASH_RPM,        // 1/min
    DASH_COOLANT,    // degC
    DASH_OUTSIDE,    // degC
    DASH_BATTERY,    // V
    DASH_FUEL,       // litres
    DASH_RANGE,      // km
    DASH_FIELD_COUNT
};

struct DashFieldInfo {
    const char *key;   // JSON key
    const char *unit;
    uint32_t canId;
    uint8_t decimals;  // suggested display precision
};

struct DashValue {
    float value = 0;
    bool valid = false;
    uint32_t updatedMs = 0;
    uint8_t dlc = 0;
    uint8_t raw[8] = {};
};

class KcanDash {
public:
    // Feed every received frame. Returns true if it updated a dashboard value.
    bool update(const CanFrame &frame, uint32_t nowMs);

    const DashValue &value(DashField f) const { return values_[f]; }
    static const DashFieldInfo &info(DashField f);

    uint32_t totalFrames() const { return totalFrames_; }
    uint32_t lastFrameMs() const { return lastFrameMs_; }

private:
    DashValue values_[DASH_FIELD_COUNT];
    uint32_t totalFrames_ = 0;
    uint32_t lastFrameMs_ = 0;
};
