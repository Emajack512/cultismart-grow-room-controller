#pragma once

// ---------- Blynk ----------
#define BLYNK_TEMPLATE_ID   "PUT_YOUR_TEMPLATE_ID"
#define BLYNK_TEMPLATE_NAME "PUT_YOUR_TEMPLATE_NAME"
#define BLYNK_AUTH_TOKEN    "PUT_YOUR_BLYNK_TOKEN"

// ---------- WiFi ----------
static const char* WIFI_SSID     = "PUT_YOUR_WIFI_SSID";
static const char* WIFI_PASSWORD = "PUT_YOUR_WIFI_PASSWORD";

// ---------- ESP-NOW MACs ----------
// MAC del TRANSMITTER (nodo que envía sensores y recibe setpoint)
static uint8_t ESPNOW_TX_MAC[6] = {0x00,0x00,0x00,0x00,0x00,0x00};

// MAC del CENTRAL (opcional, por si querés documentarlo)
static uint8_t ESPNOW_CENTRAL_MAC[6] = {0x00,0x00,0x00,0x00,0x00,0x00};
