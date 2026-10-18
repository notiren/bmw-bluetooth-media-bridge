#include "watchdog.h"
#include <esp_system.h>
#include <esp_task_wdt.h>

void watchdogBegin() {
    // The core starts the task watchdog with 5 s; give loop() some more
    // headroom (Wi-Fi start, Bluetooth) and restart the chip when it fires.
    esp_task_wdt_init(kWatchdogTimeoutS, true);
    watchdogWatchThisTask();
}

void watchdogWatchThisTask() { esp_task_wdt_add(nullptr); }

void watchdogFeed() { esp_task_wdt_reset(); }

const char *resetReasonName() {
    switch (esp_reset_reason()) {
        case ESP_RST_POWERON: return "power-on";
        case ESP_RST_SW: return "restart";
        case ESP_RST_PANIC: return "crash";
        case ESP_RST_INT_WDT: return "interrupt watchdog";
        case ESP_RST_TASK_WDT: return "task watchdog";
        case ESP_RST_WDT: return "watchdog";
        case ESP_RST_BROWNOUT: return "brownout";
        case ESP_RST_EXT: return "reset pin";
        case ESP_RST_DEEPSLEEP: return "deep sleep";
        default: return "other";
    }
}

bool resetWasFault() {
    switch (esp_reset_reason()) {
        case ESP_RST_PANIC:
        case ESP_RST_INT_WDT:
        case ESP_RST_TASK_WDT:
        case ESP_RST_WDT:
        case ESP_RST_BROWNOUT: return true;
        default: return false;
    }
}
