#pragma once
#include <Arduino.h>
#include "can_bus.h"

// BMW E9x steering-wheel buttons (MFL), CAN ID 0x1D6 on K-CAN (100 kbit/s).
//
// Layout from the opendbc BMW E8x/E9x DBC (BO_ 470 SteeringButtons, 2 bytes),
// cross-checked with BlueGenieBMW and llilakoblock/bmw-e87-e90-can-bt.
// Idle frame = C0 0C. byte0 == 0xFF signals a steering-button fault.
// Verify on your car (serial monitor prints each press, sniffer shows 0x1D6):
//
//   button        byte  mask   example frame
//   Up/Next        0    0x20   E0 0C
//   Down/Prev      0    0x10   D0 0C
//   Volume +       0    0x08   C8 0C
//   Volume -       0    0x04   C4 0C
//   Telephone      0    0x01   C1 0C
//   Voice          1    0x01   C0 0D
//
// If your car differs, change the table in mfl.cpp only.

#define MFL_CAN_ID 0x1D6

enum MflButton : uint8_t {
    MFL_NEXT = 0,
    MFL_PREV,
    MFL_VOL_UP,
    MFL_VOL_DOWN,
    MFL_PHONE,
    MFL_VOICE,
    MFL_BUTTON_COUNT
};

const char *mflButtonName(MflButton b);

// Tracks button state from 0x1D6 frames and reports press edges once per
// physical press (the car repeats the frame while a button is held).
class MflDecoder {
public:
    // Feed a received frame. Returns a bitmask (1 << MflButton) of buttons
    // that were newly pressed by this frame. Non-0x1D6 frames return 0.
    uint8_t update(const CanFrame &frame, uint32_t nowMs);

    // Call regularly: releases all buttons if 0x1D6 has not been seen for a
    // while (e.g. ignition off / bus asleep).
    void tick(uint32_t nowMs);

    uint8_t heldMask() const { return held_; }

private:
    uint8_t held_ = 0;
    uint32_t lastFrameMs_ = 0;
    uint32_t lastPressMs_[MFL_BUTTON_COUNT] = {};
};
