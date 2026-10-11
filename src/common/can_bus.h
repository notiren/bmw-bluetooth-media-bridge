#pragma once
#include <Arduino.h>

// Thin CAN abstraction over either the ESP32 built-in TWAI controller
// (default, needs a 3.3 V transceiver such as SN65HVD230) or an MCP2515 module
// (build flag CAN_BACKEND_MCP2515=1).
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
void canEnd();
// Returns true if a frame was received within timeoutMs.
bool canRead(CanFrame &frame, uint32_t timeoutMs);
const char *canBackendName();

// MCP2515 INT diagnostics. hasInt is false when no INT pin is configured.
struct CanIntStats {
    bool hasInt;
    uint32_t irqs;       // INT falling edges seen
    uint32_t frames;     // frames read
    uint32_t polled;     // frames only found by the 10 ms fallback poll (INT missed)
    uint32_t errClears;  // MCP2515 error flags cleared
    bool pinHigh;        // current INT level (idle = high)
};
CanIntStats canIntStats();
