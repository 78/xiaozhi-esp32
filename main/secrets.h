/**
 * @file secrets.h
 * @brief Private configuration file — DO NOT COMMIT TO GIT.
 *
 * This file is listed in .gitignore and should never be published.
 * Copy secrets.h.example to secrets.h and fill in your actual values.
 */

#pragma once

// ---------------------------------------------------------------------------
// Wi-Fi credentials (optional hardcoded defaults)
// ---------------------------------------------------------------------------
// Uncomment the lines below to bake your Wi-Fi into the firmware at compile
// time. This is convenient when deploying multiple devices on the same
// network. Without these defines, use xiaozhi built-in Config Mode instead:
// hold the Boot button while powering on to enter Wi-Fi setup via the AP.
#define SECRET_WIFI_SSID     "home4"
#define SECRET_WIFI_PASSWORD "P@$$vv0rd"

// ---------------------------------------------------------------------------
// Nanobot voice gateway
// ---------------------------------------------------------------------------
// WebSocket address of your Nanobot server and the shared auth token.
#define SECRET_WS_URL   "ws://192.168.22.102:18790"
#define SECRET_WS_TOKEN "xZ_seCrEt_729!"

// ---------------------------------------------------------------------------
// OTA update server
// ---------------------------------------------------------------------------
#define SECRET_OTA_URL  "http://192.168.22.102:18791/"
