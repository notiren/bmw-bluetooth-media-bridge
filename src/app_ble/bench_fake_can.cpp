// Bench test without a car (-DBENCH_FAKE_CAN=1): replaces the MCP2515 with
// synthetic K-CAN frames, fed into the same queue as the real CAN task.
#if BENCH_FAKE_CAN
#include <Arduino.h>
#include "../common/can_bus.h"

void benchFakeCanTask(void *queue) {
    QueueHandle_t q = (QueueHandle_t)queue;
    for (;;) {
        const uint32_t now = millis();
        const uint16_t spd = (uint16_t)((now / 100) % 2000);  // 0..125 km/h * 16
        const uint16_t rpm = (uint16_t)(3200 + (now / 10) % 12000);
        const CanFrame frames[] = {
            {0x1B4, 8, false, false, {(uint8_t)spd, (uint8_t)(0xC0 | (spd >> 8)), 0, 0, 0, 0, 0, 0}},
            {0x0AA, 8, false, false, {0, 0, 0, 0, (uint8_t)rpm, (uint8_t)(rpm >> 8), 0, 0}},
            {0x1D0, 8, false, false, {138, 0, 0, 0, 0, 0, 0, 0}},
            {0x3B4, 8, false, false, {0x40, 0xF3, 0, 0, 0, 0, 0, 0}},
            {0x1D6, 2, false, false, {0xC0, 0x0C}},
            {0x130, 5, false, false, {0x45, 0x40, 0x21, 0x8F, (uint8_t)(now / 1000)}},
        };
        for (const CanFrame &f : frames) xQueueSend(q, &f, 0);
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}
#endif
