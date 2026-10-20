#include "can_bus.h"

#include "driver/twai.h"

static bool s_installed = false;

bool canBegin(uint32_t bitrate) {
    if (s_installed) return true;
    twai_timing_config_t timing;
    switch (bitrate) {
        case 100000: timing = TWAI_TIMING_CONFIG_100KBITS(); break;
        case 125000: timing = TWAI_TIMING_CONFIG_125KBITS(); break;
        case 250000: timing = TWAI_TIMING_CONFIG_250KBITS(); break;
        case 500000: timing = TWAI_TIMING_CONFIG_500KBITS(); break;
        default: return false;
    }

    twai_general_config_t general = TWAI_GENERAL_CONFIG_DEFAULT(
        (gpio_num_t)CAN_TX_PIN, (gpio_num_t)CAN_RX_PIN, TWAI_MODE_LISTEN_ONLY);
    general.rx_queue_len = 64;
    twai_filter_config_t filter = TWAI_FILTER_CONFIG_ACCEPT_ALL();

    if (twai_driver_install(&general, &timing, &filter) != ESP_OK) return false;
    s_installed = true;
    return twai_start() == ESP_OK;
}

bool canRead(CanFrame &frame, uint32_t timeoutMs) {
    if (!s_installed) return false;
    twai_message_t msg;
    if (twai_receive(&msg, pdMS_TO_TICKS(timeoutMs)) != ESP_OK) return false;
    frame.id = msg.identifier;
    frame.extended = msg.extd;
    frame.rtr = msg.rtr;
    frame.dlc = msg.data_length_code > 8 ? 8 : msg.data_length_code;
    memcpy(frame.data, msg.data, frame.dlc);
    return true;
}

const char *canBackendName() { return "TWAI"; }

bool canGetStatus(CanStatus &out) {
    twai_status_info_t s;
    if (!s_installed || twai_get_status_info(&s) != ESP_OK) return false;
    out.running = s.state == TWAI_STATE_RUNNING;
    out.busErrors = s.bus_error_count;
    out.missed = s.rx_missed_count + s.rx_overrun_count;
    return true;
}
