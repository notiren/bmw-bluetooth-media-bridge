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
// Returns true if a frame was received within timeoutMs.
bool canRead(CanFrame &frame, uint32_t timeoutMs);
const char *canBackendName();

// MCP2515 INT wire check: None = no INT pin configured (TWAI or polling),
// Ok = interrupts seen, Silent = no interrupt yet (unwired: polling instead).
enum class CanInt : uint8_t { None, Ok, Silent };
CanInt canIntStatus();
