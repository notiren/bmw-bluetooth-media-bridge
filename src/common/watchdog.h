#pragma once
#include <Arduino.h>

// Task watchdog: the ESP32 restarts if a watched task stops calling
// watchdogFeed() for kWatchdogTimeoutS (e.g. stuck in a loop or deadlocked).

constexpr uint32_t kWatchdogTimeoutS = 8;

// Call once in setup(); also watches the calling task (Arduino loopTask).
void watchdogBegin();
// Watch the calling task too (call at the start of a task).
void watchdogWatchThisTask();
void watchdogFeed();

// Why the chip last restarted, e.g. "power-on", "task watchdog", "crash".
const char *resetReasonName();
// True if the last restart was a crash, watchdog or brownout.
bool resetWasFault();
