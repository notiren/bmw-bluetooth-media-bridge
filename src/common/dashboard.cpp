#include "dashboard.h"

#if ENABLE_DASHBOARD
#include <Update.h>
#include <WebServer.h>
#include <WiFi.h>
#include <esp_netif.h>
#include "can_id_table.h"
#include "dashboard_page.h"
#include "watchdog.h"

#if !__has_include("secrets.h")
#error "include/secrets.h missing: copy include/secrets.example.h to include/secrets.h and set your passwords (or build with -DENABLE_DASHBOARD=0)."
#endif
#include "secrets.h"

#ifndef FW_VERSION
#define FW_VERSION "dev"
#endif
#ifndef DASH_WIFI_AT_BOOT
#define DASH_WIFI_AT_BOOT 0  // 1 = Wi-Fi on at power-up (testing)
#endif

static_assert(sizeof(AP_PASSWORD) == 1 || sizeof(AP_PASSWORD) >= 9,
              "AP_PASSWORD must be empty (open network) or at least 8 characters");
static_assert(sizeof(OTA_PASSWORD) >= 9, "OTA_PASSWORD needs at least 8 characters");

namespace {
constexpr uint32_t kIdleOffMs = 10UL * 60 * 1000;  // nobody uses the dashboard -> Wi-Fi off
constexpr uint32_t kChangedWindowMs = 1000;         // sniffer: highlight bytes changed this recently

const KcanDash *g_dash = nullptr;
const volatile bool *g_phone = nullptr;
WebServer g_server(80);
bool g_wifiOn = false;
uint32_t g_lastUseMs = 0;

const char *g_btnName = nullptr;
uint32_t g_btnMs = 0;

uint32_t g_fpsWindowMs = 0;
uint32_t g_fpsWindowFrames = 0;
uint32_t g_fps = 0;

bool g_updateAuthorized = false;

CanIdTable g_sniff;

int32_t ageOf(uint32_t stampMs, bool valid, uint32_t now) {
    return valid ? (int32_t)(now - stampMs) : -1;
}

void sendJson(const String &j) {
    g_server.sendHeader("Cache-Control", "no-store");
    g_server.send(200, "application/json", j);
}

// /canapi: every seen ID for the sniffer page.
void handleCanApi() {
    g_lastUseMs = millis();
    if (g_server.hasArg("reset")) g_sniff.clear();
    const uint32_t now = millis();

    String j;
    j.reserve(64 + g_sniff.size() * 48);
    j += "{\"full\":";
    j += g_sniff.full() ? "true" : "false";
    CanStatus st;
    if (canGetStatus(st)) {
        j += ",\"bus\":[";
        j += st.running ? "1," : "0,";
        j += st.busErrors;
        j += ',';
        j += st.missed;
        j += ']';
    }
    j += ",\"ids\":[";
    char buf[96];
    for (uint16_t i = 0; i < g_sniff.size(); ++i) {
        const CanIdEntry &e = g_sniff[i];
        uint8_t chg = 0;
        char hex[8 * 2 + 1];
        for (uint8_t b = 0; b < e.dlc; ++b) {
            snprintf(hex + b * 2, 3, "%02X", e.data[b]);
            if (e.changedMs[b] && now - e.changedMs[b] < kChangedWindowMs) chg |= 1 << b;
        }
        hex[e.dlc * 2] = 0;
        // [id, ext, data hex, changed-byte mask, count, age ms]
        snprintf(buf, sizeof(buf), "%s[%lu,%d,\"%s\",%u,%lu,%lu]", i ? "," : "",
                 (unsigned long)e.id, e.ext ? 1 : 0, hex, chg, (unsigned long)e.count,
                 (unsigned long)(now - e.lastMs));
        j += buf;
    }
    j += "]}";
    sendJson(j);
}

// /api: dashboard values and status.
void handleApi() {
    const uint32_t now = millis();
    g_lastUseMs = now;
    String j;
    j.reserve(1200);
    j += "{\"ver\":\"" FW_VERSION "\",\"net\":\"" AP_SSID "\",\"reset\":\"";
    j += resetReasonName();
    j += "\",\"resetFault\":";
    j += resetWasFault() ? "true" : "false";
    j += ",\"clients\":";
    j += WiFi.softAPgetStationNum();
    j += ",\"phone\":";
    j += (g_phone && *g_phone) ? "true" : "false";
    j += ",\"fps\":";
    j += g_fps;
    j += ",\"canAge\":";
    j += ageOf(g_dash->lastFrameMs(), g_dash->totalFrames() > 0, now);
    j += ",\"btn\":";
    if (g_btnName) {
        j += '"';
        j += g_btnName;
        j += '"';
    } else {
        j += "null";
    }
    j += ",\"btnAge\":";
    j += ageOf(g_btnMs, g_btnName != nullptr, now);
    j += ",\"f\":[";
    for (uint8_t i = 0; i < DASH_FIELD_COUNT; ++i) {
        const DashFieldInfo &inf = KcanDash::info((DashField)i);
        const DashValue &v = g_dash->value((DashField)i);
        char id[8], raw[8 * 3 + 1] = "";
        snprintf(id, sizeof(id), "%03lX", (unsigned long)inf.canId);
        size_t pos = 0;
        for (uint8_t b = 0; b < v.dlc; ++b) {
            pos += snprintf(raw + pos, sizeof(raw) - pos, b ? " %02X" : "%02X", v.raw[b]);
        }
        if (i) j += ',';
        j += "{\"k\":\"";
        j += inf.key;
        j += "\",\"u\":\"";
        j += inf.unit;
        j += "\",\"d\":";
        j += inf.decimals;
        j += ",\"id\":\"";
        j += id;
        j += "\",\"v\":";
        j += v.valid ? String(v.value, (unsigned)inf.decimals) : String("null");
        j += ",\"age\":";
        j += ageOf(v.updatedMs, v.valid, now);
        j += ",\"raw\":\"";
        j += raw;
        j += "\"}";
    }
    j += "]}";
    sendJson(j);
}

// ---- OTA firmware update via /update (browser or curl), user "admin" ----

bool checkAuth() {
    g_lastUseMs = millis();
    if (g_server.authenticate("admin", OTA_PASSWORD)) return true;
    g_server.requestAuthentication();
    return false;
}

void handleUpdateUpload() {
    HTTPUpload &up = g_server.upload();
    g_lastUseMs = millis();
    // The whole upload runs inside one loop() pass and can take longer than
    // the watchdog timeout.
    watchdogFeed();
    if (up.status == UPLOAD_FILE_START) {
        g_updateAuthorized = g_server.authenticate("admin", OTA_PASSWORD);
        if (!g_updateAuthorized) return;
        Serial.printf("OTA: receiving %s\n", up.filename.c_str());
        if (!Update.begin(UPDATE_SIZE_UNKNOWN)) Update.printError(Serial);
    } else if (!g_updateAuthorized) {
        return;
    } else if (up.status == UPLOAD_FILE_WRITE) {
        if (Update.write(up.buf, up.currentSize) != up.currentSize) Update.printError(Serial);
    } else if (up.status == UPLOAD_FILE_END) {
        if (Update.end(true)) {
            Serial.printf("OTA: %u bytes OK\n", up.totalSize);
        } else {
            Update.printError(Serial);
        }
    } else if (up.status == UPLOAD_FILE_ABORTED) {
        Update.abort();
    }
}

void handleUpdateDone() {
    if (!g_updateAuthorized) {
        g_server.requestAuthentication();
        return;
    }
    const bool ok = !Update.hasError();
    g_server.sendHeader("Connection", "close");
    g_server.send(ok ? 200 : 500, "text/plain",
                  ok ? "Update OK, restarting...\n" : "Update FAILED, see serial log\n");
    if (ok) {
        delay(500);
        ESP.restart();
    }
}

// ---- Wi-Fi ----

// Don't offer a default gateway via DHCP: the phone then keeps using mobile
// data for the internet and only reaches 192.168.4.x over this network.
void apNoGateway() {
    esp_netif_t *ap = esp_netif_get_handle_from_ifkey("WIFI_AP_DEF");
    if (!ap) return;
    uint8_t offerRouter = 0;
    esp_netif_dhcps_stop(ap);
    esp_netif_dhcps_option(ap, ESP_NETIF_OP_SET, ESP_NETIF_ROUTER_SOLICITATION_ADDRESS,
                           &offerRouter, sizeof(offerRouter));
    esp_netif_dhcps_start(ap);
}

void wifiOn() {
    // Modem sleep stays on: required for Wi-Fi and Bluetooth to share the radio.
    WiFi.mode(WIFI_AP);
    WiFi.softAP(AP_SSID, sizeof(AP_PASSWORD) > 1 ? AP_PASSWORD : nullptr);
    apNoGateway();
    // The phone is within a metre or two: low TX power, less heat.
    WiFi.setTxPower(WIFI_POWER_8_5dBm);
    g_lastUseMs = millis();
    g_server.begin();
    g_wifiOn = true;
    Serial.printf("WiFi: on, network \"%s\" at http://%s\n", AP_SSID,
                  WiFi.softAPIP().toString().c_str());
}

void wifiOff(const char *why) {
    g_server.stop();
    WiFi.softAPdisconnect(false);
    WiFi.mode(WIFI_OFF);
    g_wifiOn = false;
    Serial.printf("WiFi: off (%s)\n", why);
}
}  // namespace

void dashboardBegin(const KcanDash &dash, const volatile bool &phoneConnected) {
    g_dash = &dash;
    g_phone = &phoneConnected;
    g_server.on("/", HTTP_GET, [] { g_server.send_P(200, "text/html", kDashboardPage); });
    g_server.on("/api", HTTP_GET, handleApi);
    g_server.on("/can", HTTP_GET, [] { g_server.send_P(200, "text/html", kSnifferPage); });
    g_server.on("/canapi", HTTP_GET, handleCanApi);
    g_server.on("/update", HTTP_GET, [] {
        if (checkAuth()) g_server.send_P(200, "text/html", kUpdatePage);
    });
    g_server.on("/update", HTTP_POST, handleUpdateDone, handleUpdateUpload);
    g_server.onNotFound([] { g_server.send(404, "text/plain", "not found"); });
#if DASH_WIFI_AT_BOOT
    wifiOn();
#else
    Serial.println("WiFi: off (hold Voice 2 s to turn on)");
#endif
}

void dashboardToggleWifi() {
    if (!g_dash) return;
    if (g_wifiOn) {
        wifiOff("toggled");
    } else {
        wifiOn();
    }
}

bool dashboardWifiOn() { return g_wifiOn; }

void dashboardLoop() {
    if (!g_dash) return;
    const uint32_t now = millis();
    if (now - g_fpsWindowMs >= 1000) {
        const uint32_t total = g_dash->totalFrames();
        g_fps = (total - g_fpsWindowFrames) * 1000 / (now - g_fpsWindowMs);
        g_fpsWindowFrames = total;
        g_fpsWindowMs = now;
    }
    if (!g_wifiOn) return;
    if (now - g_lastUseMs > kIdleOffMs && !Update.isRunning()) {
        wifiOff("10 min unused");
        return;
    }
    g_server.handleClient();
}

void dashboardNoteFrame(const CanFrame &f, uint32_t nowMs) {
    if (g_wifiOn && !f.rtr) g_sniff.note(f, nowMs);
}

void dashboardNoteButton(const char *name) {
    g_btnName = name;
    g_btnMs = millis();
}

#endif  // ENABLE_DASHBOARD
