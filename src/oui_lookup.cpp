#include "oui_lookup.h"

struct OUIEntry {
    const char* prefix;
    const char* manufacturer;
};

static const OUIEntry ouiTable[] = {

    { "80:7d:3a", "Flipper Devices"  },
    { "80:e1:26", "Flipper Devices"  },
    { "80:e1:27", "Flipper Devices"  },  
    { "0c:fa:22", "Flipper Devices"  },  

    { "ac:de:48", "Apple"            },
    { "00:cd:fe", "Apple"            },
    { "f0:99:bf", "Apple"            },
    { "28:cf:e9", "Apple"            },
    { "3c:06:30", "Apple"            },
    { "70:56:81", "Apple"            },
    { "a4:83:e7", "Apple"            },
    { "d0:03:4b", "Apple"            },
    { "54:ea:a8", "Apple"            },
    { "78:7e:61", "Apple"            },
  
    { "8c:77:12", "Samsung"          },
    { "40:0e:85", "Samsung"          },
    { "94:35:0a", "Samsung"          },
    { "c4:42:02", "Samsung"          },
    { "cc:46:d6", "Samsung"          },
    { "50:01:bb", "Samsung"          },
    { "84:25:19", "Samsung"          },

    { "f4:f5:d8", "Google"           },
    { "54:60:09", "Google"           },
    { "3c:5a:b4", "Google"           },
    { "a4:77:33", "Google"           },

    { "28:18:78", "Microsoft"        },
    { "00:50:f2", "Microsoft"        },

    { "e4:60:14", "Tile"             },
    { "d0:bb:61", "Tile"             },

    { "c4:4f:33", "Fitbit"           },
    { "00:8b:20", "Fitbit"           },

    { "00:1d:92", "Garmin"           },
    { "58:93:d8", "Garmin"           },

    { "04:52:c7", "Bose"             },
    { "88:df:9e", "Bose"             },

    { "00:24:be", "Sony"             },
    { "28:3f:69", "Sony"             },
    { "04:5d:4b", "Sony"             },

    { "00:26:83", "JBL/Harman"       },
    { "a0:e9:db", "JBL/Harman"       },

    { "00:1a:7d", "Beats"            },
    { "ac:bc:32", "Beats"            },

    { "fc:65:de", "Amazon"           },
    { "44:65:0d", "Amazon"           },
    { "68:37:e9", "Amazon"           },

    { "78:28:ca", "Sonos"            },
    { "b8:e9:37", "Sonos"            },

    { "dc:3a:5e", "Roku"             },
    { "b0:a7:37", "Roku"             },

    { "50:c7:bf", "TP-Link"          },
    { "98:da:c4", "TP-Link"          },

    { "64:cc:2e", "Xiaomi"           },
    { "78:11:dc", "Xiaomi"           },
    { "7c:49:eb", "Xiaomi"           },

    { "00:46:4b", "Huawei"           },
    { "48:46:fb", "Huawei"           },

    { "b0:09:da", "Ring"             },

    { "2c:aa:8e", "Wyze"             },

    { "4c:fc:aa", "Tesla"            },
    { "98:ed:5c", "Tesla"            },

    { "60:60:1f", "DJI"              },

    { "2c:26:17", "Meta/Oculus"      },

    { "18:d6:c7", "Nest/Google Home" },

    { "00:1f:20", "Logitech"         },
    { "b4:7a:f1", "Logitech"         },

    { "00:1b:21", "Intel"            },
    { "a4:c4:94", "Intel"            },

    { "24:0a:c4", "Espressif"        },
    { "24:62:ab", "Espressif"        },
    { "30:ae:a4", "Espressif"        },

    { "b8:27:eb", "Raspberry Pi"     },
    { "dc:a6:32", "Raspberry Pi"     },
    { "e4:5f:01", "Raspberry Pi"     },

    { "a0:9e:1a", "Polar"            },

    { "00:09:a7", "Skullcandy"       },

    { "00:13:17", "Jabra"            },
    { "70:bf:92", "Jabra"            },

    { "00:1b:66", "Sennheiser"       },

    { "00:25:df", "Axon"             },
};
static const int ouiCount = sizeof(ouiTable) / sizeof(ouiTable[0]);

String lookupOUI(const String& address) {
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
        case 0x0059: return "Nordic Semi";
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
        case 0x02E5: return "Espressif";
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
        case 0x01: return "iPhone (Nearby)";
        case 0x02: return "iBeacon";
        case 0x03: return "AirPrint";
        case 0x05: return "AirDrop";
        case 0x06: return "HomeKit";
        case 0x07: return "AirPods";
        case 0x08: return "Siri";
        case 0x09: return "AirPlay";
        case 0x0A: return "iPad (Nearby)";
        case 0x0B: return "Pencil";
        case 0x0C: return "Watch";
        case 0x0D: return "Mac (Nearby)";
        case 0x0E: return "TV";
        case 0x0F: return "HomePod";
        case 0x10: return "AirPods Pro";
        case 0x11: return "AirPods Max";
        case 0x12: return "AirTag / FindMy";
        case 0x13: return "AirPods (3rd Gen)";
        case 0x14: return "AirPods Pro (2nd Gen)";
        default:   return "Apple Device";
    }
}

String decodeAppearance(uint16_t appearance) {
    switch (appearance) {
        case 0x0000: return "";
        case 0x0040: return "Phone";
        case 0x0080: return "Computer";
        case 0x00C0: return "Watch";
        case 0x00C1: return "Sports Watch";
        case 0x0100: return "Clock";
        case 0x0140: return "Display";
        case 0x0180: return "Remote Control";
        case 0x01C0: return "Eye Glasses";
        case 0x0200: return "Tag / Tracker";
        case 0x0240: return "Keyring";
        case 0x0280: return "Media Player";
        case 0x02C0: return "Barcode Scanner";
        case 0x0300: return "Thermometer";
        case 0x0340: return "Heart Rate Monitor";
        case 0x0380: return "Blood Pressure Monitor";
        case 0x03C0: return "HID Device";
        case 0x03C1: return "Keyboard";
        case 0x03C2: return "Mouse";
        case 0x03C3: return "Joystick";
        case 0x03C4: return "Gamepad";
        case 0x0440: return "Glucose Meter";
        case 0x0480: return "Running Sensor";
        case 0x04C0: return "Cycling Sensor";
        case 0x0500: return "Pulse Oximeter";
        case 0x0540: return "Weight Scale";
        case 0x0580: return "Outdoor Sports";
        case 0x0940: return "Earbuds / Headphones";
        case 0x0941: return "In-Ear Headphones";
        case 0x0942: return "Headset";
        case 0x0943: return "Neck Band";
        default: {
            uint16_t category = appearance >> 6;
            switch (category) {
                case 1:  return "Phone";
                case 2:  return "Computer";
                case 3:  return "Watch";
                case 4:  return "Clock";
                case 5:  return "Display";
                case 6:  return "Remote Control";
                case 7:  return "Eye Glasses";
                case 8:  return "Tag";
                case 9:  return "Keyring";
                case 10: return "Media Player";
                case 11: return "Barcode";
                case 12: return "Thermometer";
                case 13: return "Heart Rate";
                case 14: return "Blood Pressure";
                case 15: return "HID Device";
                case 16: return "Glucose Meter";
                case 17: return "Running";
                case 18: return "Cycling";
                case 49: return "Pulse Oximeter";
                case 50: return "Weight Scale";
                default: return "";
            }
        }
    }
}

String decodeServiceUUID(const String& uuid) {
    String u = uuid;
    u.toLowerCase();

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