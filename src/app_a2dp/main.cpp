// CLASSIC ESP32 (WROOM-32) main firmware - does not build for S3/C3.
// - Bluetooth A2DP sink: phone streams music to the ESP32.
// - I2S -> PCM5102 DAC -> 3.5 mm AUX in the center console.
// - Steering-wheel buttons (0x1D6, K-CAN, listen-only) -> AVRCP commands back
//   to the phone over the same Bluetooth connection (no HID needed).
//
// Voice acts on release: short press = Play/Pause, hold 2 s = Wi-Fi on/off
// (Wi-Fi dashboard + OTA in common/dashboard.cpp, -DENABLE_DASHBOARD=1).
#include <Arduino.h>
#include "AudioTools.h"
#include "BluetoothA2DPSink.h"
#include "../common/bench_fake_can.h"
#include "../common/can_bus.h"
#include "../common/dashboard.h"
#include "../common/kcan_dash.h"
#include "../common/mfl.h"
#include "../common/watchdog.h"

#if !defined(CONFIG_IDF_TARGET_ESP32)
#error "app_a2dp needs a classic ESP32: ESP32-S3/C3 have no Bluetooth Classic (A2DP)."
#endif

#ifndef CAN_BITRATE
#define CAN_BITRATE 100000
#endif
#ifndef FW_VERSION
#define FW_VERSION "dev"
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
#define I2S_DATA_PIN 33
#endif

static constexpr uint32_t kVoiceHoldMs = 2000;

static I2SStream g_i2s;
static BluetoothA2DPSink g_a2dp(g_i2s);
static MflDecoder g_mfl;
static KcanDash g_dash;
static volatile bool g_playing = false;
static volatile bool g_connected = false;
static bool g_voicePending = false;
static uint32_t g_voiceDownMs = 0;

// CAN is read in its own task so Bluetooth and Wi-Fi work in loop() cannot
// make the controller's small receive buffer overflow.
static QueueHandle_t g_canQueue = nullptr;

static void canTask(void *) {
    watchdogWatchThisTask();
    CanFrame f;
    for (;;) {
        watchdogFeed();
        if (canRead(f, 5)) xQueueSend(g_canQueue, &f, 0);
    }
}

static void onAudioState(esp_a2d_audio_state_t state, void *) {
    g_playing = (state == ESP_A2D_AUDIO_STATE_STARTED);
}

static void onConnState(esp_a2d_connection_state_t state, void *) {
    g_connected = (state == ESP_A2D_CONNECTION_STATE_CONNECTED);
    Serial.printf("A2DP: %s\n", g_connected ? "phone connected" : "disconnected");
}

static void runAction(MflButton b) {
    if (b == MFL_VOL_UP || b == MFL_VOL_DOWN) return;  // volume stays with the factory radio
    dashboardNoteButton(mflButtonName(b));
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
        default: return;
    }
    Serial.printf("MFL %s -> AVRCP\n", mflButtonName(b));
}

static void handlePress(MflButton b, uint32_t now) {
    if (b == MFL_VOICE) {  // decided on release or after the hold time
        g_voicePending = true;
        g_voiceDownMs = now;
        return;
    }
    runAction(b);
}

static void voiceHoldCheck(uint32_t now) {
    if (!g_voicePending) return;
    const bool held = g_mfl.heldMask() & (1 << MFL_VOICE);
    if (held && now - g_voiceDownMs >= kVoiceHoldMs) {
        g_voicePending = false;
        dashboardToggleWifi();
        dashboardNoteButton(dashboardWifiOn() ? "WIFI ON" : "WIFI OFF");
    } else if (!held) {
        g_voicePending = false;
        runAction(MFL_VOICE);
    }
}

// Bench test over the serial monitor: n/p/t = Next/Previous/Play-Pause,
// w = Wi-Fi on/off.
static void benchCommands() {
    if (!Serial.available()) return;
    switch (Serial.read()) {
        case 'n': runAction(MFL_NEXT); break;
        case 'p': runAction(MFL_PREV); break;
        case 't': runAction(MFL_PHONE); break;
        case 'w': dashboardToggleWifi(); break;
        default: break;
    }
}

void setup() {
    Serial.begin(115200);
    delay(500);
    Serial.println("Firmware v" FW_VERSION);
    Serial.printf("Reset: %s\n", resetReasonName());
    watchdogBegin();

    auto cfg = g_i2s.defaultConfig(TX_MODE);
    cfg.pin_bck = I2S_BCK_PIN;
    cfg.pin_ws = I2S_WS_PIN;
    cfg.pin_data = I2S_DATA_PIN;
    g_i2s.begin(cfg);

    g_a2dp.set_on_audio_state_changed(onAudioState);
    g_a2dp.set_on_connection_state_changed(onConnState);
    g_a2dp.set_auto_reconnect(true);
    g_a2dp.start(BT_DEVICE_NAME);
    Serial.printf("A2DP sink started as \"%s\"\n", BT_DEVICE_NAME);

    g_canQueue = xQueueCreate(64, sizeof(CanFrame));
#if BENCH_FAKE_CAN
    Serial.println("CAN: fake frames (BENCH_FAKE_CAN)");
    xTaskCreate(benchFakeCanTask, "fakecan", 4096, g_canQueue, 2, nullptr);
#else
    if (canBegin(CAN_BITRATE)) {
        Serial.printf("CAN: %s @ %lu bit/s listen-only\n", canBackendName(),
                      (unsigned long)CAN_BITRATE);
        xTaskCreate(canTask, "can", 4096, nullptr, 2, nullptr);
    } else {
        Serial.println("!! CAN init failed");
    }
#endif
    dashboardBegin(g_dash, g_connected);
}

void loop() {
    watchdogFeed();
    CanFrame f;
    // Wait briefly for the first frame, then drain what is queued (bounded so
    // Wi-Fi/web work still runs on a busy bus).
    TickType_t wait = pdMS_TO_TICKS(10);
    for (int n = 0; n < 64 && xQueueReceive(g_canQueue, &f, wait) == pdTRUE; ++n) {
        wait = 0;
        const uint32_t now = millis();
        const uint8_t pressed = g_mfl.update(f, now);
        for (uint8_t i = 0; i < MFL_BUTTON_COUNT; ++i) {
            if (pressed & (1 << i)) handlePress((MflButton)i, now);
        }
        g_dash.update(f, now);
        dashboardNoteFrame(f, now);
    }
    const uint32_t now = millis();
    g_mfl.tick(now);
    voiceHoldCheck(now);
    benchCommands();
    dashboardLoop();
}
