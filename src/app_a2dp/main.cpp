// CLASSIC ESP32 (WROOM-32) main firmware - does not build for S3/C3.
// - Bluetooth A2DP sink: phone streams music to the ESP32.
// - I2S -> PCM5102 DAC -> 3.5 mm AUX in the center console.
// - Steering-wheel buttons (0x1D6, K-CAN, listen-only) -> AVRCP commands back
//   to the phone over the same Bluetooth connection (no HID needed).
#include <Arduino.h>
#include "AudioTools.h"
#include "BluetoothA2DPSink.h"
#include "../common/can_bus.h"
#include "../common/mfl.h"
#include "../common/watchdog.h"

#if !defined(CONFIG_IDF_TARGET_ESP32)
#error "app_a2dp needs a classic ESP32: ESP32-S3/C3 have no Bluetooth Classic (A2DP)."
#endif

#ifndef CAN_BITRATE
#define CAN_BITRATE 100000
#endif
#ifndef BT_DEVICE_NAME
#define BT_DEVICE_NAME "BMW E92 Audio"
#endif
#ifndef I2S_BCK_PIN
#define I2S_BCK_PIN 26
#endif
#ifndef I2S_WS_PIN
#define I2S_WS_PIN 25
#endif
#ifndef I2S_DATA_PIN
#define I2S_DATA_PIN 22
#endif

static I2SStream g_i2s;
static BluetoothA2DPSink g_a2dp(g_i2s);
static MflDecoder g_mfl;
static volatile bool g_playing = false;

static void onAudioState(esp_a2d_audio_state_t state, void *) {
    g_playing = (state == ESP_A2D_AUDIO_STATE_STARTED);
}

static void handlePress(MflButton b) {
    if (!g_a2dp.is_connected()) {
        Serial.printf("MFL %s (no phone connected)\n", mflButtonName(b));
        return;
    }
    switch (b) {
        case MFL_NEXT: g_a2dp.next(); break;
        case MFL_PREV: g_a2dp.previous(); break;
        case MFL_VOICE:
        case MFL_PHONE:
            if (g_playing) g_a2dp.pause(); else g_a2dp.play();
            break;
        default: return;  // volume stays with the factory radio
    }
    Serial.printf("MFL %s -> AVRCP\n", mflButtonName(b));
}

void setup() {
    Serial.begin(115200);
    delay(500);
    Serial.printf("Reset: %s\n", resetReasonName());
    watchdogBegin();

    auto cfg = g_i2s.defaultConfig(TX_MODE);
    cfg.pin_bck = I2S_BCK_PIN;
    cfg.pin_ws = I2S_WS_PIN;
    cfg.pin_data = I2S_DATA_PIN;
    g_i2s.begin(cfg);

    g_a2dp.set_on_audio_state_changed(onAudioState);
    g_a2dp.set_auto_reconnect(true);
    g_a2dp.start(BT_DEVICE_NAME);
    Serial.printf("A2DP sink started as \"%s\"\n", BT_DEVICE_NAME);

    if (!canBegin(CAN_BITRATE)) {
        Serial.println("!! CAN init failed");
    } else {
        Serial.printf("CAN: %s @ %lu bit/s listen-only\n", canBackendName(),
                      (unsigned long)CAN_BITRATE);
    }
}

void loop() {
    watchdogFeed();
    CanFrame f;
    const uint32_t now = millis();
    if (canRead(f, 10)) {
        const uint8_t pressed = g_mfl.update(f, now);
        for (uint8_t i = 0; i < MFL_BUTTON_COUNT; ++i) {
            if (pressed & (1 << i)) handlePress((MflButton)i);
        }
    }
    g_mfl.tick(now);
}
