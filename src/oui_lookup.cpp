#include "oui_lookup.h"

struct OUIEntry {
    const char* prefix;
    const char* manufacturer;
};


static const OUIEntry ouiTable[] = {

    { "80:7d:3a", "Flipper Devices" }, { "80:e1:26", "Flipper Devices" },
    { "80:e1:27", "Flipper Devices" }, { "0c:fa:22", "Flipper Devices" },  

    { "b8:27:eb", "Raspberry Pi" }, { "dc:a6:32", "Raspberry Pi" },
    { "e4:5f:01", "Raspberry Pi" }, { "80:48:2c", "Raspberry Pi" },
    { "28:cd:c1", "Raspberry Pi"     }, { "d8:3a:dd", "Raspberry Pi"      },

    { "60:c5:a8", "RAKwireless"               }, { "ac:1f:0f", "RAKwireless"               },
    { "00:05:0c", "Seeed Studio (T-Echo)"     }, { "98:84:e3", "Seeed Studio (T-Echo)"     },
    { "bc:83:85", "Seeed Studio (T-Echo)"     }, { "00:01:a8", "Heltec Automation"         },
    { "48:e7:29", "Heltec Automation"         }, { "30:c6:f7", "LilyGO / TTGO"             }, 
    { "70:b8:f6", "LilyGO / TTGO"             },

    { "00:1e:59", "Nordic Semiconductor" }, { "00:f0:ca", "Nordic Semiconductor" },
    { "40:a5:ef", "Nordic Semiconductor" }, { "5c:e5:0c", "Nordic Semiconductor" },
    { "68:0a:28", "Nordic Semiconductor" }, { "84:2e:14", "Nordic Semiconductor" },
    { "c0:04:15", "Nordic Semiconductor" }, { "d4:ca:6e", "Nordic Semiconductor" },
    { "e0:dc:ff", "Nordic Semiconductor" }, { "f4:ce:36", "Nordic Semiconductor" },
    { "f8:e4:e3", "Nordic Semiconductor" },

    { "00:08:22", "Espressif Systems" }, { "04:25:c5", "Espressif Systems" }, { "08:3a:8d", "Espressif Systems" }, 
    { "08:b6:1f", "Espressif Systems" }, { "0c:8b:95", "Espressif Systems" }, { "10:52:1c", "Espressif Systems" }, 
    { "10:91:a8", "Espressif Systems" }, { "14:2b:2f", "Espressif Systems" }, { "18:8b:0e", "Espressif Systems" }, 
    { "18:c8:e7", "Espressif Systems" }, { "1c:9d:c2", "Espressif Systems" }, { "24:0a:c4", "Espressif Systems" }, 
    { "24:62:ab", "Espressif Systems" }, { "24:6f:28", "Espressif Systems" }, { "24:a1:60", "Espressif Systems" }, 
    { "24:d7:eb", "Espressif Systems" }, { "2c:bc:bb", "Espressif Systems" }, { "30:30:f9", "Espressif Systems" }, 
    { "30:ae:a4", "Espressif Systems" }, { "30:ae:c1", "Espressif Systems" }, { "34:85:18", "Espressif Systems" }, 
    { "34:86:5d", "Espressif Systems" }, { "34:94:54", "Espressif Systems" }, { "34:ab:95", "Espressif Systems" }, 
    { "34:b4:72", "Espressif Systems" }, { "3c:61:05", "Espressif Systems" }, { "3c:71:bf", "Espressif Systems" }, 
    { "40:22:d8", "Espressif Systems" }, { "40:4c:ca", "Espressif Systems" }, { "40:91:51", "Espressif Systems" }, 
    { "48:27:e2", "Espressif Systems" }, { "48:3f:da", "Espressif Systems" }, { "48:55:19", "Espressif Systems" }, 
    { "4c:11:ae", "Espressif Systems" }, { "4c:75:25", "Espressif Systems" }, { "50:02:91", "Espressif Systems" }, 
    { "54:32:04", "Espressif Systems" }, { "54:43:b2", "Espressif Systems" }, { "58:bf:25", "Espressif Systems" }, 
    { "5c:cf:7f", "Espressif Systems" }, { "60:55:f9", "Espressif Systems" }, { "64:b7:08", "Espressif Systems" }, 
    { "64:e8:33", "Espressif Systems" }, { "68:b6:b3", "Espressif Systems" }, { "68:c6:3a", "Espressif Systems" }, 
    { "70:03:9f", "Espressif Systems" }, { "70:04:1d", "Espressif Systems" }, { "74:4d:bd", "Espressif Systems" }, 
    { "78:21:84", "Espressif Systems" }, { "78:e3:6d", "Espressif Systems" }, { "7c:87:ce", "Espressif Systems" }, 
    { "7c:9e:bd", "Espressif Systems" }, { "7c:df:a1", "Espressif Systems" }, { "80:65:99", "Espressif Systems" }, 
    { "84:0d:8e", "Espressif Systems" }, { "84:cc:a8", "Espressif Systems" }, { "84:f3:eb", "Espressif Systems" }, 
    { "8c:4b:14", "Espressif Systems" }, { "8c:aa:b5", "Espressif Systems" }, { "90:38:0c", "Espressif Systems" }, 
    { "90:97:d5", "Espressif Systems" }, { "94:3c:c6", "Espressif Systems" }, { "94:b9:7e", "Espressif Systems" }, 
    { "94:e6:86", "Espressif Systems" }, { "98:cd:ac", "Espressif Systems" }, { "9c:9c:1f", "Espressif Systems" }, 
    { "a0:20:a6", "Espressif Systems" }, { "a0:76:4e", "Espressif Systems" }, { "a0:b7:65", "Espressif Systems" }, 
    { "a4:cf:12", "Espressif Systems" }, { "a8:03:2a", "Espressif Systems" }, { "a8:42:e3", "Espressif Systems" }, 
    { "ac:0b:fb", "Espressif Systems" }, { "ac:67:b2", "Espressif Systems" }, { "ac:d0:74", "Espressif Systems" }, 
    { "b0:a7:32", "Espressif Systems" }, { "b0:b2:1c", "Espressif Systems" }, { "b4:8a:0a", "Espressif Systems" }, 
    { "b4:e6:2d", "Espressif Systems" }, { "b8:d6:1a", "Espressif Systems" }, { "bc:dd:95", "Espressif Systems" }, 
    { "c0:49:ef", "Espressif Systems" }, { "c4:4f:33", "Espressif Systems" }, { "c4:dd:57", "Espressif Systems" }, 
    { "c8:2b:96", "Espressif Systems" }, { "c8:c9:a3", "Espressif Systems" }, { "c8:f0:9e", "Espressif Systems" }, 
    { "cc:50:e3", "Espressif Systems" }, { "d4:8a:fc", "Espressif Systems" }, { "d4:d4:da", "Espressif Systems" }, 
    { "d8:a0:1d", "Espressif Systems" }, { "d8:bf:c0", "Espressif Systems" }, { "dc:4f:22", "Espressif Systems" }, 
    { "e0:5a:1b", "Espressif Systems" }, { "e0:98:06", "Espressif Systems" }, { "e0:e2:e6", "Espressif Systems" }, 
    { "e4:65:b8", "Espressif Systems" }, { "e8:68:e7", "Espressif Systems" }, { "e8:9f:6d", "Espressif Systems" }, 
    { "ec:62:60", "Espressif Systems" }, { "ec:94:cb", "Espressif Systems" }, { "ec:da:3b", "Espressif Systems" }, 
    { "f0:08:d1", "Espressif Systems" }, { "f4:12:fa", "Espressif Systems" }, { "f4:cf:a2", "Espressif Systems" }, 
    { "f8:b3:b7", "Espressif Systems" }, { "fc:e8:c0", "Espressif Systems" },

    { "ac:de:48", "Apple" }, { "00:cd:fe", "Apple" }, { "f0:99:bf", "Apple" }, { "28:cf:e9", "Apple" },
    { "3c:06:30", "Apple" }, { "70:56:81", "Apple" }, { "a4:83:e7", "Apple" }, { "d0:03:4b", "Apple" },
    { "54:ea:a8", "Apple" }, { "78:7e:61", "Apple" },
    { "8c:77:12", "Samsung" }, { "40:0e:85", "Samsung" }, { "94:35:0a", "Samsung" }, { "c4:42:02", "Samsung" },
    { "cc:46:d6", "Samsung" }, { "50:01:bb", "Samsung" }, { "84:25:19", "Samsung" },
    { "f4:f5:d8", "Google" }, { "54:60:09", "Google" }, { "3c:5a:b4", "Google" }, { "a4:77:33", "Google" },
    { "28:18:78", "Microsoft" }, { "00:50:f2", "Microsoft" },
    { "e4:60:14", "Tile" }, { "d0:bb:61", "Tile" }, { "c4:4f:33", "Fitbit" }, { "00:8b:20", "Fitbit" },
    { "00:1d:92", "Garmin" }, { "58:93:d8", "Garmin" }, { "04:52:c7", "Bose" }, { "88:df:9e", "Bose" },
    { "00:24:be", "Sony" }, { "28:3f:69", "Sony" }, { "04:5d:4b", "Sony" }, { "00:26:83", "JBL/Harman" },
    { "a0:e9:db", "JBL/Harman" }, { "00:1a:7d", "Beats" }, { "ac:bc:32", "Beats" }, { "fc:65:de", "Amazon" },
    { "44:65:0d", "Amazon" }, { "68:37:e9", "Amazon" }, { "78:28:ca", "Sonos" }, { "b8:e9:37", "Sonos" },
    { "dc:3a:5e", "Roku" }, { "b0:a7:37", "Roku" }, { "50:c7:bf", "TP-Link" }, { "98:da:c4", "TP-Link" },
    { "64:cc:2e", "Xiaomi" }, { "78:11:dc", "Xiaomi" }, { "7c:49:eb", "Xiaomi" }, { "00:46:4b", "Huawei" },
    { "48:46:fb", "Huawei" }, { "b0:09:da", "Ring" }, { "2c:aa:8e", "Wyze" }, { "4c:fc:aa", "Tesla" },
    { "98:ed:5c", "Tesla" }, { "60:60:1f", "DJI" }, { "2c:26:17", "Meta/Oculus" }, { "18:d6:c7", "Nest/Google Home" },
    { "00:1f:20", "Logitech" }, { "b4:7a:f1", "Logitech" }, { "00:1b:21", "Intel" }, { "a4:c4:94", "Intel" },
    { "a0:9e:1a", "Polar" }, { "00:09:a7", "Skullcandy" }, { "00:13:17", "Jabra" }, { "70:bf:92", "Jabra" }, 
    { "00:1b:66", "Sennheiser" }, { "00:25:df", "Axon" }
};
static const int ouiCount = sizeof(ouiTable) / sizeof(ouiTable[0]);

String lookupOUI(const String& address) {
    if (address.length() < 8) return "";

    if (address.equalsIgnoreCase("ff:ff:ff:ff:ff:ff")) return "Broadcast";

    uint8_t firstOctet = (uint8_t)strtol(address.substring(0, 2).c_str(), NULL, 16);
    if ((firstOctet & 0x02) != 0) {
        return "Random/Spoofed";
    }

    String prefix = address.substring(0, 8);
    prefix.toLowerCase();
    for (int i = 0; i < ouiCount; i++) {
        if (prefix == ouiTable[i].prefix) return ouiTable[i].manufacturer;
    }
    return "";
}

String decodeManufacturerID(uint16_t mfrId) {
    switch (mfrId) {
        case 0x004C: return "Apple";
        case 0x00E0: return "Google";
        case 0x0006: return "Microsoft";
        case 0x0075: return "Samsung";
        case 0x0087: return "Garmin";
        case 0x01D6: return "Tile";
        case 0x067C: return "Tile";
        case 0x0392: return "Fitbit";
        case 0x004F: return "Bose";
        case 0x012D: return "Sony";
        case 0x0154: return "JBL/Harman";
        case 0x0057: return "Beats";
        case 0x0499: return "Ruuvi";
        case 0x0059: return "Nordic Semiconductor";
        case 0x000A: return "Qualcomm";
        case 0x0002: return "Intel";
        case 0x000D: return "Texas Instruments";
        case 0x000F: return "Broadcom";
        case 0x001D: return "Qualcomm";
        case 0x0030: return "ST Micro";
        case 0x00E3: return "Amazon";
        case 0x0171: return "Amazon Lab126";
        case 0x0310: return "Xiaomi";
        case 0x038F: return "Huawei";
        case 0x02E5: return "Espressif Systems";
        case 0x0131: return "Tesla";
        case 0x022B: return "Tesla";
        case 0x0822: return "Wyze";
        case 0x0046: return "Sonos";
        case 0x0188: return "Ring";
        case 0x00FF: return "Logitech";
        case 0x006B: return "Polar";
        case 0x08AA: return "DJI";
        case 0xFC81: return "Axon";
        case 0x01AB: return "Meta";
        case 0x058E: return "Meta XR";
        default:     return "";
    }
}

String decodeAppleType(uint8_t type) {
    switch (type) {
        case 0x01: return "iPhone (Nearby)"; case 0x02: return "iBeacon"; case 0x03: return "AirPrint";
        case 0x05: return "AirDrop"; case 0x06: return "HomeKit"; case 0x07: return "AirPods";
        case 0x08: return "Siri"; case 0x09: return "AirPlay"; case 0x0A: return "iPad (Nearby)";
        case 0x0B: return "Pencil"; case 0x0C: return "Watch"; case 0x0D: return "Mac (Nearby)";
        case 0x0E: return "TV"; case 0x0F: return "HomePod"; case 0x10: return "AirPods Pro";
        case 0x11: return "AirPods Max"; case 0x12: return "AirTag / FindMy"; case 0x13: return "AirPods (3rd Gen)";
        case 0x14: return "AirPods Pro (2nd Gen)"; default: return "Apple Device";
    }
}

String decodeAppearance(uint16_t appearance) {
    switch (appearance) {
        case 0x0000: return ""; case 0x0040: return "Phone"; case 0x0080: return "Computer";
        case 0x00C0: return "Watch"; case 0x00C1: return "Sports Watch"; case 0x0100: return "Clock";
        case 0x0140: return "Display"; case 0x0180: return "Remote Control"; case 0x01C: return "Eye Glasses";
        case 0x0200: return "Tag / Tracker"; case 0x0240: return "Keyring"; case 0x0280: return "Media Player";
        case 0x02C0: return "Barcode Scanner"; case 0x0300: return "Thermometer"; case 0x0340: return "Heart Rate Monitor";
        case 0x0380: return "Blood Pressure Monitor"; case 0x03C0: return "HID Device"; case 0x03C1: return "Keyboard";
        case 0x03C2: return "Mouse"; case 0x03C3: return "Joystick"; case 0x03C4: return "Gamepad";
        case 0x0440: return "Glucose Meter"; case 0x0480: return "Running Sensor"; case 0x04C0: return "Cycling Sensor";
        case 0x0500: return "Pulse Oximeter"; case 0x0540: return "Weight Scale"; case 0x0580: return "Outdoor Sports";
        case 0x0940: return "Earbuds / Headphones"; case 0x0941: return "In-Ear Headphones"; case 0x0942: return "Headset";
        case 0x0943: return "Neck Band";
        default: {
            uint16_t category = appearance >> 6;
            switch (category) {
                case 1:  return "Phone"; case 2:  return "Computer"; case 3:  return "Watch";
                case 4:  return "Clock"; case 5:  return "Display"; case 6:  return "Remote Control";
                case 7:  return "Eye Glasses"; case 8:  return "Tag"; case 9:  return "Keyring";
                case 10: return "Media Player"; case 11: return "Barcode"; case 12: return "Thermometer";
                case 13: return "Heart Rate"; case 14: return "Blood Pressure"; case 15: return "HID Device";
                case 16: return "Glucose Meter"; case 17: return "Running"; case 18: return "Cycling";
                case 49: return "Pulse Oximeter"; case 50: return "Weight Scale"; default: return "";
            }
        }
    }
}

String decodeServiceUUID(const String& uuid) {
    String u = uuid;
    u.toLowerCase();

    if (u == "1ba9f000-b248-433b-8c1c-3705edd8b74b") return "Meshtastic Node (Primary)";
    if (u == "cb0b9a0b-a84c-4c0d-bdbb-442e3144ee30") return "Meshtastic Node (Legacy)";
    if (u == "6e400001-b5a3-f393-e0a9-e50e24dcca9e") return "Nordic UART (RAK/Meshtastic)";


    if (u == "0000110b-0000-1000-8000-00805f9b34fb") return "Audio Sink (Speaker)";
    if (u == "0000110a-0000-1000-8000-00805f9b34fb") return "Audio Source";
    if (u == "0000111e-0000-1000-8000-00805f9b34fb") return "Handsfree";
    if (u == "00001108-0000-1000-8000-00805f9b34fb") return "Headset";
    if (u == "00001800-0000-1000-8000-00805f9b34fb") return "Generic Access";
    if (u == "00001801-0000-1000-8000-00805f9b34fb") return "Generic Attribute";
    if (u == "00001802-0000-1000-8000-00805f9b34fb") return "Immediate Alert";
    if (u == "00001803-0000-1000-8000-00805f9b34fb") return "Link Loss (Tag)";
    if (u == "00001804-0000-1000-8000-00805f9b34fb") return "Tx Power";
    if (u == "00001805-0000-1000-8000-00805f9b34fb") return "Current Time";
    if (u == "00001809-0000-1000-8000-00805f9b34fb") return "Health Thermometer";
    if (u == "0000180a-0000-1000-8000-00805f9b34fb") return "Device Info";
    if (u == "0000180d-0000-1000-8000-00805f9b34fb") return "Heart Rate";
    if (u == "0000180f-0000-1000-8000-00805f9b34fb") return "Battery Service";
    if (u == "00001810-0000-1000-8000-00805f9b34fb") return "Blood Pressure";
    if (u == "00001811-0000-1000-8000-00805f9b34fb") return "Alert Notification";
    if (u == "00001812-0000-1000-8000-00805f9b34fb") return "HID (Keyboard/Mouse)";
    if (u == "00001814-0000-1000-8000-00805f9b34fb") return "Running Speed";
    if (u == "00001816-0000-1000-8000-00805f9b34fb") return "Cycling Speed";
    if (u == "00001818-0000-1000-8000-00805f9b34fb") return "Cycling Power";
    if (u == "00001819-0000-1000-8000-00805f9b34fb") return "Location/Navigation";
    if (u == "0000181c-0000-1000-8000-00805f9b34fb") return "User Data";
    if (u == "00001826-0000-1000-8000-00805f9b34fb") return "Fitness Machine";
    if (u == "0000183b-0000-1000-8000-00805f9b34fb") return "Binary Sensor";

    if (u == "0000fe9f-0000-1000-8000-00805f9b34fb") return "Google Fast Pair";
    if (u == "0000febe-0000-1000-8000-00805f9b34fb") return "Tile Tracker";
    if (u == "0000fecb-0000-1000-8000-00805f9b34fb") return "Tile Tracker";
    if (u == "0000feec-0000-1000-8000-00805f9b34fb") return "Tile Tracker";
    if (u == "0000feed-0000-1000-8000-00805f9b34fb") return "Tile Tracker";
    if (u == "0000fe07-0000-1000-8000-00805f9b34fb") return "Eddystone Beacon";
    if (u == "0000feaa-0000-1000-8000-00805f9b34fb") return "Eddystone";
    if (u == "0000fd6f-0000-1000-8000-00805f9b34fb") return "COVID Exposure";
    if (u == "0000fe2c-0000-1000-8000-00805f9b34fb") return "Tile (alt)";
    if (u == "0000fd5a-0000-1000-8000-00805f9b34fb") return "Samsung SmartThings";
    if (u == "0000fe26-0000-1000-8000-00805f9b34fb") return "Google Nearby";
    if (u == "0000fd5f-0000-1000-8000-00805f9b34fb") return "RayBan/Meta";
    if (u == "0000fe95-0000-1000-8000-00805f9b34fb") return "Xiaomi";
    if (u == "0000fffa-0000-1000-8000-00805f9b34fb") return "RemoteID (Drone)";

    if (u.indexOf("3081") >= 0) return "Flipper Zero (Black)";
    if (u.indexOf("3082") >= 0) return "Flipper Zero (White)";
    if (u.indexOf("3083") >= 0) return "Flipper Zero (Trans)";

    return "";
}