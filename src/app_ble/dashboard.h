#pragma once
// Optional Wi-Fi dashboard + OTA updates for the BLE app.
// Build with -DENABLE_DASHBOARD=1 (see platformio.ini); set it to 0 to build
// the plain BLE buttons firmware without Wi-Fi.
//
// Wi-Fi is off at boot. Holding the Voice button for 2 s toggles it; it also
// turns off after 10 min without dashboard/OTA use.
// Wi-Fi: the C3 opens its own network (AP_SSID, include/secrets.h) without a
// default gateway, so the phone keeps mobile data for the internet.
// Dashboard address: http://192.168.4.1

#ifndef ENABLE_DASHBOARD
#define ENABLE_DASHBOARD 0
#endif

#if ENABLE_DASHBOARD
#include "../common/kcan_dash.h"

void dashboardBegin(const KcanDash &dash, const volatile bool &bleConnected);
void dashboardLoop();
void dashboardNoteButton(const char *name);
void dashboardToggleWifi();
bool dashboardWifiOn();
#endif
