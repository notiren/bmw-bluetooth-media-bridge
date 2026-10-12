#pragma once
#include <Arduino.h>
#include "../common/can_bus.h"

// Table of every CAN ID seen, with its latest payload, for the dashboard's
// sniffer page.

struct CanIdEntry {
    uint32_t id;
    bool ext;
    uint8_t dlc;
    uint8_t data[8];
    uint32_t count;
    uint32_t lastMs;
    uint32_t changedMs[8];  // per byte: last time it changed (0 = never)
};

class CanIdTable {
public:
    static constexpr uint16_t kMax = 160;

    // Records the frame. Returns true if the ID is new or its payload changed
    // (also true when the table is full and the frame is not stored).
    bool note(const CanFrame &f, uint32_t nowMs);
    void clear();

    uint16_t size() const { return count_; }
    bool full() const { return full_; }
    const CanIdEntry &operator[](uint16_t i) const { return entries_[i]; }

private:
    CanIdEntry entries_[kMax];
    uint16_t count_ = 0;
    bool full_ = false;
};
