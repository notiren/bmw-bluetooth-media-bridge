#pragma once
#include <Arduino.h>

// Thin CAN wrapper over the ESP32 built-in TWAI controller (needs a 3.3 V
// transceiver such as SN65HVD230).
//
// The bus is ALWAYS opened in listen-only mode: the node never transmits and
// never ACKs, so it cannot disturb the vehicle network.

struct CanFrame {
    uint32_t id;
    uint8_t dlc;
    bool extended;
    bool rtr;
    uint8_t data[8];
};

bool canBegin(uint32_t bitrate);
// Returns true if a frame was received within timeoutMs.
bool canRead(CanFrame &frame, uint32_t timeoutMs);
const char *canBackendName();

// Controller health for the dashboard. Bus errors climbing usually mean a
// wrong bitrate or swapped CANH/CANL; missed = RX queue overflowed.
struct CanStatus {
    bool running;
    uint32_t busErrors;
    uint32_t missed;
};
bool canGetStatus(CanStatus &out);
