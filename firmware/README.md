# CultiSmart — Grow Room Controller (ESP32 + ESP-NOW + Blynk)

Embedded monitoring and control system for a grow room:
- Central ESP32: receives sensor data via ESP-NOW, publishes to Blynk, schedules relays, shows OLED status
- Transmitter ESP32: reads sensors, sends data to Central, receives setpoint, controls A/C via IR

## Architecture (high-level)
Transmitter (DHT sensors + IR control)  →  ESP-NOW  →  Central (Blynk + RTC schedules + OLED + relays)

## Firmware
- `firmware/central/central.ino`
- `firmware/transmitter/transmitter.ino`

## Configuration (required)
This repository is public-safe. Secrets are NOT committed.

1) Copy templates:
- `firmware/config.example.h` → `firmware/config.h`
- `firmware/ir_codes.example.h` → `firmware/ir_codes.h`

2) Edit `firmware/config.h` with:
- Blynk Template ID/Name/Auth Token
- WiFi SSID/password
- ESP-NOW MAC addresses for Central and Transmitter

3) Edit `firmware/ir_codes.h` with your real IR raw codes and lengths.

> `firmware/config.h` and `firmware/ir_codes.h` are ignored by git (see `.gitignore`).

## Notes
- ESP-NOW reliability depends on both boards being on the same WiFi channel.
- Arduino IDE compile error “Missing FQBN” means you must select **Board** and **Port**.

## Media
See `media/` for photos and demo.
