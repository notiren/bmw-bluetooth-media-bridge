#include "mfl.h"

namespace {
struct BitDef {
    uint8_t byte;
    uint8_t mask;
};

// Index = MflButton. Adjust here after verifying on the car.
constexpr BitDef kBits[MFL_BUTTON_COUNT] = {
    {0, 0x20},  // MFL_NEXT
    {0, 0x10},  // MFL_PREV
    {0, 0x08},  // MFL_VOL_UP
    {0, 0x04},  // MFL_VOL_DOWN
    {0, 0x01},  // MFL_PHONE  (DBC: Telephone, bit 0)
    {1, 0x01},  // MFL_VOICE  (DBC: VoiceControl, bit 8)
};

constexpr uint32_t kReleaseTimeoutMs = 500;  // no frame -> all released
constexpr uint32_t kMinRepressMs = 150;      // ignore bounce between frames

const char *const kNames[MFL_BUTTON_COUNT] = {
    "NEXT", "PREV", "VOL+", "VOL-", "PHONE", "VOICE"};

// Bitmask (1 << MflButton) of the buttons held in a valid 0x1D6 frame.
uint8_t buttonsIn(const CanFrame &frame) {
    uint8_t mask = 0;
    for (uint8_t i = 0; i < MFL_BUTTON_COUNT; ++i) {
        if (kBits[i].byte < frame.dlc && (frame.data[kBits[i].byte] & kBits[i].mask)) {
            mask |= 1 << i;
        }
    }
    return mask;
}
}  // namespace

const char *mflButtonName(MflButton b) {
    return b < MFL_BUTTON_COUNT ? kNames[b] : "?";
}

uint8_t MflDecoder::update(const CanFrame &frame, uint32_t nowMs) {
    if (frame.id != MFL_CAN_ID || frame.extended || frame.rtr) return 0;
    if (frame.dlc < 1 || frame.data[0] == 0xFF) return 0;  // keep last state on fault frames
    lastFrameMs_ = nowMs;

    const uint8_t now = buttonsIn(frame);
    uint8_t pressed = now & ~held_;
    for (uint8_t i = 0; i < MFL_BUTTON_COUNT; ++i) {
        if (!(pressed & (1 << i))) continue;
        if (nowMs - lastPressMs_[i] < kMinRepressMs) {
            pressed &= ~(1 << i);
        } else {
            lastPressMs_[i] = nowMs;
        }
    }
    held_ = now;
    return pressed;
}

void MflDecoder::tick(uint32_t nowMs) {
    if (held_ && nowMs - lastFrameMs_ > kReleaseTimeoutMs) held_ = 0;
}
