// Copy this file to include/secrets.h and fill in your values. Required for
// both apps when the dashboard is on (ENABLE_DASHBOARD=1).
// include/secrets.h is git-ignored, so your passwords stay on your machine.
#pragma once

// The ESP32's own Wi-Fi network (dashboard at http://192.168.4.1).
// Password: at least 8 characters, or "" for an open network (the dashboard
// is read-only; OTA updates still need OTA_PASSWORD).
#define AP_SSID "BMW E92 Dash"
#define AP_PASSWORD "change-me-ap"

// Password for OTA firmware updates (the /update page or curl, user "admin").
// At least 8 characters.
#define OTA_PASSWORD "change-me-ota"
