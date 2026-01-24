/*
  CultiSmart - Central node (ESP32)

  - Receives SensorData via ESP-NOW
  - Publishes to Blynk (V1 temperature, V2 humidity)
  - Receives setpoint from Blynk (V0) and schedules (V6-V9)
  - Drives relays on GPIO2 and GPIO15 (active LOW)
  - Updates OLED SSD1306 (I2C 0x3C)
*/

#include "../config.h"

#include <WiFi.h>
#include <esp_now.h>

#include <BlynkSimpleEsp32.h>
#include <TimeLib.h>
#include <WidgetRTC.h>

#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// ---------- Pins ----------
#define RELAY1_PIN 2
#define RELAY2_PIN 15

// ---------- OLED ----------
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 32
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

// ---------- Blynk ----------
WidgetRTC rtc;
BlynkTimer timer;

// ---------- Data ----------
typedef struct {
  float temperatura;
  float humedad;
} SensorData;

volatile SensorData receivedData = {0.0f, 0.0f};
volatile bool hasNewSensorData = false;

int setpoint = 25;

// Schedule times (from Blynk RTC Time Input)
int horaEncenderRelay1 = 0, minutoEncenderRelay1 = 0, horaApagarRelay1 = 0, minutoApagarRelay1 = 0;
int horaEncenderRelay2 = 0, minutoEncenderRelay2 = 0, horaApagarRelay2 = 0, minutoApagarRelay2 = 0;

// ---------- Forward declarations ----------
void initWiFi();
void initESPNow();
void initOLED();
void updateOLED();
void applyRelaySchedule();
void sendSetpointToTransmitter();
bool isActiveWindow(int hOn, int mOn, int hOff, int mOff, int hNow, int mNow);

// ESP-NOW callback (compatible with ESP32 Arduino core 2.x and 3.x)
#if defined(ESP_ARDUINO_VERSION_MAJOR) && (ESP_ARDUINO_VERSION_MAJOR >= 3)
void onEspNowRecv(const esp_now_recv_info_t* info, const uint8_t* incomingData, int len)
#else
void onEspNowRecv(const uint8_t* mac, const uint8_t* incomingData, int len)
#endif
{
  if (len < (int)sizeof(SensorData)) return;

  SensorData tmp;
  memcpy(&tmp, incomingData, sizeof(SensorData));

  receivedData = tmp;
  hasNewSensorData = true;

  // Push to Blynk
  Blynk.virtualWrite(V1, tmp.temperatura);
  Blynk.virtualWrite(V2, tmp.humedad);
}

void setup() {
  Serial.begin(115200);

  pinMode(RELAY1_PIN, OUTPUT);
  pinMode(RELAY2_PIN, OUTPUT);
  digitalWrite(RELAY1_PIN, HIGH); // active LOW
  digitalWrite(RELAY2_PIN, HIGH);

  initWiFi();
  initESPNow();
  initOLED();

  // Blynk
  Blynk.config(BLYNK_AUTH_TOKEN, "blynk.cloud", 80);
  Blynk.connect(8000);

  rtc.begin();

  // Timers
  timer.setInterval(1000L, updateOLED);          // OLED refresh
  timer.setInterval(60000L, applyRelaySchedule); // schedule check each minute
}

void loop() {
  if (!Blynk.connected()) {
    Blynk.connect(2000);
  }
  Blynk.run();
  timer.run();
}

// ---------- WiFi ----------
void initWiFi() {
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
    Serial.print("WiFi OK. IP: ");
    Serial.println(WiFi.localIP());
    Serial.print("WiFi channel: ");
    Serial.println(WiFi.channel());
  } else {
    Serial.println("WiFi timeout. Running without cloud connectivity.");
  }
}

// ---------- ESP-NOW ----------
void initESPNow() {
  if (esp_now_init() != ESP_OK) {
    Serial.println("ESP-NOW init failed");
    return;
  }

#if defined(ESP_ARDUINO_VERSION_MAJOR) && (ESP_ARDUINO_VERSION_MAJOR >= 3)
  esp_now_register_recv_cb(onEspNowRecv);
#else
  esp_now_register_recv_cb(onEspNowRecv);
#endif

  esp_now_peer_info_t peerInfo = {};
  memcpy(peerInfo.peer_addr, ESPNOW_TX_MAC, 6);
  peerInfo.channel = 0;     // use current WiFi channel
  peerInfo.encrypt = false;

  if (esp_now_add_peer(&peerInfo) != ESP_OK) {
    Serial.println("ESP-NOW add peer failed");
  } else {
    Serial.println("ESP-NOW peer added");
  }
}

// ---------- OLED ----------
void initOLED() {
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println("OLED init failed");
    return;
  }
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.print("CultiSmart Central");
  display.display();
  delay(800);
}

void updateOLED() {
  SensorData tmp = receivedData;

  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);

  display.setCursor(0, 0);
  display.printf("T: %.1fC  H: %.1f%%", tmp.temperatura, tmp.humedad);

  display.setCursor(0, 12);
  display.printf("SP: %dC", setpoint);

  int h = hour();
  int m = minute();
  display.setCursor(0, 22);
  display.printf("%02d:%02d  R1 %02d:%02d-%02d:%02d",
                 h, m,
                 horaEncenderRelay1, minutoEncenderRelay1,
                 horaApagarRelay1, minutoApagarRelay1);

  display.display();
}

// ---------- Blynk handlers ----------
BLYNK_CONNECTED() {
  rtc.begin();
  Blynk.syncVirtual(V6, V7, V8, V9);
  Blynk.syncVirtual(V0);
}

// Setpoint from Blynk
BLYNK_WRITE(V0) {
  setpoint = param.asInt();
  sendSetpointToTransmitter();
}

// Schedules from Blynk Time Input
BLYNK_WRITE(V6) { int s = param[0].asInt(); horaEncenderRelay1 = s / 3600; minutoEncenderRelay1 = (s % 3600) / 60; }
BLYNK_WRITE(V7) { int s = param[0].asInt(); horaApagarRelay1  = s / 3600; minutoApagarRelay1  = (s % 3600) / 60; }
BLYNK_WRITE(V8) { int s = param[0].asInt(); horaEncenderRelay2 = s / 3600; minutoEncenderRelay2 = (s % 3600) / 60; }
BLYNK_WRITE(V9) { int s = param[0].asInt(); horaApagarRelay2  = s / 3600; minutoApagarRelay2  = (s % 3600) / 60; }

// ---------- Relays ----------
void applyRelaySchedule() {
  int h = hour();
  int m = minute();

  bool r1 = isActiveWindow(horaEncenderRelay1, minutoEncenderRelay1, horaApagarRelay1, minutoApagarRelay1, h, m);
  bool r2 = isActiveWindow(horaEncenderRelay2, minutoEncenderRelay2, horaApagarRelay2, minutoApagarRelay2, h, m);

  digitalWrite(RELAY1_PIN, r1 ? LOW : HIGH);
  digitalWrite(RELAY2_PIN, r2 ? LOW : HIGH);
}

bool isActiveWindow(int hOn, int mOn, int hOff, int mOff, int hNow, int mNow) {
  int on  = hOn * 60 + mOn;
  int off = hOff * 60 + mOff;
  int now = hNow * 60 + mNow;

  if (on == off) return false;

  if (on < off) {
    return (now >= on && now < off);
  } else {
    // wraps midnight
    return (now >= on || now < off);
  }
}

// ---------- Setpoint send ----------
void sendSetpointToTransmitter() {
  esp_err_t result = esp_now_send(ESPNOW_TX_MAC, (uint8_t*)&setpoint, sizeof(setpoint));
  (void)result;
}
