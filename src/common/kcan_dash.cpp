#include "kcan_dash.h"

namespace {
// Each decoder returns false if the frame is too short or marked invalid.
using DecodeFn = bool (*)(const uint8_t *d, uint8_t dlc, float &out);

struct FieldDef {
    DashFieldInfo info;
    DecodeFn decode;
};

// ---- UNVERIFIED E9x decodings: confirm each with the logger ----

// 0x1B4 Kombi speed: 12 bit, low byte first, 1/16 km/h.
bool decodeSpeed(const uint8_t *d, uint8_t dlc, float &out) {
    if (dlc < 2) return false;
    out = (((d[1] & 0x0F) << 8) | d[0]) / 16.0f;
    return true;
}

// 0x0AA engine RPM: bytes 4-5, low byte first, 1/4 rpm.
bool decodeRpm(const uint8_t *d, uint8_t dlc, float &out) {
    if (dlc < 6) return false;
    const uint16_t raw = (d[5] << 8) | d[4];
    if (raw == 0xFFFF) return false;
    out = raw / 4.0f;
    return true;
}

// 0x1D0 engine data: byte 0 = coolant, offset -48 degC.
bool decodeCoolant(const uint8_t *d, uint8_t dlc, float &out) {
    if (dlc < 1 || d[0] == 0xFF) return false;
    out = (int)d[0] - 48;
    return true;
}

// 0x2CA outside temperature: byte 0, 0.5 degC per bit, offset -40 degC.
bool decodeOutside(const uint8_t *d, uint8_t dlc, float &out) {
    if (dlc < 1 || d[0] == 0xFF) return false;
    out = d[0] * 0.5f - 40.0f;
    return true;
}

// 0x3B4 power management: 12 bit battery voltage, 15 mV per bit.
bool decodeBattery(const uint8_t *d, uint8_t dlc, float &out) {
    if (dlc < 2) return false;
    const uint16_t raw = ((d[1] & 0x0F) << 8) | d[0];
    if (raw == 0x0FFF) return false;
    out = raw * 0.015f;
    return true;
}

// 0x349 fuel level: left + right tank sensor (16 bit each), 1/160 litre.
bool decodeFuel(const uint8_t *d, uint8_t dlc, float &out) {
    if (dlc < 4) return false;
    const uint16_t left = (d[1] << 8) | d[0];
    const uint16_t right = (d[3] << 8) | d[2];
    out = (left + right) / 160.0f;
    return true;
}

// 0x366 Kombi display: bytes 1-2 = range, upper 12 bits, km.
bool decodeRange(const uint8_t *d, uint8_t dlc, float &out) {
    if (dlc < 3) return false;
    out = (((d[2] << 8) | d[1]) >> 4);
    return true;
}

// Index = DashField.
const FieldDef kFields[DASH_FIELD_COUNT] = {
    {{"speed", "km/h", 0x1B4, 0}, decodeSpeed},
    {{"rpm", "rpm", 0x0AA, 0}, decodeRpm},
    {{"coolant", "°C", 0x1D0, 0}, decodeCoolant},
    {{"outside", "°C", 0x2CA, 1}, decodeOutside},
    {{"battery", "V", 0x3B4, 2}, decodeBattery},
    {{"fuel", "L", 0x349, 1}, decodeFuel},
    {{"range", "km", 0x366, 0}, decodeRange},
};
}  // namespace

const DashFieldInfo &KcanDash::info(DashField f) { return kFields[f].info; }

bool KcanDash::update(const CanFrame &frame, uint32_t nowMs) {
    ++totalFrames_;
    lastFrameMs_ = nowMs;
    if (frame.extended || frame.rtr) return false;

    bool used = false;
    for (uint8_t i = 0; i < DASH_FIELD_COUNT; ++i) {
        if (kFields[i].info.canId != frame.id) continue;
        DashValue &v = values_[i];
        v.dlc = frame.dlc;
        memcpy(v.raw, frame.data, frame.dlc);
        float out;
        if (kFields[i].decode(frame.data, frame.dlc, out)) {
            v.value = out;
            v.valid = true;
            v.updatedMs = nowMs;
        }
        used = true;
    }
    return used;
}
