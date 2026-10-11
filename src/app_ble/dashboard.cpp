#include "dashboard.h"

#if ENABLE_DASHBOARD
#include <ArduinoOTA.h>
#include <ESPmDNS.h>
#include <Update.h>
#include <WebServer.h>
#include <WiFi.h>
#include <esp_netif.h>
#include "dashboard_page.h"

#if __has_include("secrets.h")
#include "secrets.h"
#define DASH_HAVE_SECRETS 1
#else
#warning "include/secrets.h missing: Wi-Fi dashboard disabled. Copy include/secrets.example.h to include/secrets.h."
#define DASH_HAVE_SECRETS 0
#endif

#ifndef FW_VERSION
#define FW_VERSION "dev"
#endif
#ifndef DASH_HOSTNAME
#define DASH_HOSTNAME "e92"  // -> http://e92.local
#endif
#ifndef DASH_WIFI_AT_BOOT
#define DASH_WIFI_AT_BOOT 0  // 1 = Wi-Fi on at power-up (testing)
#endif
#ifndef AP_SSID
#define AP_SSID "BMW E92 Dash"
#endif

#if DASH_HAVE_SECRETS
static_assert(sizeof(AP_PASSWORD) == 1 || sizeof(AP_PASSWORD) >= 9,
              "AP_PASSWORD must be empty (open network) or at least 8 characters");
static_assert(sizeof(OTA_PASSWORD) >= 9, "OTA_PASSWORD needs at least 8 characters");
#endif

namespace {
constexpr uint32_t kIdleOffMs = 10UL * 60 * 1000;  // nobody uses the dashboard -> Wi-Fi off

const KcanDash *g_dash = nullptr;
const volatile bool *g_ble = nullptr;
WebServer g_server(80);
bool g_started = false;
bool g_wifiOn = false;
uint32_t g_lastUseMs = 0;

const char *g_btnName = nullptr;
uint32_t g_btnMs = 0;

uint32_t g_fpsWindowMs = 0;
uint32_t g_fpsWindowFrames = 0;
uint32_t g_fps = 0;

bool g_updateAuthorized = false;

// CAN sniffer: one entry per seen ID.
struct SniffEntry {
    uint32_t id;
    bool ext;
    uint8_t dlc;
    uint8_t data[8];
    uint32_t count;
    uint32_t lastMs;
    uint32_t changedMs[8];
};
constexpr uint16_t kSniffMax = 160;
constexpr uint32_t kChangedWindowMs = 1000;
SniffEntry g_sniff[kSniffMax];
uint16_t g_sniffCount = 0;
bool g_sniffFull = false;

void handleCanApi() {
    g_lastUseMs = millis();
    if (g_server.hasArg("reset")) {
        g_sniffCount = 0;
        g_sniffFull = false;
    }
    const uint32_t now = millis();
    String j;
    j.reserve(64 + g_sniffCount * 48);
    j += "{\"full\":";
    j += g_sniffFull ? "true" : "false";
    {
        const CanIntStats s = canIntStats();
        char ib[112];
        // [has INT, irqs, frames, polled, error clears, pin high]
        snprintf(ib, sizeof(ib), ",\"int\":[%d,%lu,%lu,%lu,%lu,%d]", s.hasInt ? 1 : 0,
                 (unsigned long)s.irqs, (unsigned long)s.frames, (unsigned long)s.polled,
                 (unsigned long)s.errClears, s.pinHigh ? 1 : 0);
        j += ib;
    }
    j += ",\"ids\":[";
    char buf[96];
    for (uint16_t i = 0; i < g_sniffCount; ++i) {
        const SniffEntry &e = g_sniff[i];
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
    g_server.sendHeader("Cache-Control", "no-store");
    g_server.send(200, "application/json", j);
}

int32_t ageOf(uint32_t stampMs, bool valid, uint32_t now) {
    return valid ? (int32_t)(now - stampMs) : -1;
}

void handleApi() {
    const uint32_t now = millis();
    g_lastUseMs = now;
    String j;
    j.reserve(1200);
    j += "{\"ver\":\"" FW_VERSION "\",\"net\":\"";
    j += AP_SSID;
    j += "\",\"clients\":";
    j += WiFi.softAPgetStationNum();
    j += ",\"ble\":";
    j += (g_ble && *g_ble) ? "true" : "false";
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
    g_server.sendHeader("Cache-Control", "no-store");
    g_server.send(200, "application/json", j);
}

#if DASH_HAVE_SECRETS
bool checkAuth() {
    g_lastUseMs = millis();
    if (g_server.authenticate("admin", OTA_PASSWORD)) return true;
    g_server.requestAuthentication();
    return false;
}

void handleUpdateUpload() {
    HTTPUpload &up = g_server.upload();
    g_lastUseMs = millis();
    if (up.status == UPLOAD_FILE_START) {
        g_updateAuthorized = g_server.authenticate("admin", OTA_PASSWORD);
        if (!g_updateAuthorized) return;
        Serial.printf("OTA(web): receiving %s\n", up.filename.c_str());
        if (!Update.begin(UPDATE_SIZE_UNKNOWN)) Update.printError(Serial);
    } else if (!g_updateAuthorized) {
        return;
    } else if (up.status == UPLOAD_FILE_WRITE) {
        if (Update.write(up.buf, up.currentSize) != up.currentSize) Update.printError(Serial);
    } else if (up.status == UPLOAD_FILE_END) {
        if (Update.end(true)) {
            Serial.printf("OTA(web): %u bytes OK\n", up.totalSize);
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
                  ok ? "Update OK, restarting..." : "Update FAILED, see serial log");
    if (ok) {
        delay(500);
        ESP.restart();
    }
}

void otaBegin() {
    ArduinoOTA.setHostname(DASH_HOSTNAME);
    ArduinoOTA.setPassword(OTA_PASSWORD);
    ArduinoOTA.onStart([] {
        g_lastUseMs = millis();
        Serial.println("OTA: start");
    });
    ArduinoOTA.onEnd([] { Serial.println("OTA: done, restarting"); });
    ArduinoOTA.onError([](ota_error_t e) { Serial.printf("OTA: error %u\n", e); });
    ArduinoOTA.begin();  // also starts mDNS as DASH_HOSTNAME.local
    MDNS.addService("http", "tcp", 80);
}

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
    // Modem sleep stays on: required for Wi-Fi and BLE to share the radio.
    WiFi.mode(WIFI_AP);
    WiFi.softAP(AP_SSID, sizeof(AP_PASSWORD) > 1 ? AP_PASSWORD : nullptr);
    apNoGateway();
    // The phone is within a metre or two: low TX power, less heat.
    WiFi.setTxPower(WIFI_POWER_8_5dBm);
    g_lastUseMs = millis();
    g_server.begin();
    otaBegin();
    g_wifiOn = true;
    Serial.printf("WiFi: on, network \"%s\" at http://%s\n", AP_SSID,
                  WiFi.softAPIP().toString().c_str());
}

void wifiOff(const char *why) {
    ArduinoOTA.end();  // also stops mDNS
    g_server.stop();
    WiFi.softAPdisconnect(false);
    WiFi.mode(WIFI_OFF);
    g_wifiOn = false;
    Serial.printf("WiFi: off (%s)\n", why);
}
#endif  // DASH_HAVE_SECRETS
}  // namespace

void dashboardBegin(const KcanDash &dash, const volatile bool &bleConnected) {
    g_dash = &dash;
    g_ble = &bleConnected;
#if DASH_HAVE_SECRETS
    g_server.on("/", HTTP_GET, [] { g_server.send_P(200, "text/html", kDashboardPage); });
    g_server.on("/api", HTTP_GET, handleApi);
    g_server.on("/can", HTTP_GET, [] { g_server.send_P(200, "text/html", kSnifferPage); });
    g_server.on("/canapi", HTTP_GET, handleCanApi);
    g_server.on("/update", HTTP_GET, [] {
        if (checkAuth()) g_server.send_P(200, "text/html", kUpdatePage);
    });
    g_server.on("/update", HTTP_POST, handleUpdateDone, handleUpdateUpload);
    g_server.onNotFound([] { g_server.send(404, "text/plain", "not found"); });
    g_started = true;
#if DASH_WIFI_AT_BOOT
    wifiOn();
#else
    Serial.println("WiFi: off (hold Voice 2 s to turn on)");
#endif
#else
    Serial.println("Dashboard: include/secrets.h missing, Wi-Fi disabled");
#endif
}

void dashboardToggleWifi() {
#if DASH_HAVE_SECRETS
    if (!g_started) return;
    if (g_wifiOn) {
        wifiOff("toggled");
    } else {
        wifiOn();
    }
#endif
}

bool dashboardWifiOn() { return g_wifiOn; }

void dashboardLoop() {
    if (!g_started) return;
    const uint32_t now = millis();
    if (now - g_fpsWindowMs >= 1000) {
        const uint32_t total = g_dash->totalFrames();
        g_fps = (total - g_fpsWindowFrames) * 1000 / (now - g_fpsWindowMs);
        g_fpsWindowFrames = total;
        g_fpsWindowMs = now;
    }
#if DASH_HAVE_SECRETS
    if (!g_wifiOn) return;
    if (now - g_lastUseMs > kIdleOffMs && !Update.isRunning()) {
        wifiOff("10 min unused");
        return;
    }
    ArduinoOTA.handle();
    g_server.handleClient();
#endif
}

void dashboardNoteFrame(const CanFrame &f, uint32_t nowMs) {
    if (!g_wifiOn || f.rtr) return;
    const uint8_t dlc = f.dlc > 8 ? 8 : f.dlc;
    SniffEntry *e = nullptr;
    for (uint16_t i = 0; i < g_sniffCount; ++i) {
        if (g_sniff[i].id == f.id && g_sniff[i].ext == f.extended) {
            e = &g_sniff[i];
            break;
        }
    }
    if (!e) {
        if (g_sniffCount >= kSniffMax) {
            g_sniffFull = true;
            return;
        }
        e = &g_sniff[g_sniffCount++];
        memset(e, 0, sizeof(*e));
        e->id = f.id;
        e->ext = f.extended;
        e->dlc = dlc;
        memcpy(e->data, f.data, dlc);
    } else {
        for (uint8_t b = 0; b < dlc; ++b) {
            if (b >= e->dlc || e->data[b] != f.data[b]) e->changedMs[b] = nowMs ? nowMs : 1;
        }
        e->dlc = dlc;
        memcpy(e->data, f.data, dlc);
    }
    e->count++;
    e->lastMs = nowMs;
}

void dashboardNoteButton(const char *name) {
    g_btnName = name;
    g_btnMs = millis();
}

#endif  // ENABLE_DASHBOARD
