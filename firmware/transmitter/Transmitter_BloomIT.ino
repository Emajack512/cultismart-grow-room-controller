/*
  CultiSmart - Transmitter node (ESP32)

  - Reads temperature/humidity from two DHT sensors (DHT11)
  - Sends SensorData to Central via ESP-NOW
  - Receives setpoint from Central via ESP-NOW
  - Controls two air conditioners via IR (raw codes)
  - Stores last AC temperature setting in NVS
*/

#include "../config.h"
#include "../ir_codes.h"

#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include <Preferences.h>

#include <DHT.h>
#include <IRremote.hpp>

// ---------- Pins ----------
#define DHTPIN1 18
#define DHTPIN2 22
#define DHTTYPE DHT11

#define IR_SEND_PIN_AIRE1 4
#define IR_SEND_PIN_AIRE2 2

// ---------- Control params ----------
static constexpr float HYST_C = 1.0f;        // hysteresis band
static constexpr int   LOOP_MS = 5000;       // main loop period
static constexpr int   DEFAULT_AC_TEMP = 25; // default AC temp if NVS empty

// ---------- Types ----------
typedef struct {
  float temperatura;
  float humedad;
} SensorData;

// ---------- Globals ----------
DHT dht1(DHTPIN1, DHTTYPE);
DHT dht2(DHTPIN2, DHTTYPE);

Preferences preferences;

SensorData sensorData = {0.0f, 0.0f};

volatile int setpoint = DEFAULT_AC_TEMP;

bool  acOn = false;
int   acTemp = DEFAULT_AC_TEMP;

float lastTempSent = NAN;
float lastHumSent  = NAN;

// ---------- Forward declarations ----------
void initWiFiForEspNow();
void initEspNow();
void addCentralPeer();
void loadAcTemp();
void saveAcTemp();

bool readSensors(SensorData &out);
void sendSensorData(const SensorData &data);

void applyAirControl(const SensorData &data);
void sendAirOn();
void sendAirOff();
void sendAirTempUp();
void sendAirTempDown();

// ESP-NOW callback signature (ESP32 Arduino core 2.x vs 3.x)
#if defined(ESP_ARDUINO_VERSION_MAJOR) && (ESP_ARDUINO_VERSION_MAJOR >= 3)
void onEspNowRecv(const esp_now_recv_info_t* info, const uint8_t* incomingData, int len)
#else
void onEspNowRecv(const uint8_t* mac, const uint8_t* incomingData, int len)
#endif
{
  if (len < (int)sizeof(int)) return;
  int sp;
  memcpy(&sp, incomingData, sizeof(int));
  setpoint = sp;
  Serial.print("Setpoint received: ");
  Serial.println(setpoint);
}

void setup() {
  Serial.begin(115200);

  dht1.begin();
  dht2.begin();

  // IR sender setup (we will re-init per pin when sending)
  // IRremote uses a global IrSender object
  // We'll call IrSender.begin(pin, ...) right before sending each AC command.

  initWiFiForEspNow();
  initEspNow();
  addCentralPeer();

  loadAcTemp();

  Serial.println("Transmitter ready");
}

void loop() {
  SensorData data;
  if (readSensors(data)) {
    sensorData = data;
    sendSensorData(sensorData);
    applyAirControl(sensorData);
  } else {
    Serial.println("Sensor read failed (both DHT invalid).");
  }

  delay(LOOP_MS);
}

// ---------- WiFi / ESP-NOW ----------
void initWiFiForEspNow() {
  // Central uses WiFi for Blynk, so transmitter should be on the same channel.
  // Best option: connect to the same WiFi AP (optional for ESP-NOW only).

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  Serial.print("Connecting WiFi");
  unsigned long t0 = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - t0 < 15000) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();

  if (WiFi.status() == WL_CONNECTED) {
    Serial.print("WiFi OK. Channel: ");
    Serial.println(WiFi.channel());
  } else {
    Serial.println("WiFi timeout. ESP-NOW may fail if channel mismatches Central.");
    // If you know the channel, you could set it here:
    // esp_wifi_set_channel(CHANNEL, WIFI_SECOND_CHAN_NONE);
  }
}

void initEspNow() {
  if (esp_now_init() != ESP_OK) {
    Serial.println("ESP-NOW init failed");
    return;
  }

#if defined(ESP_ARDUINO_VERSION_MAJOR) && (ESP_ARDUINO_VERSION_MAJOR >= 3)
  esp_now_register_recv_cb(onEspNowRecv);
#else
  esp_now_register_recv_cb(onEspNowRecv);
#endif

  Serial.println("ESP-NOW initialized");
}

void addCentralPeer() {
  esp_now_peer_info_t peerInfo = {};
  memcpy(peerInfo.peer_addr, ESPNOW_CENTRAL_MAC, 6);
  peerInfo.channel = 0;     // auto
  peerInfo.encrypt = false;

  if (esp_now_add_peer(&peerInfo) != ESP_OK) {
    Serial.println("ESP-NOW add peer (Central) failed");
  } else {
    Serial.println("ESP-NOW peer (Central) added");
  }
}

// ---------- NVS ----------
void loadAcTemp() {
  preferences.begin("cultismart", true);
  acTemp = preferences.getInt("acTemp", DEFAULT_AC_TEMP);
  preferences.end();

  Serial.print("Loaded AC temp: ");
  Serial.println(acTemp);
}

void saveAcTemp() {
  preferences.begin("cultismart", false);
  preferences.putInt("acTemp", acTemp);
  preferences.end();
}

// ---------- Sensors ----------
bool readSensors(SensorData &out) {
  float t1 = dht1.readTemperature();
  float h1 = dht1.readHumidity();
  float t2 = dht2.readTemperature();
  float h2 = dht2.readHumidity();

  bool v1 = !isnan(t1) && !isnan(h1);
  bool v2 = !isnan(t2) && !isnan(h2);

  if (v1 && v2) {
    out.temperatura = (t1 + t2) / 2.0f;
    out.humedad     = (h1 + h2) / 2.0f;
    return true;
  }
  if (v1) {
    out.temperatura = t1;
    out.humedad     = h1;
    return true;
  }
  if (v2) {
    out.temperatura = t2;
    out.humedad     = h2;
    return true;
  }
  return false;
}

void sendSensorData(const SensorData &data) {
  esp_err_t result = esp_now_send(ESPNOW_CENTRAL_MAC, (uint8_t*)&data, sizeof(SensorData));
  if (result == ESP_OK) {
    Serial.print("Sent: T=");
    Serial.print(data.temperatura, 1);
    Serial.print("C H=");
    Serial.print(data.humedad, 1);
    Serial.println("%");
  } else {
    Serial.println("ESP-NOW send failed");
  }
}

// ---------- Air control ----------
void applyAirControl(const SensorData &data) {
  const float temp = data.temperatura;
  const int sp = setpoint;

  // Hysteresis decision
  bool shouldOn  = (temp > (sp + HYST_C));
  bool shouldOff = (temp < (sp - HYST_C));

  if (shouldOn && !acOn) {
    sendAirOn();
    acOn = true;
  } else if (shouldOff && acOn) {
    sendAirOff();
    acOn = false;
  }

  // Temperature alignment (only if AC is ON)
  if (!acOn) return;

  // Adjust AC set temperature toward setpoint (one step per loop)
  if (sp > acTemp) {
    sendAirTempUp();
    acTemp++;
    saveAcTemp();
  } else if (sp < acTemp) {
    sendAirTempDown();
    acTemp--;
    saveAcTemp();
  }
}

// ---------- IR send helpers ----------
void sendAirOn() {
  Serial.println("IR: AC ON");

  IrSender.begin(IR_SEND_PIN_AIRE1, ENABLE_LED_FEEDBACK);
  if (IR_AIRE1_LEN > 0) IrSender.sendRaw(IR_AIRE1_ON, IR_AIRE1_LEN, IR_KHZ);

  delay(400);

  IrSender.begin(IR_SEND_PIN_AIRE2, ENABLE_LED_FEEDBACK);
  if (IR_AIRE2_LEN > 0) IrSender.sendRaw(IR_AIRE2_ON, IR_AIRE2_LEN, IR_KHZ);
}

void sendAirOff() {
  Serial.println("IR: AC OFF");

  IrSender.begin(IR_SEND_PIN_AIRE1, ENABLE_LED_FEEDBACK);
  if (IR_AIRE1_LEN > 0) IrSender.sendRaw(IR_AIRE1_OFF, IR_AIRE1_LEN, IR_KHZ);

  delay(400);

  IrSender.begin(IR_SEND_PIN_AIRE2, ENABLE_LED_FEEDBACK);
  if (IR_AIRE2_LEN > 0) IrSender.sendRaw(IR_AIRE2_OFF, IR_AIRE2_LEN, IR_KHZ);
}

void sendAirTempUp() {
  Serial.println("IR: AC TEMP UP");

  IrSender.begin(IR_SEND_PIN_AIRE1, ENABLE_LED_FEEDBACK);
  if (IR_AIRE1_LEN > 0) IrSender.sendRaw(IR_AIRE1_UP, IR_AIRE1_LEN, IR_KHZ);

  delay(250);

  IrSender.begin(IR_SEND_PIN_AIRE2, ENABLE_LED_FEEDBACK);
  if (IR_AIRE2_LEN > 0) IrSender.sendRaw(IR_AIRE2_UP, IR_AIRE2_LEN, IR_KHZ);
}

void sendAirTempDown() {
  Serial.println("IR: AC TEMP DOWN");

  IrSender.begin(IR_SEND_PIN_AIRE1, ENABLE_LED_FEEDBACK);
  if (IR_AIRE1_LEN > 0) IrSender.sendRaw(IR_AIRE1_DOWN, IR_AIRE1_LEN, IR_KHZ);

  delay(250);

  IrSender.begin(IR_SEND_PIN_AIRE2, ENABLE_LED_FEEDBACK);
  if (IR_AIRE2_LEN > 0) IrSender.sendRaw(IR_AIRE2_DOWN, IR_AIRE2_LEN, IR_KHZ);
}
