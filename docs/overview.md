# CultiSmart — Overview (Portfolio)

## Goal
CultiSmart is an embedded monitoring & control module for a grow-room environment.  
It measures **temperature/humidity**, controls **relays** on schedules, and can control **air conditioners via IR** based on a setpoint.

## Architecture
Two ESP32 nodes:

### 1) Central ESP32 (Control + Dashboard)
- Receives sensor data via **ESP-NOW**
- Publishes data to **Blynk** (temperature/humidity)
- Receives **setpoint** and **schedules** from Blynk (RTC)
- Drives **Relay 1** and **Relay 2** (time schedules)
- Shows system status on **OLED SSD1306**

### 2) Transmitter ESP32 (Sensors + IR Control)
- Reads **two DHT11** sensors (uses average when both are valid)
- Sends SensorData to Central via **ESP-NOW**
- Receives setpoint from Central via **ESP-NOW**
- Controls **two air conditioners** using **IR raw codes**
- Stores last AC temperature setting in **NVS (Preferences)**

## Data flow (high-level)
1. Transmitter reads sensors → sends `{temperature, humidity}` to Central via ESP-NOW  
2. Central updates OLED + pushes values to Blynk  
3. User sets setpoint/schedules in Blynk → Central applies relay schedules and sends setpoint to Transmitter  
4. Transmitter applies hysteresis and sends IR commands to AC units

## Key features
- Dual-sensor acquisition with validation (fallback to single sensor)
- ESP-NOW low-latency local link between nodes
- Blynk dashboard + RTC schedules for automation
- Relay scheduling logic supports on/off windows (including overnight windows)
- IR control of AC units with hysteresis and stored last temperature

## Repository notes (public-safe)
This repository is sanitized for portfolio use:
- Secrets are not committed (WiFi/Blynk tokens)
- IR codes are provided as templates (`ir_codes.example.h`)
- To run locally, copy:
  - `firmware/config.example.h` → `firmware/config.h`
  - `firmware/ir_codes.example.h` → `firmware/ir_codes.h`

## Media
See `media/` for photos and demo video.
