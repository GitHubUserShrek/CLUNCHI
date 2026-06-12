# CLUNCHI BETA v1.0

> ⚠️ **IMPORTANT:** Versions moving forward will require soldering the jumper **"A"** on the TTP223 module before assembly.

CLUNCHI is an ESP32-based network companion with a personality.

Built on an ESP32-C3 SuperMini with a 1.3" SH1106 OLED, capacitive touch, a tiny speaker (8Ω, 0.25W), an SD card reader, an ATGM336H GPS module, and a tilt sensor for shake-based interaction.

3D Print files:
- https://www.printables.com/model/1702860-clunchi-v1
- https://makerworld.com/en/models/2774193-clunchi-v1

BUILD GUIDE:
https://www.instructables.com/CLUNCHI-V1/

This project is currently in **beta** with active development on the firmware, dashboard, and future hardware ports.

---

## Wi-Fi Setup

Wi-Fi credentials are **not** hardcoded into the firmware and are **not** meant to be saved manually in source code. CLUNCHI stores credentials through its setup portal only.

On first boot, or when no credentials are saved:
- Open the menu (double tap), go to WiFi, select Setup Portal
- Connect to the CLUNCHI_Setup access point from your phone or computer (pw: `clunchi123`)
- Open the portal page
- Enter your Wi-Fi SSID and password
- Save the configuration
- CLUNCHI reboots and uses the saved credentials (network labeled with `*`)

To change networks later, clear the saved credentials and use the setup portal again.

---

## Features

### Wi-Fi
- Captive portal setup flow for saving Wi-Fi credentials
- Local network connection monitoring
- RSSI / signal quality reporting
- Nearby network scanning
- IP / DNS / gateway / subnet reporting
- 2.4GHz wardriving with GPS and SD logging

### Threat Detection
- Deauth/disassoc frame detection
- Evil Twin Detection
- Beacon Flood detection
- Threat scoring
- Recent event log
- Targeted attack indication
- SD logging of threat events

### BLE
- BLE radar alerts to Flipper Zero presence
- Largeish OUI lookup for BLE device naming
- More Flipper Zero details (color, name)
- GPS and SD logging for BLE radar

### Wardriving
- Scans for networks every 3 seconds
- Dynamically allocated dedupe table to keep heap clean
- Session-based logging — once the dedupe table is full, it creates a new session part
- Logs to a .CSV with GPS coords, time, date, RSSI, channel, and security protocol

### Dashboard
Once CLUNCHI is connected to Wi-Fi, open its local IP address in a browser to access the analyzer dashboard.
- Wi-Fi details
- L2 security info
- L3 connectivity stats (internet / DNS / latency)
- Signal quality
- Threat alert information
- Nearby networks with channel info
- Hardware stats
- Local JSON API endpoint (possible ESPHome port in the future)

### Games
- Dice Roller (D4 through D100)
- Magic 8-Ball
- Blackjack

Games are accessible from the menu, and Dice Roller and Magic 8-Ball support shake-to-play via the tilt sensor.

### Personality / Interaction
- Touch-based interaction (Bouncy! Tap 10 times for Bonk! mode)
- Mood states based on connection quality / under attack
- Tilt/shake interaction via the onboard tilt sensor
- Audio feedback
- Animated responses

---

## Touch Controls

CLUNCHI responds to tap count and long-press gestures.

- **1 tap** → squish
- **2 taps** → enter menu
- **3 taps** → network check (while connected) / HAPPY
- **5 taps** → curious reaction / Wi-Fi scan
- **6+ taps** → annoyed reaction
- **10+ taps** → BONK! mode
- **Long press** → special mood reaction / select / exit in menus

Some touch behavior is context-sensitive depending on current state, menu mode, radar mode, and mood.

---

## Roadmap

- ESP32-C5 board port
- Expanded BLE scanning and alerting
- Dashboard improvements

---

## Credits

Thanks to NyanBox for the BLE OUI data and Flipper Zero detail mappings.

---

## Legal Disclaimer

This device is not a substitute for a factory-certified speedometer and should not be used as primary evidence for speed compliance.
