# CLUNCHI BETA v1.0

CLUNCHI is an ESP32-based quirked up network companion.

Built on an ESP32-C3 supermini with a 1.3" SH1106 OLED, capacitive touch, and a tiny speaker (8ohm .25 Watt)

3D Print files @ https://www.printables.com/model/1702860-clunchi-v1

**Build design coming soon**

- local Wi-Fi setup through a captive portal stored to NVS
- network health monitoring
- deauth/disassoc detection
- a network analyzer web dashboard
- touch-driven mood/audio/animation reactions
- early BLE radar functionality (currently only detects Flipper Zero)

- Alot of the personality and info comes out in the serial monitor

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
- CLUNCHI reboots and uses the saved credentials

If you want to change networks later, clear the saved credentials and use the setup portal again.

---

## Features

### Wi-Fi
- Captive portal setup flow for saving Wi-Fi credentials
- Local network connection monitoring
- RSSI / signal quality reporting
- Nearby network scanning
- IP / DNS / gateway / subnet reporting

### Threat Detection
- Deauth/disassoc frame detection
- Threat scoring
- Recent event log
- Targeted attack indication

### Dashboard
- Local web analyzer dashboard
- Wi-Fi info
- L2 security info
- L3 connectivity stats
- Nearby network list
- Hardware stats
- Local JSON API endpoint

### Personality / Interaction
- Touch-based interaction
- Mood state machine
- Audio feedback
- Animation reactions

### BLE
- Early BLE radar features
- Minimal implementation in this beta
- Planned expansion in future versions

---

## Touch Controls

CLUNCHI responds to tap count and long-press gestures.

### Current tap behavior
- **1 tap** → squish
- **2 taps** → enter menu
- **3 taps** → network check (while connected)/ HAPPY
- **5 taps** → curious reaction / Wi-Fi scan
- **6+ taps** → annoyed reaction
- **10+ taps** → dribble mode
- **Long press** → special mood reaction/ select/ exit in menus

Some touch behavior is context-sensitive depending on current state, menu mode, radar mode, and mood.

---

## BLE Status

BLE functionality in this version is intentionally light.

Right now, BLE support is present as an **early framework / minimal radar feature set** only detects Flipper Zero for now.

Planned BLE roadmap includes:
- expanded scanning behavior
- better alerting and visibility
- more useful device interpretation

---

## Dashboard

Once CLUNCHI is connected to Wi-Fi, open its local IP address in a browser to access the analyzer dashboard.

The dashboard currently includes:
- Wi-Fi details
- signal quality
- deauth/disassoc alert information
- internet / DNS / latency stats
- nearby networks
- hardware stats

- ## Roadmap
- ESP32-C5 board port
- GPS module integration for wardriving
- expanded BLE scanning and alerting
- dashboard improvements
- Logging data to SD card

