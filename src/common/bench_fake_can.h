#pragma once
// Bench test without a car (-DBENCH_FAKE_CAN=1): a task that feeds synthetic
// K-CAN frames into the CAN queue instead of the real transceiver.
#if BENCH_FAKE_CAN
void benchFakeCanTask(void *queue);  // queue: QueueHandle_t of CanFrame
#endif
