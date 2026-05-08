# CLUNCHI BETA v1.0

CLUNCHI is an ESP32-based network companion.

Built on an ESP32-C3 supermini with a 1.3" SH1106 OLED, capacitive touch, a tiny speaker (8ohm .25 Watt) and an SD reader as well as an ATGM336H GPS module.

3D Print files @ https://www.printables.com/model/1702860-clunchi-v1
                 https://makerworld.com/en/models/2774193-clunchi-v1

**Build design coming soon**

- local Wi-Fi setup through a captive portal
- network health monitoring
- deauth/disassoc detection with SD logging
- network analyzer web dashboard
- touch-driven mood/audio/animation reactions
- early BLE radar functionality (currently only detects Flipper Zero)
- Added OUI lookup for more devices (Thank you NyanBox for the OUIs)
- GPS and SD logging for Wardriving and BLE radar
- TimeZone/ DST settings for clock
- Serial Monitor expressions and logging

This project is currently in **beta** and active development is happening on the firmware, dashboard, and future hardware ports.

---

## Important Wi-Fi Setup Note

Wi-Fi credentials are **not** hardcoded into the firmware and are **not** meant to be saved manually in source code.

CLUNCHI stores Wi-Fi credentials through its **setup portal only**.

### Wi-Fi setup 
- On first boot, or when no credentials are saved, go to menu (double tap), go to wifi, select setup portal
- Connect to the CLUNCHI access point from your phone or computer (pw: clunchi123)
- Open the portal page
- Enter your Wi-Fi SSID and password
- Save the configuration
- CLUNCHI reboots and uses the saved credentials (network labeled *)

If you want to change networks later, clear the saved credentials and use the setup portal again.

---

## Features

### Wi-Fi
- Captive portal setup flow for saving Wi-Fi credentials
- Local network connection monitoring
- RSSI / signal quality reporting
- Nearby network scanning
- IP / DNS / gateway / subnet reporting
- 2.4Ghz Wardrving with GPS and SD logging

### Threat Detection
- Deauth/disassoc frame detection
- Threat scoring
- Recent event log
- Targeted attack indication
- BLE radar detects and logs Flipper Zero presence 

### Dashboard
- Local web analyzer dashboard
- Wi-Fi info
- L2 security info
- L3 connectivity stats
- Nearby network list with Channel info
- Hardware stats
- Local JSON API endpoint/ possible ESPHome port in the future

### Personality / Interaction
- Touch-based interaction (Bouncy! Tap 10 times for Bonk! mode)
- Mood states based on connection quality/ under attack
- Audio feedback
- Animated responses

### BLE
- BLE radar alerts to Flipper Zero presence 
- Largeish OUI lookup for BLE device naming
- More Flipper Zero details (Color, Name) (Also thanks again NyanBox)


### Wardriving
- Scans for networks every 3 seconds
- Dynamically allocated dedupe table to keep heap clean
- Session based logging, once dedupe table is full, will create a new session part.
- Logs in a .CSV with GPS coords, time, date, rssi, channel, and security protocol

---

## Touch Controls

CLUNCHI responds to tap count and long-press gestures.

### Current tap behavior
- **1 tap** → squish
- **2 taps** → enter menu
- **3 taps** → network check (while connected)/ HAPPY
- **5 taps** → curious reaction / Wi-Fi scan
- **6+ taps** → annoyed reaction
- **10+ taps** → BONK! mode
- **Long press** → special mood reaction/ select/ exit in menus

Some touch behavior is context-sensitive depending on current state, menu mode, radar mode, and mood.


## Dashboard

Once CLUNCHI is connected to Wi-Fi, open its local IP address in a browser to access the analyzer dashboard.

The dashboard currently includes:
- Wi-Fi details
- signal quality
- deauth/disassoc alert information
- internet / DNS / latency stats
- nearby networks with channel info`
- hardware stats

- ## Roadmap
- ESP32-C5 board port
- expanded BLE scanning and alerting
- dashboard improvements

- Legal Disclaimer: This device is not a substitute for a factory-certified speedometer and should not be used as primary evidence for speed compliance.


