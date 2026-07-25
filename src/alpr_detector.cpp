#include "alpr_detector.h"

#if defined(BOARD_XIAO_C5)

#include "config.h"
#include "sd_manager.h"
#include "gps_manager.h"
#include "wifi_manager.h"
#include "oui_lookup.h"
#include <WiFi.h>
#include <SD.h>
#include <ctype.h>
#include "esp_wifi.h"

#define ALPR_SEEN_EMPTY 0
#define ALPR_SEEN_USED 1

#define ALPR_BURST_MIN_COUNT 3
#define ALPR_BURST_WINDOW_MS 800
#define ALPR_BURST_SPACING_MS 200
#define ALPR_SUSTAINED_RATE 5
#define ALPR_SUSTAINED_WINDOW_MS 3000
#define ALPR_RSSI_STABLE_MAX_VAR 6
#define ALPR_RSSI_HISTORY_SIZE 8
#define ALPR_CHANNEL_HOP_MIN 2

#define POINTS_OUI_MATCH 30
#define POINTS_WILDCARD_PROBE 10
#define POINTS_LITEON_IE 20
#define POINTS_PACK_SIGNATURE 25
#define POINTS_BURST_PATTERN 15
#define POINTS_STATIONARY_RSSI 15
#define POINTS_SUSTAINED_PROBE 15
#define POINTS_CHANNEL_HOPPING 15
#define POINTS_SSID_MATCH 35
#define POINTS_BEACON_MATCH 25
#define POINTS_PROBE_RESP 15
#define POINTS_BLE_XUNTONG 45
#define POINTS_BLE_PENGUIN 40
#define POINTS_BLE_FS_BATTERY 40
#define POINTS_BLE_10DIGIT 15
#define POINTS_BLE_NAME_MATCH 30

struct VendorOUI
{
    uint8_t oui[3];
    ALPRVendor vendor;
    ALPRDeviceType defaultType;
};

static const VendorOUI vendorDatabase[] = {
    {{0x70, 0xc9, 0x4e}, VENDOR_FLOCK, TYPE_ALPR},
    {{0x3c, 0x91, 0x80}, VENDOR_FLOCK, TYPE_ALPR},
    {{0xd8, 0xf3, 0xbc}, VENDOR_FLOCK, TYPE_ALPR},
    {{0x80, 0x30, 0x49}, VENDOR_FLOCK, TYPE_ALPR},
    {{0xb8, 0x35, 0x32}, VENDOR_FLOCK, TYPE_ALPR},
    {{0x14, 0x5a, 0xfc}, VENDOR_FLOCK, TYPE_ALPR},
    {{0x74, 0x4c, 0xa1}, VENDOR_FLOCK, TYPE_ALPR},
    {{0x08, 0x3a, 0x88}, VENDOR_FLOCK, TYPE_ALPR},
    {{0x9c, 0x2f, 0x9d}, VENDOR_FLOCK, TYPE_ALPR},
    {{0xc0, 0x35, 0x32}, VENDOR_FLOCK, TYPE_ALPR},
    {{0x94, 0x08, 0x53}, VENDOR_FLOCK, TYPE_ALPR},
    {{0xe4, 0xaa, 0xea}, VENDOR_FLOCK, TYPE_ALPR},
    {{0xf4, 0x6a, 0xdd}, VENDOR_FLOCK, TYPE_ALPR},
    {{0xf8, 0xa2, 0xd6}, VENDOR_FLOCK, TYPE_ALPR},
    {{0x24, 0xb2, 0xb9}, VENDOR_FLOCK, TYPE_ALPR},
    {{0x00, 0xf4, 0x8d}, VENDOR_FLOCK, TYPE_ALPR},
    {{0xd0, 0x39, 0x57}, VENDOR_FLOCK, TYPE_ALPR},
    {{0xe8, 0xd0, 0xfc}, VENDOR_FLOCK, TYPE_ALPR},
    {{0xe0, 0x4f, 0x43}, VENDOR_FLOCK, TYPE_ALPR},
    {{0xb8, 0x1e, 0xa4}, VENDOR_FLOCK, TYPE_ALPR},
    {{0x70, 0x08, 0x94}, VENDOR_FLOCK, TYPE_ALPR},
    {{0x58, 0x8e, 0x81}, VENDOR_FLOCK, TYPE_ALPR},
    {{0xec, 0x1b, 0xbd}, VENDOR_FLOCK, TYPE_ALPR},
    {{0x58, 0x00, 0xe3}, VENDOR_FLOCK, TYPE_ALPR},
    {{0x90, 0x35, 0xea}, VENDOR_FLOCK, TYPE_ALPR},
    {{0x5c, 0x93, 0xa2}, VENDOR_FLOCK, TYPE_ALPR},
    {{0x64, 0x6e, 0x69}, VENDOR_FLOCK, TYPE_ALPR},
    {{0x48, 0x27, 0xea}, VENDOR_FLOCK, TYPE_ALPR},
    {{0xcc, 0xcc, 0xcc}, VENDOR_FLOCK, TYPE_ALPR},
    {{0x04, 0x0d, 0x84}, VENDOR_FLOCK, TYPE_ALPR},
    {{0xf0, 0x82, 0xc0}, VENDOR_FLOCK, TYPE_ALPR},
    {{0x1c, 0x34, 0xf1}, VENDOR_FLOCK, TYPE_ALPR},
    {{0x38, 0x5b, 0x44}, VENDOR_FLOCK, TYPE_ALPR},
    {{0x94, 0x34, 0x69}, VENDOR_FLOCK, TYPE_ALPR},
    {{0xb4, 0xe3, 0xf9}, VENDOR_FLOCK, TYPE_ALPR},
    {{0xB4, 0x1E, 0x52}, VENDOR_FLOCK, TYPE_ALPR},
    {{0xa4, 0xcf, 0x12}, VENDOR_FLOCK, TYPE_ALPR},
    {{0x3c, 0x71, 0xbf}, VENDOR_FLOCK, TYPE_ALPR},

    {{0x00, 0x19, 0xc1}, VENDOR_MOTOROLA, TYPE_ALPR},
    {{0x00, 0x1e, 0x0b}, VENDOR_MOTOROLA, TYPE_ALPR},
    {{0x00, 0x24, 0x37}, VENDOR_MOTOROLA, TYPE_ALPR},
    {{0x6c, 0x96, 0xcf}, VENDOR_MOTOROLA, TYPE_ALPR},
    {{0x70, 0xb3, 0xd5}, VENDOR_MOTOROLA, TYPE_ALPR},
    {{0x84, 0x24, 0x8d}, VENDOR_MOTOROLA, TYPE_ALPR},
    {{0xd0, 0xd9, 0x4f}, VENDOR_MOTOROLA, TYPE_ALPR},
    {{0x00, 0x0f, 0x8f}, VENDOR_MOTOROLA, TYPE_ALPR},
    {{0x00, 0x18, 0xba}, VENDOR_MOTOROLA, TYPE_ALPR},
    {{0x10, 0x74, 0x6F}, VENDOR_MOTOROLA, TYPE_ALPR},
    {{0x9C, 0x86, 0x2B}, VENDOR_MOTOROLA, TYPE_ALPR},
    {{0xB8, 0xE2, 0x8C}, VENDOR_MOTOROLA, TYPE_ALPR},

    {{0x00, 0x25, 0xdf}, VENDOR_AXON, TYPE_BODY_CAM},
    {{0x94, 0xc6, 0x91}, VENDOR_AXON, TYPE_BODY_CAM},

    {{0x00, 0x40, 0x7f}, VENDOR_HIKVISION, TYPE_SECURITY_CAM},
    {{0x28, 0x57, 0xbe}, VENDOR_HIKVISION, TYPE_SECURITY_CAM},
    {{0x44, 0x19, 0xb6}, VENDOR_HIKVISION, TYPE_SECURITY_CAM},
    {{0xb0, 0x93, 0x5b}, VENDOR_HIKVISION, TYPE_SECURITY_CAM},
    {{0xc4, 0x2f, 0x90}, VENDOR_HIKVISION, TYPE_SECURITY_CAM},
    {{0xc0, 0x51, 0x7e}, VENDOR_HIKVISION, TYPE_SECURITY_CAM},
    {{0xf8, 0x4d, 0xfc}, VENDOR_HIKVISION, TYPE_SECURITY_CAM},
    {{0xbc, 0xad, 0x28}, VENDOR_HIKVISION, TYPE_SECURITY_CAM},

    {{0x00, 0x1a, 0xb0}, VENDOR_DAHUA, TYPE_SECURITY_CAM},
    {{0x14, 0xa7, 0x8b}, VENDOR_DAHUA, TYPE_SECURITY_CAM},
    {{0x4c, 0x11, 0xbf}, VENDOR_DAHUA, TYPE_SECURITY_CAM},
    {{0x90, 0x02, 0xa9}, VENDOR_DAHUA, TYPE_SECURITY_CAM},
    {{0xa0, 0xbd, 0x1d}, VENDOR_DAHUA, TYPE_SECURITY_CAM},
    {{0xfc, 0x5f, 0x49}, VENDOR_DAHUA, TYPE_SECURITY_CAM},

    {{0x00, 0x40, 0x8c}, VENDOR_AXIS, TYPE_SECURITY_CAM},
    {{0xac, 0xcc, 0x8e}, VENDOR_AXIS, TYPE_SECURITY_CAM},
    {{0xb8, 0xa4, 0x4f}, VENDOR_AXIS, TYPE_SECURITY_CAM},

    {{0x00, 0x1c, 0x44}, VENDOR_BOSCH, TYPE_SECURITY_CAM},
    {{0x1c, 0x1a, 0xc0}, VENDOR_BOSCH, TYPE_SECURITY_CAM},

    {{0xb0, 0x09, 0xda}, VENDOR_RING, TYPE_DOORBELL},
    {{0xf0, 0x81, 0x73}, VENDOR_RING, TYPE_DOORBELL},
    {{0x88, 0xa9, 0xa7}, VENDOR_RING, TYPE_DOORBELL},
    

    {{0x18, 0xd6, 0xc7}, VENDOR_NEST, TYPE_SECURITY_CAM},
    {{0x64, 0x16, 0x66}, VENDOR_NEST, TYPE_SECURITY_CAM},

    {{0x2c, 0xaa, 0x8e}, VENDOR_WYZE, TYPE_SECURITY_CAM},
    {{0x7c, 0x78, 0xb2}, VENDOR_WYZE, TYPE_SECURITY_CAM},

    {{0xec, 0x71, 0xdb}, VENDOR_REOLINK, TYPE_SECURITY_CAM},
    {{0xf4, 0xbf, 0x80}, VENDOR_REOLINK, TYPE_SECURITY_CAM},

    {{0xdc, 0xef, 0xca}, VENDOR_ARLO, TYPE_SECURITY_CAM},

    {{0x24, 0xa4, 0x3c}, VENDOR_UBIQUITI, TYPE_SECURITY_CAM},
    {{0x74, 0x83, 0xc2}, VENDOR_UBIQUITI, TYPE_SECURITY_CAM},
    {{0x78, 0x8a, 0x20}, VENDOR_UBIQUITI, TYPE_SECURITY_CAM},
    {{0xf0, 0x9f, 0xc2}, VENDOR_UBIQUITI, TYPE_SECURITY_CAM},
};
static const size_t VENDOR_DB_COUNT = sizeof(vendorDatabase) / sizeof(vendorDatabase[0]);

static const char *ALPR_SSID_PATTERNS[] = {
    // Flock
    "flock",
    "FS Ext Battery",
    "Penguin",
    "Pigvision",
    "FalconLPR",
    "FS-",
    // Motorola/Vigilant
    "vigilant",
    "LEARN-",
    "avigilon",
    "PIPS",
    "MotoALPR",
    // Axon
    "AxonFleet",
    "axon-",
    "Fleet3",
    // Genetec
    "AutoVu",
    "genetec",
    // Hikvision
    "HIK-",
    "Hikvision",
    "DS-",
    // Dahua
    "DAHUA-",
    "IPC-",
    "DHI-",
    // Axis
    "AXIS-",
    "axis-",
    // Bosch
    "BOSCH-",
    "AUTODOME",
    // Ring
    "Ring-",
    "ring_setup",
    // Nest
    "Nest-",
    "nest-cam",
    // Wyze
    "WyzeCam",
    "wyze-",
    // Reolink
    "Reolink",
    "REOL-",
    // Arlo
    "Arlo-",
    "arlo_",
    // UniFi
    "UniFi-",
    "unifi-cam",
    "UVC-",
    // Generic
    "ALPR",
    "LPR-",
    "plate",
    "PLATE",
    "BodyCam",
    "bodycam",
    "BWC-",
    "trafficcam",
    "traffic-",
};
static const size_t ALPR_SSID_PATTERN_COUNT =
    sizeof(ALPR_SSID_PATTERNS) / sizeof(ALPR_SSID_PATTERNS[0]);

bool alprDetectorActive = false;
uint32_t alprDetectionsLogged = 0;
uint32_t alprLastDetectionTime = 0;
int8_t alprLastRSSI = 0;
uint8_t alprLastChannel = 0;
char alprLastMAC[18] = "";
char alprLastSSID[33] = "";
uint8_t alprCurrentChannel = 1;
bool alprJustDetected = false;
uint8_t alprLastConfidence = 0;
ALPRClassification alprLastClassification = ALPR_CLASS_IGNORE;
ALPRVendor alprLastVendor = VENDOR_UNKNOWN;
ALPRDeviceType alprLastType = TYPE_UNKNOWN;
char alprLastSerial[24] = "";

static String _sessionTimestamp = "";
static uint32_t _sessionPart = 0;

static uint32_t _countDefinite = 0;
static uint32_t _countLikely = 0;
static uint32_t _countProbable = 0;
static uint32_t _countPossible = 0;
static uint32_t _countBeacons = 0;

static const uint8_t alpr_channels[] = {11, 6, 1};
static const size_t alpr_channel_count = sizeof(alpr_channels) / sizeof(alpr_channels[0]);
static size_t _channelIndex = 0;
static uint32_t _lastHop = 0;

static char *strcasestr_local(const char *haystack, const char *needle)
{
    if (!needle || !*needle)
        return (char *)haystack;
    if (!haystack)
        return nullptr;
    for (; *haystack; ++haystack)
    {
        const char *h = haystack;
        const char *n = needle;
        while (*h && *n && tolower((unsigned char)*h) == tolower((unsigned char)*n))
        {
            ++h;
            ++n;
        }
        if (!*n)
            return (char *)haystack;
    }
    return nullptr;
}

bool isALPRSSID(const char *ssid)
{
    if (!ssid || strlen(ssid) == 0)
        return false;
    for (size_t i = 0; i < ALPR_SSID_PATTERN_COUNT; i++)
    {
        if (strcasestr_local(ssid, ALPR_SSID_PATTERNS[i]))
        {
            return true;
        }
    }
    return false;
}

#define ALPR_ALERT_QUEUE_SIZE 32

struct ALPRAlert
{
    uint8_t mac[6];
    int8_t rssi;
    uint8_t channel;
    uint16_t flags;
    char ssid[33];
    uint8_t frameSubtype;
    char serial[24];
};

static volatile ALPRAlert _alertQueue[ALPR_ALERT_QUEUE_SIZE];
static volatile size_t _alertHead = 0;
static volatile size_t _alertTail = 0;
static portMUX_TYPE _queueMux = portMUX_INITIALIZER_UNLOCKED;

static void IRAM_ATTR enqueueAlert(const uint8_t *mac, int8_t rssi,
                                   uint8_t ch, uint16_t flags,
                                   const char *ssid, uint8_t frameSubtype,
                                   const char *serial = nullptr)
{
    portENTER_CRITICAL_ISR(&_queueMux);
    size_t next = (_alertHead + 1) % ALPR_ALERT_QUEUE_SIZE;
    if (next == _alertTail)
    {
        portEXIT_CRITICAL_ISR(&_queueMux);
        return;
    }

    ALPRAlert *a = (ALPRAlert *)&_alertQueue[_alertHead];
    memcpy((void *)a->mac, mac, 6);
    a->rssi = rssi;
    a->channel = ch;
    a->flags = flags;
    a->frameSubtype = frameSubtype;

    if (ssid && ssid[0])
    {
        size_t n = strlen(ssid);
        if (n > 32)
            n = 32;
        memcpy((void *)a->ssid, ssid, n);
        ((char *)a->ssid)[n] = '\0';
    }
    else
    {
        ((char *)a->ssid)[0] = '\0';
    }

    if (serial && serial[0])
    {
        size_t n = strlen(serial);
        if (n > 23)
            n = 23;
        memcpy((void *)a->serial, serial, n);
        ((char *)a->serial)[n] = '\0';
    }
    else
    {
        ((char *)a->serial)[0] = '\0';
    }

    _alertHead = next;
    portEXIT_CRITICAL_ISR(&_queueMux);
}

#if ALPR_ENABLE_BLE

#include <NimBLEDevice.h>

static bool bleActive = false;
static NimBLEScan *pBLEScan = nullptr;

static uint16_t detectFlockBLE(const uint8_t *payload, size_t len,
                               const String &name, String *serialOut)
{
    if (!payload || len < 4)
        return ALPR_FLAG_NONE;

    uint16_t flags = ALPR_FLAG_NONE;

    bool hasXuntong = false;
    size_t mfgIndex = 0;

    for (size_t i = 1; i + 2 < len; i++)
    {
        if (payload[i] == 0xFF &&
            payload[i + 1] == 0xC8 &&
            payload[i + 2] == 0x09)
        {
            hasXuntong = true;
            mfgIndex = i;
            break;
        }
    }

    if (hasXuntong)
    {
        flags |= ALPR_FLAG_BLE_XUNTONG;
    }

    if (name.length() > 0)
    {
        if (name.startsWith("Penguin-") && name.length() == 18)
        {
            bool allDigits = true;
            for (int i = 8; i < (int)name.length(); i++)
            {
                char c = name.charAt(i);
                if (c < '0' || c > '9')
                {
                    allDigits = false;
                    break;
                }
            }
            if (allDigits)
            {
                flags |= ALPR_FLAG_BLE_PENGUIN;
            }
        }

        if (name == "FS Ext Battery")
        {
            flags |= ALPR_FLAG_BLE_FS_BATTERY;
        }

        if (name.length() == 10)
        {
            bool allDigits = true;
            for (int i = 0; i < (int)name.length(); i++)
            {
                char c = name.charAt(i);
                if (c < '0' || c > '9')
                {
                    allDigits = false;
                    break;
                }
            }
            if (allDigits)
            {
                flags |= ALPR_FLAG_BLE_10DIGIT;
            }
        }
    }

    if (serialOut != nullptr && hasXuntong)
    {
        *serialOut = "";

        if (mfgIndex > 0)
        {
            uint8_t adLen = payload[mfgIndex - 1];
            size_t adStart = mfgIndex - 1;
            size_t adEnd = adStart + adLen;
            if (adEnd > len)
                adEnd = len;

            size_t vendorStart = mfgIndex + 3;

            if (vendorStart < adEnd)
            {
                bool started = false;

                for (size_t k = vendorStart; k < adEnd; k++)
                {
                    char c = (char)payload[k];

                    if (!started)
                    {
                        if (c == 'T' && (k + 1) < adEnd &&
                            (char)payload[k + 1] == 'N')
                        {
                            started = true;
                            *serialOut += 'T';
                            *serialOut += 'N';
                            k++;
                        }
                    }
                    else
                    {
                        if (c >= '0' && c <= '9')
                        {
                            *serialOut += c;
                        }
                        else if (c == ' ' || c == '#' || c == '-')
                        {
                            continue;
                        }
                        else
                        {
                            break;
                        }
                    }
                }
            }
        }
    }

    return flags;
}

class ALPRBLECallbacks : public NimBLEScanCallbacks
{
public:
    void onResult(const NimBLEAdvertisedDevice *device) override
    {
        if (!alprDetectorActive)
            return;
        if (!bleActive)
            return;
        if (!device)
            return;

        String name = String(device->getName().c_str());
        String macStr = String(device->getAddress().toString().c_str());
        macStr.toUpperCase();
        int rssi = device->getRSSI();

        if (rssi < ALPR_RSSI_MIN)
            return;

        std::vector<uint8_t> payloadVec = device->getPayload();
        const uint8_t *payload = payloadVec.data();
        size_t payloadLen = payloadVec.size();

        String serial;
        uint16_t bleFlags = detectFlockBLE(payload, payloadLen, name, &serial);

        if (bleFlags == ALPR_FLAG_NONE)
            return;

        uint8_t macBytes[6];
        sscanf(macStr.c_str(), "%hhx:%hhx:%hhx:%hhx:%hhx:%hhx",
               &macBytes[0], &macBytes[1], &macBytes[2],
               &macBytes[3], &macBytes[4], &macBytes[5]);

        Serial.printf("[ALPR-BLE] MAC=%s Name='%s' Serial='%s' RSSI=%d flags=0x%04x\n",
                      macStr.c_str(),
                      name.length() ? name.c_str() : "(none)",
                      serial.length() ? serial.c_str() : "N/A",
                      rssi, bleFlags);

        enqueueAlert(macBytes, rssi, 0, bleFlags,
                     name.length() ? name.c_str() : "",
                     0xFF,
                     serial.length() ? serial.c_str() : nullptr);
    }
};

static ALPRBLECallbacks bleCallbacks;

static void startBLEScan()
{
    if (bleActive)
        return;

    if (!NimBLEDevice::isInitialized())
    {
        Serial.println("[ALPR] Warning: BLE not initialized at boot!");
        return;
    }

    pBLEScan = NimBLEDevice::getScan();
    pBLEScan->setScanCallbacks(&bleCallbacks, false);
    pBLEScan->setActiveScan(true);
    pBLEScan->setInterval(100);
    pBLEScan->setWindow(99);
    pBLEScan->start(0, false);

    bleActive = true;
    Serial.println("[ALPR] BLE scanning started");
}

static void stopBLEScan()
{
    if (!bleActive)
        return;

    bleActive = false;

    if (pBLEScan)
    {
        pBLEScan->stop();
        delay(50);
        pBLEScan->clearResults();
        pBLEScan->setScanCallbacks(nullptr, false);
    }

    pBLEScan = nullptr;

    Serial.println("[ALPR] BLE scanning stopped");
}

#endif

ALPRVendor alprIdentifyVendor(const uint8_t *mac)
{
    if (!mac)
        return VENDOR_UNKNOWN;
    if (mac[0] & 0x02)
        return VENDOR_UNKNOWN;

    for (size_t i = 0; i < VENDOR_DB_COUNT; i++)
    {
        if (mac[0] == vendorDatabase[i].oui[0] &&
            mac[1] == vendorDatabase[i].oui[1] &&
            mac[2] == vendorDatabase[i].oui[2])
        {
            return vendorDatabase[i].vendor;
        }
    }
    return VENDOR_UNKNOWN;
}

ALPRDeviceType alprGuessDeviceType(ALPRVendor v, const char *ssid)
{
    if (ssid && strlen(ssid) > 0)
    {
        if (strcasestr_local(ssid, "doorbell") || strcasestr_local(ssid, "ring"))
            return TYPE_DOORBELL;
        if (strcasestr_local(ssid, "alpr") || strcasestr_local(ssid, "lpr") ||
            strcasestr_local(ssid, "plate") || strcasestr_local(ssid, "flock") ||
            strcasestr_local(ssid, "vigilant") || strcasestr_local(ssid, "falcon"))
            return TYPE_ALPR;
        if (strcasestr_local(ssid, "bodycam") || strcasestr_local(ssid, "bwc"))
            return TYPE_BODY_CAM;
        if (strcasestr_local(ssid, "traffic"))
            return TYPE_TRAFFIC_CAM;
    }

    switch (v)
    {
    case VENDOR_FLOCK:
        return TYPE_ALPR;
    case VENDOR_MOTOROLA:
        return TYPE_ALPR;
    case VENDOR_AXON:
        return TYPE_BODY_CAM;
    case VENDOR_GENETEC:
        return TYPE_ALPR;
    case VENDOR_REKOR:
        return TYPE_ALPR;
    case VENDOR_HIKVISION:
        return TYPE_SECURITY_CAM;
    case VENDOR_DAHUA:
        return TYPE_SECURITY_CAM;
    case VENDOR_AXIS:
        return TYPE_SECURITY_CAM;
    case VENDOR_BOSCH:
        return TYPE_SECURITY_CAM;
    case VENDOR_RING:
        return TYPE_DOORBELL;
    case VENDOR_NEST:
        return TYPE_SECURITY_CAM;
    case VENDOR_WYZE:
        return TYPE_SECURITY_CAM;
    case VENDOR_REOLINK:
        return TYPE_SECURITY_CAM;
    case VENDOR_ARLO:
        return TYPE_SECURITY_CAM;
    case VENDOR_UBIQUITI:
        return TYPE_SECURITY_CAM;
    default:
        return TYPE_UNKNOWN;
    }
}

const char *alprVendorName(ALPRVendor v)
{
    switch (v)
    {
    case VENDOR_FLOCK:
        return "Flock";
    case VENDOR_MOTOROLA:
        return "Motorola";
    case VENDOR_AXON:
        return "Axon";
    case VENDOR_GENETEC:
        return "Genetec";
    case VENDOR_REKOR:
        return "Rekor";
    case VENDOR_HIKVISION:
        return "Hikvision";
    case VENDOR_DAHUA:
        return "Dahua";
    case VENDOR_AXIS:
        return "Axis";
    case VENDOR_BOSCH:
        return "Bosch";
    case VENDOR_NEOLOGY:
        return "Neology";
    case VENDOR_PERCEPTICS:
        return "Perceptics";
    case VENDOR_ELSAG:
        return "ELSAG";
    case VENDOR_RING:
        return "Ring";
    case VENDOR_NEST:
        return "Nest";
    case VENDOR_WYZE:
        return "Wyze";
    case VENDOR_REOLINK:
        return "Reolink";
    case VENDOR_ARLO:
        return "Arlo";
    case VENDOR_UBIQUITI:
        return "UniFi";
    default:
        return "Unknown";
    }
}

const char *alprDeviceTypeName(ALPRDeviceType t)
{
    switch (t)
    {
    case TYPE_ALPR:
        return "ALPR";
    case TYPE_SECURITY_CAM:
        return "Camera";
    case TYPE_DOORBELL:
        return "Doorbell";
    case TYPE_BODY_CAM:
        return "BodyCam";
    case TYPE_TRAFFIC_CAM:
        return "Traffic";
    default:
        return "Unknown";
    }
}

const char *alprClassificationName(ALPRClassification c)
{
    switch (c)
    {
    case ALPR_CLASS_DEFINITE:
        return "DEFINITE";
    case ALPR_CLASS_LIKELY:
        return "LIKELY";
    case ALPR_CLASS_PROBABLE:
        return "PROBABLE";
    case ALPR_CLASS_POSSIBLE:
        return "POSSIBLE";
    default:
        return "IGNORE";
    }
}

struct ALPRSeenEntry
{
    uint8_t state;
    uint32_t hash;
    char mac[18];
    char ssid[33];
    uint32_t firstSeen;
    uint32_t lastSeen;
    uint16_t count;
    int8_t lastRSSI;
    uint8_t lastChannel;

    uint16_t flags;
    uint8_t confidence;
    ALPRClassification classification;
    ALPRVendor vendor;
    ALPRDeviceType deviceType;

    int8_t rssiHistory[ALPR_RSSI_HISTORY_SIZE];
    uint8_t rssiHistoryIdx;
    uint8_t rssiHistoryCount;
    uint8_t channelsSeen;
    uint32_t burstStartTime;
    uint8_t burstProbeCount;
    uint32_t recentProbeTimes[8];
    uint8_t recentProbeIdx;

    uint16_t beaconCount;
    uint16_t probeReqCount;
    uint16_t probeRespCount;

    float lat;
    float lon;
    float firstLat;
    float firstLon;

    char serial[24];
    bool isBLE;
};

static ALPRSeenEntry *_seenTable = nullptr;
static int _seenCount = 0;

static uint32_t fnvHash(const String &s)
{
    uint32_t h = 2166136261u;
    for (int i = 0; i < (int)s.length(); i++)
    {
        h ^= (uint8_t)s[i];
        h *= 16777619u;
    }
    return h;
}

static void clearSeen()
{
    if (!_seenTable)
    {
        _seenCount = 0;
        return;
    }
    for (int i = 0; i < ALPR_SEEN_CAPACITY; i++)
    {
        memset(&_seenTable[i], 0, sizeof(ALPRSeenEntry));
    }
    _seenCount = 0;
}

static bool allocSeen()
{
    if (_seenTable)
        return true;
    _seenTable = new ALPRSeenEntry[ALPR_SEEN_CAPACITY];
    if (!_seenTable)
    {
        Serial.println("[ALPR] Warning — failed to allocate dedup table.");
        _seenCount = 0;
        return false;
    }
    clearSeen();
    Serial.printf("[ALPR] Dedup table allocated (%d entries, %d bytes)\n",
                  ALPR_SEEN_CAPACITY,
                  ALPR_SEEN_CAPACITY * (int)sizeof(ALPRSeenEntry));
    return true;
}

static void freeSeen()
{
    if (!_seenTable)
    {
        _seenCount = 0;
        return;
    }
    delete[] _seenTable;
    _seenTable = nullptr;
    _seenCount = 0;
    Serial.println("[ALPR] Dedup table freed");
}

static int findSeen(const String &mac)
{
    if (!_seenTable)
        return -1;
    uint32_t h = fnvHash(mac);
    uint32_t idx = h % ALPR_SEEN_CAPACITY;

    for (int probe = 0; probe < ALPR_SEEN_CAPACITY; probe++)
    {
        uint32_t i = (idx + probe) % ALPR_SEEN_CAPACITY;
        if (_seenTable[i].state == ALPR_SEEN_EMPTY)
            return -1;
        if (_seenTable[i].hash == h && strcmp(_seenTable[i].mac, mac.c_str()) == 0)
            return (int)i;
    }
    return -1;
}

static uint8_t calculateConfidence(uint16_t flags)
{
    uint16_t score = 0;
    if (flags & ALPR_FLAG_OUI_MATCH)
        score += POINTS_OUI_MATCH;
    if (flags & ALPR_FLAG_WILDCARD_PROBE)
        score += POINTS_WILDCARD_PROBE;
    if (flags & ALPR_FLAG_LITEON_IE)
        score += POINTS_LITEON_IE;
    if (flags & ALPR_FLAG_PACK_SIGNATURE)
        score += POINTS_PACK_SIGNATURE;
    if (flags & ALPR_FLAG_BURST_PATTERN)
        score += POINTS_BURST_PATTERN;
    if (flags & ALPR_FLAG_STATIONARY_RSSI)
        score += POINTS_STATIONARY_RSSI;
    if (flags & ALPR_FLAG_SUSTAINED_PROBE)
        score += POINTS_SUSTAINED_PROBE;
    if (flags & ALPR_FLAG_CHANNEL_HOPPING)
        score += POINTS_CHANNEL_HOPPING;
    if (flags & ALPR_FLAG_SSID_MATCH)
        score += POINTS_SSID_MATCH;
    if (flags & ALPR_FLAG_BEACON_MATCH)
        score += POINTS_BEACON_MATCH;
    if (flags & ALPR_FLAG_PROBE_RESP)
        score += POINTS_PROBE_RESP;
    if (flags & ALPR_FLAG_BLE_XUNTONG)
        score += POINTS_BLE_XUNTONG;
    if (flags & ALPR_FLAG_BLE_PENGUIN)
        score += POINTS_BLE_PENGUIN;
    if (flags & ALPR_FLAG_BLE_FS_BATTERY)
        score += POINTS_BLE_FS_BATTERY;
    if (flags & ALPR_FLAG_BLE_10DIGIT)
        score += POINTS_BLE_10DIGIT;
    if (flags & ALPR_FLAG_BLE_NAME_MATCH)
        score += POINTS_BLE_NAME_MATCH;
    return (score > 100) ? 100 : (uint8_t)score;
}

static ALPRClassification scoreToClassification(uint8_t score)
{
    if (score >= ALPR_CONFIDENCE_DEFINITE)
        return ALPR_CLASS_DEFINITE;
    if (score >= ALPR_CONFIDENCE_LIKELY)
        return ALPR_CLASS_LIKELY;
    if (score >= ALPR_CONFIDENCE_PROBABLE)
        return ALPR_CLASS_PROBABLE;
    if (score >= ALPR_CONFIDENCE_POSSIBLE)
        return ALPR_CLASS_POSSIBLE;
    return ALPR_CLASS_IGNORE;
}

static bool checkRssiStability(ALPRSeenEntry &e)
{
    if (e.rssiHistoryCount < 4)
        return false;
    int8_t minRssi = 127, maxRssi = -127;
    for (int i = 0; i < e.rssiHistoryCount; i++)
    {
        if (e.rssiHistory[i] < minRssi)
            minRssi = e.rssiHistory[i];
        if (e.rssiHistory[i] > maxRssi)
            maxRssi = e.rssiHistory[i];
    }
    return ((maxRssi - minRssi) <= ALPR_RSSI_STABLE_MAX_VAR);
}

static bool checkSustainedProbing(ALPRSeenEntry &e, uint32_t now)
{
    int probesInWindow = 0;
    for (int i = 0; i < 8; i++)
    {
        if (e.recentProbeTimes[i] > 0 &&
            (now - e.recentProbeTimes[i]) <= ALPR_SUSTAINED_WINDOW_MS)
        {
            probesInWindow++;
        }
    }
    float rate = (probesInWindow * 1000.0f) / ALPR_SUSTAINED_WINDOW_MS;
    return (rate >= ALPR_SUSTAINED_RATE);
}

static bool checkBurstPattern(ALPRSeenEntry &e, uint32_t now)
{
    if ((now - e.burstStartTime) < ALPR_BURST_WINDOW_MS &&
        e.burstProbeCount >= ALPR_BURST_MIN_COUNT)
    {
        return true;
    }
    return false;
}

static bool checkChannelHopping(ALPRSeenEntry &e)
{
    int channelCount = 0;
    if (e.channelsSeen & (1 << 0))
        channelCount++;
    if (e.channelsSeen & (1 << 1))
        channelCount++;
    if (e.channelsSeen & (1 << 2))
        channelCount++;
    return (channelCount >= ALPR_CHANNEL_HOP_MIN);
}

static void updateBehavioralTracking(ALPRSeenEntry &e, int8_t rssi, uint8_t channel, uint32_t now)
{
    e.rssiHistory[e.rssiHistoryIdx] = rssi;
    e.rssiHistoryIdx = (e.rssiHistoryIdx + 1) % ALPR_RSSI_HISTORY_SIZE;
    if (e.rssiHistoryCount < ALPR_RSSI_HISTORY_SIZE)
        e.rssiHistoryCount++;

    if (channel == 1)
        e.channelsSeen |= (1 << 0);
    if (channel == 6)
        e.channelsSeen |= (1 << 1);
    if (channel == 11)
        e.channelsSeen |= (1 << 2);

    e.recentProbeTimes[e.recentProbeIdx] = now;
    e.recentProbeIdx = (e.recentProbeIdx + 1) % 8;

    if (e.burstStartTime == 0 || (now - e.burstStartTime) > ALPR_BURST_WINDOW_MS)
    {
        e.burstStartTime = now;
        e.burstProbeCount = 1;
    }
    else if ((now - e.lastSeen) <= ALPR_BURST_SPACING_MS)
    {
        if (e.burstProbeCount < 255)
            e.burstProbeCount++;
    }
    else
    {
        e.burstStartTime = now;
        e.burstProbeCount = 1;
    }
}

static void updateFlags(ALPRSeenEntry &e, uint32_t now)
{
    if (checkRssiStability(e))
        e.flags |= ALPR_FLAG_STATIONARY_RSSI;
    else
        e.flags &= ~ALPR_FLAG_STATIONARY_RSSI;

    if (checkSustainedProbing(e, now))
        e.flags |= ALPR_FLAG_SUSTAINED_PROBE;
    else
        e.flags &= ~ALPR_FLAG_SUSTAINED_PROBE;

    if (checkBurstPattern(e, now))
        e.flags |= ALPR_FLAG_BURST_PATTERN;
    else
        e.flags &= ~ALPR_FLAG_BURST_PATTERN;

    if (checkChannelHopping(e))
        e.flags |= ALPR_FLAG_CHANNEL_HOPPING;

    e.confidence = calculateConfidence(e.flags);
    e.classification = scoreToClassification(e.confidence);
}

static void checkAndRotateSession()
{
    if (!_seenTable)
        return;
    if (_seenCount < ALPR_SEEN_CAPACITY - 100)
        return;

    _sessionPart++;
    Serial.println("[ALPR] ──────────────────────────────────────");
    Serial.printf("[ALPR]  File capacity full — rotating to part %lu\n",
                  (unsigned long)_sessionPart);
    Serial.println("[ALPR] ──────────────────────────────────────");
}

static String getTimeString()
{
    LocalTime lt = gpsGetLocalTime();
    char buf[16];
    if (lt.valid)
        snprintf(buf, sizeof(buf), "%02d:%02d:%02d", lt.hour, lt.minute, lt.second);
    else
        snprintf(buf, sizeof(buf), "%02d:%02d:%02d", gpsData.hour, gpsData.minute, gpsData.second);
    return String(buf);
}

static File openALPRLog()
{
    if (!sdActive)
        return File();
    if (!SD.exists("/alpr"))
        SD.mkdir("/alpr");

    String path;
    if (_sessionPart == 0)
    {
        path = "/alpr/ALPR_" + _sessionTimestamp + ".csv";
    }
    else
    {
        char partBuf[8];
        snprintf(partBuf, sizeof(partBuf), "_P%lu", (unsigned long)_sessionPart);
        path = "/alpr/ALPR_" + _sessionTimestamp + String(partBuf) + ".csv";
    }

    bool isNew = !SD.exists(path.c_str());
    File f = SD.open(path.c_str(), FILE_APPEND);

    if (f && isNew)
    {
#if ALPR_DEBUG_LOG_ALL
        f.println("# ALPR/Surveillance Device Detector - Debug Mode");
#endif
        f.println("time,mac,vendor,type,ssid,serial,transport,classification,confidence,flags,rssi,channel,channels_seen,beacons,probes,resps,lat,lon,alt,speed,sats,hdop,count,duration_ms");
        Serial.printf("[ALPR] Created new log: %s\n", path.c_str());
    }

    return f;
}

static String flagsToString(uint16_t flags)
{
    String s = "";
    if (flags & ALPR_FLAG_OUI_MATCH)
        s += "OUI|";
    if (flags & ALPR_FLAG_WILDCARD_PROBE)
        s += "WILD|";
    if (flags & ALPR_FLAG_LITEON_IE)
        s += "LITEON|";
    if (flags & ALPR_FLAG_PACK_SIGNATURE)
        s += "PACK|";
    if (flags & ALPR_FLAG_BURST_PATTERN)
        s += "BURST|";
    if (flags & ALPR_FLAG_STATIONARY_RSSI)
        s += "STABLE|";
    if (flags & ALPR_FLAG_SUSTAINED_PROBE)
        s += "SUSTAINED|";
    if (flags & ALPR_FLAG_CHANNEL_HOPPING)
        s += "HOP|";
    if (flags & ALPR_FLAG_SSID_MATCH)
        s += "SSID|";
    if (flags & ALPR_FLAG_BEACON_MATCH)
        s += "BEACON|";
    if (flags & ALPR_FLAG_PROBE_RESP)
        s += "RESP|";
    if (flags & ALPR_FLAG_BLE_XUNTONG)
        s += "XUNTONG|";
    if (flags & ALPR_FLAG_BLE_PENGUIN)
        s += "PENGUIN|";
    if (flags & ALPR_FLAG_BLE_FS_BATTERY)
        s += "FSBAT|";
    if (flags & ALPR_FLAG_BLE_10DIGIT)
        s += "10DIG|";
    if (flags & ALPR_FLAG_BLE_NAME_MATCH)
        s += "BLENAME|";
    if (s.length() > 0)
        s.remove(s.length() - 1);
    return s;
}

static String csvEscape(const char *s)
{
    String r = String(s ? s : "");
    r.replace(",", " ");
    r.replace("\"", "'");
    return r;
}

static void logDetection(const ALPRSeenEntry &e)
{
    checkAndRotateSession();

    File f = openALPRLog();
    bool hasFile = (f == true);

    uint32_t duration = e.lastSeen - e.firstSeen;
    String flagsStr = flagsToString(e.flags);
    String safeSSID = csvEscape(e.ssid);
    String safeSerial = csvEscape(e.serial);

    char line[450];
    snprintf(line, sizeof(line),
             "%s,%s,%s,%s,%s,%s,%s,%s,%d,%s,%d,%d,0x%02x,%d,%d,%d,%.6f,%.6f,%.1f,%.1f,%d,%d,%d,%lu",
             getTimeString().c_str(),
             e.mac,
             alprVendorName(e.vendor),
             alprDeviceTypeName(e.deviceType),
             safeSSID.c_str(),
             safeSerial.c_str(),
             e.isBLE ? "BLE" : "WiFi",
             alprClassificationName(e.classification),
             (int)e.confidence,
             flagsStr.c_str(),
             (int)e.lastRSSI,
             (int)e.lastChannel,
             (unsigned)e.channelsSeen,
             (int)e.beaconCount,
             (int)e.probeReqCount,
             (int)e.probeRespCount,
             gpsData.latitude,
             gpsData.longitude,
             gpsData.altitude,
             gpsData.speed,
             gpsData.satellites,
             gpsData.hdop,
             (int)e.count,
             (unsigned long)duration);

    if (hasFile)
    {
        f.println(line);
        f.close();
    }

    Serial.printf("[ALPR-%s] %s %s [%s] conf=%d [%s] SSID='%s' Serial='%s' MAC=%s\n",
                  alprClassificationName(e.classification),
                  alprVendorName(e.vendor),
                  alprDeviceTypeName(e.deviceType),
                  e.isBLE ? "BLE" : "WiFi",
                  e.confidence, flagsStr.c_str(),
                  e.ssid[0] ? e.ssid : "(none)",
                  e.serial[0] ? e.serial : "(none)",
                  e.mac);
}

typedef struct __attribute__((packed))
{
    uint16_t frame_ctrl;
    uint16_t duration;
    uint8_t addr1[6];
    uint8_t addr2[6];
    uint8_t addr3[6];
    uint16_t seq_ctrl;
} alpr_wifi_hdr_t;

#define IE_SSID 0
#define IE_VENDOR 221

static int IRAM_ATTR isWildcardProbeIE(const uint8_t *body, int len)
{
    if (!body || len < 2)
        return -1;
    while (len >= 2)
    {
        uint8_t id = body[0];
        uint8_t elen = body[1];
        if ((int)elen + 2 > len)
            break;
        if (id == 0)
            return (elen == 0) ? 1 : 0;
        body += elen + 2;
        len -= elen + 2;
    }
    return -1;
}

static bool IRAM_ATTR extractSSID(const uint8_t *body, int len,
                                  char *outSsid, size_t outLen)
{
    if (!body || len < 2 || !outSsid || outLen == 0)
        return false;
    while (len >= 2)
    {
        uint8_t id = body[0];
        uint8_t elen = body[1];
        if ((int)elen + 2 > len)
            break;
        if (id == 0)
        {
            size_t n = (elen < (outLen - 1)) ? elen : (outLen - 1);
            memcpy(outSsid, body + 2, n);
            outSsid[n] = '\0';
            return true;
        }
        body += elen + 2;
        len -= elen + 2;
    }
    return false;
}

static bool IRAM_ATTR hasLiteonVendorIE(const uint8_t *body, int bodyLen)
{
    if (!body || bodyLen < 4)
        return false;
    int i = 0;
    while (i + 2 <= bodyLen)
    {
        uint8_t id = body[i];
        int elen = (int)body[i + 1];
        if (i + 2 + elen > bodyLen)
        {
            if (elen > 200)
            {
                i += 2;
                continue;
            }
            break;
        }
        i += 2;
        if (id == IE_VENDOR && elen >= 4)
        {
            if (body[i] == 0x50 && body[i + 1] == 0x6f && body[i + 2] == 0x9a)
            {
                return true;
            }
        }
        i += elen;
    }
    return false;
}

static bool IRAM_ATTR hasPackSignature(const uint8_t *body, int bodyLen)
{
    if (!body || bodyLen < 20)
        return false;
    bool sawRates = false, sawDS = false, sawExtCap = false;
    bool sawLiteONFull = false, sawHTCap = false, sawVHTCap = false;
    bool sawMSFull = false;

    int i = 0;
    while (i + 2 <= bodyLen)
    {
        uint8_t id = body[i];
        int elen = (int)body[i + 1];
        if (i + 2 + elen > bodyLen)
        {
            if (elen > 200)
            {
                i += 2;
                continue;
            }
            break;
        }
        i += 2;

        switch (id)
        {
        case 2:
            sawRates = true;
            break;
        case 12:
            sawDS = true;
            break;
        case 127:
            sawExtCap = true;
            break;
        case 45:
            sawHTCap = true;
            break;
        case 191:
            sawVHTCap = true;
            break;
        case 221:
            if (elen >= 7)
            {
                if (body[i] == 0x50 && body[i + 1] == 0x6f && body[i + 2] == 0x9a &&
                    body[i + 3] == 0x16 && body[i + 4] == 0x03 &&
                    body[i + 5] == 0x01 && body[i + 6] == 0x03)
                {
                    sawLiteONFull = true;
                }
                else if (body[i] == 0x00 && body[i + 1] == 0x50 && body[i + 2] == 0xf2 &&
                         body[i + 3] == 0x08 && body[i + 4] == 0x00 &&
                         body[i + 5] == 0x00 && body[i + 6] == 0x00)
                {
                    sawMSFull = true;
                }
            }
            break;
        }
        i += elen;
    }

    return sawRates && sawDS && sawExtCap && sawLiteONFull &&
           sawHTCap && sawVHTCap && sawMSFull;
}

static const char ALPR_FLOCK_PROBE_IE_SIG_PRIMARY[] =
    "2,12,127,221:506f9a16030103,45,191,221:0050f208000000";

static const char ALPR_FLOCK_LITEON_IE_SIG_PREFIX[] =
    "221:506f9a16030103";

#define ALPR_FLOCK_IE_SSID          0
#define ALPR_FLOCK_IE_VENDOR        221
#define ALPR_FLOCK_PHANTOM_SKIP_CAP 16
#define ALPR_FLOCK_TLV_RESYNC_MAX   64

static void IRAM_ATTR alprFlockHexNibbles(char *dst,
                                           const uint8_t *bytes,
                                           int count)
{
    static const char hex[] = "0123456789abcdef";

    for (int i = 0; i < count; i++)
    {
        dst[i * 2]     = hex[bytes[i] >> 4];
        dst[i * 2 + 1] = hex[bytes[i] & 0x0F];
    }
}

static bool IRAM_ATTR alprFlockLiteonVendorAt(const uint8_t *ies,
                                               int len,
                                               int pos)
{
    return pos + 9 <= len &&
           ies[pos] == ALPR_FLOCK_IE_VENDOR &&
           ies[pos + 1] == 7 &&
           ies[pos + 2] == 0x50 &&
           ies[pos + 3] == 0x6F &&
           ies[pos + 4] == 0x9A;
}

static bool IRAM_ATTR alprFlockPhantomLiteonAhead(const uint8_t *ies,
                                                   int len,
                                                   int pos)
{
    int end = pos + 2 + 32;
    if (end > len - 1)
        end = len - 1;

    for (int j = pos + 2; j < end; j++)
    {
        if (alprFlockLiteonVendorAt(ies, len, j))
            return true;
    }

    return false;
}

static bool IRAM_ATTR alprFlockIsPhantomOverflow(const uint8_t *ies,
                                                  int len,
                                                  uint8_t id,
                                                  int elementLen,
                                                  int pos)
{
    if (pos + 2 + elementLen <= len)
        return false;

    if (elementLen > 200)
        return true;

    return id == 64 &&
           elementLen == 128 &&
           alprFlockPhantomLiteonAhead(ies, len, pos);
}

static int IRAM_ATTR alprFlockTlvResync(const uint8_t *ies,
                                         int len,
                                         int start)
{
    int end = start + ALPR_FLOCK_TLV_RESYNC_MAX;
    if (end > len - 1)
        end = len - 1;

    for (int j = start; j < end; j++)
    {
        int elementLen = (int)ies[j + 1];
        if (elementLen <= 200 && j + 2 + elementLen <= len)
            return j;
    }

    return -1;
}

static bool IRAM_ATTR alprFlockSigAppend(char *out,
                                         size_t cap,
                                         size_t *pos,
                                         const char *part)
{
    size_t partLen = strlen(part);

    if (*pos != 0)
    {
        if (*pos + 1 >= cap)
            return false;
        out[(*pos)++] = ',';
    }

    if (*pos + partLen >= cap)
        return false;

    memcpy(out + *pos, part, partLen);
    *pos += partLen;
    out[*pos] = '\0';

    return true;
}

static bool IRAM_ATTR alprFlockSigAppendTag(char *out,
                                            size_t cap,
                                            size_t *pos,
                                            uint8_t id)
{
    char buffer[8];
    snprintf(buffer, sizeof(buffer), "%u", (unsigned)id);

    return alprFlockSigAppend(out, cap, pos, buffer);
}

static bool IRAM_ATTR alprFlockSigAppendVendor(char *out,
                                               size_t cap,
                                               size_t *pos,
                                               const uint8_t *body,
                                               int elementLen)
{
    char buffer[24];
    int take = elementLen < 8 ? elementLen : 8;

    buffer[0] = '2';
    buffer[1] = '2';
    buffer[2] = '1';
    buffer[3] = ':';

    alprFlockHexNibbles(buffer + 4, body, take);
    buffer[4 + take * 2] = '\0';

    return alprFlockSigAppend(out, cap, pos, buffer);
}

static bool IRAM_ATTR alprBuildFlockIeSigFromIes(const uint8_t *ies,
                                                  int len,
                                                  char *out,
                                                  size_t cap,
                                                  bool *complete)
{
    if (!ies || len < 2 || !out || cap < 2)
        return false;

    size_t pos = 0;
    out[0] = '\0';

    int i = 0;
    uint8_t phantomSkips = 0;

    while (i + 2 <= len)
    {
        uint8_t id = ies[i];
        int elementLen = (int)ies[i + 1];

        if (i + 2 + elementLen > len)
        {
            if (phantomSkips < ALPR_FLOCK_PHANTOM_SKIP_CAP &&
                alprFlockIsPhantomOverflow(ies, len, id, elementLen, i))
            {
                phantomSkips++;
                i += 2;
                continue;
            }

            int resync = alprFlockTlvResync(ies, len, i);
            if (resync > i)
            {
                i = resync;
                continue;
            }

            return false;
        }

        i += 2;

        if (id == ALPR_FLOCK_IE_SSID)
        {
            if (elementLen == 0)
            {
                while (i + 2 <= len && ies[i] == 0 && ies[i + 1] == 0)
                    i += 2;
            }
            else
            {
                i += elementLen;
            }

            continue;
        }

        if (id == ALPR_FLOCK_IE_VENDOR && elementLen >= 4)
        {
            if (!alprFlockSigAppendVendor(out, cap, &pos, ies + i, elementLen))
                return false;
        }
        else
        {
            if (!alprFlockSigAppendTag(out, cap, &pos, id))
                return false;
        }

        i += elementLen;
    }

    if (complete)
        *complete = (i == len);

    return pos > 0;
}

static void IRAM_ATTR alprCanonicalizeFlockIeSig(char *sig, size_t cap)
{
    if (!sig || cap < 8)
        return;

    if (strncmp(sig, "2,12,127,", 9) == 0 &&
        strstr(sig, ALPR_FLOCK_LITEON_IE_SIG_PREFIX) != nullptr)
    {
        return;
    }

    const char *anchor = strstr(sig, ALPR_FLOCK_LITEON_IE_SIG_PREFIX);
    if (!anchor)
        return;

    char temp[128];
    int n = snprintf(temp, sizeof(temp), "2,12,127,%s", anchor);

    if (n > 0 && (size_t)n < cap)
        memcpy(sig, temp, (size_t)n + 1);
}

static bool IRAM_ATTR alprPickBetterFlockSig(const char *a,
                                             bool aComplete,
                                             const char *b,
                                             bool bComplete,
                                             char *out,
                                             size_t cap)
{
    if (!a[0] && !b[0])
        return false;

    if (a[0] && !b[0])
    {
        strncpy(out, a, cap - 1);
        out[cap - 1] = '\0';
        return true;
    }

    if (!a[0] && b[0])
    {
        strncpy(out, b, cap - 1);
        out[cap - 1] = '\0';
        return true;
    }

    const char *pick = a;

    if (aComplete && !bComplete)
        pick = a;
    else if (!aComplete && bComplete)
        pick = b;
    else if (strlen(b) > strlen(a))
        pick = b;

    strncpy(out, pick, cap - 1);
    out[cap - 1] = '\0';

    return true;
}

static bool IRAM_ATTR alprBuildFlockIeSigFromProbeBody(const uint8_t *body,
                                                        int bodyLen,
                                                        char *out,
                                                        size_t cap)
{
    if (!body || bodyLen < 2 || !out || cap < 16)
        return false;

    char sigA[128] = {0};
    char sigB[128] = {0};

    bool completeA = false;
    bool completeB = false;

    bool okA = alprBuildFlockIeSigFromIes(
        body, bodyLen, sigA, sizeof(sigA), &completeA
    );

    bool okB = false;

    if (bodyLen >= 2 && body[0] == 0 && body[1] == 0)
    {
        okB = alprBuildFlockIeSigFromIes(
            body + 2, bodyLen - 2, sigB, sizeof(sigB), &completeB
        );
    }

    char merged[128] = {0};

    if (!alprPickBetterFlockSig(
            okA ? sigA : "",
            completeA,
            okB ? sigB : "",
            completeB,
            merged,
            sizeof(merged)))
    {
        return false;
    }

    alprCanonicalizeFlockIeSig(merged, sizeof(merged));

    strncpy(out, merged, cap - 1);
    out[cap - 1] = '\0';

    return out[0] != '\0';
}

static bool IRAM_ATTR alprFlockIeSigIsPrimary(const char *sig)
{
    return sig &&
           strcmp(sig, ALPR_FLOCK_PROBE_IE_SIG_PRIMARY) == 0;
}

static bool IRAM_ATTR alprProbeBodyFlockIeSigPrimary(const uint8_t *body,
                                                      int bodyLen)
{
    char ieSig[128];

    if (alprBuildFlockIeSigFromProbeBody(
            body, bodyLen, ieSig, sizeof(ieSig)) &&
        alprFlockIeSigIsPrimary(ieSig))
    {
        return true;
    }

    if (bodyLen > 4 &&
        alprBuildFlockIeSigFromProbeBody(
            body, bodyLen - 4, ieSig, sizeof(ieSig)) &&
        alprFlockIeSigIsPrimary(ieSig))
    {
        return true;
    }

    return false;
}

static void IRAM_ATTR alprWifiSniffer(void *buf, wifi_promiscuous_pkt_type_t type)
{
    if (!buf || !alprDetectorActive)
        return;
    if (type != WIFI_PKT_MGMT)
        return;

    wifi_promiscuous_pkt_t *pkt = (wifi_promiscuous_pkt_t *)buf;
    if (pkt->rx_ctrl.sig_len < sizeof(alpr_wifi_hdr_t))
        return;

    alpr_wifi_hdr_t *hdr = (alpr_wifi_hdr_t *)pkt->payload;
    int8_t rssi = pkt->rx_ctrl.rssi;

    if (rssi < ALPR_RSSI_MIN)
        return;

    uint8_t ch = (uint8_t)pkt->rx_ctrl.channel;
    uint16_t flags = ALPR_FLAG_NONE;
    char ssid[33] = {0};

    uint8_t fc0 = hdr->frame_ctrl & 0xFF;
    uint8_t ftype = (fc0 >> 2) & 0x03;
    uint8_t subtype = (fc0 >> 4) & 0x0F;

    bool isProbeRequest = (ftype == 0 && subtype == 4);
    bool isProbeResponse = (ftype == 0 && subtype == 5);
    bool isBeacon = (ftype == 0 && subtype == 8);

    if (!isProbeRequest && !isProbeResponse && !isBeacon)
        return;

    ALPRVendor vendor = alprIdentifyVendor(hdr->addr2);
    bool ouiMatch = (vendor != VENDOR_UNKNOWN);
    if (ouiMatch)
        flags |= ALPR_FLAG_OUI_MATCH;

    int sigLen = (int)pkt->rx_ctrl.sig_len;
    int bodyLen = sigLen - (int)sizeof(alpr_wifi_hdr_t);
    const uint8_t *body = pkt->payload + sizeof(alpr_wifi_hdr_t);

    if (isBeacon || isProbeResponse)
    {
        if (bodyLen > 12)
        {
            const uint8_t *ieBody = body + 12;
            int ieLen = bodyLen - 12;
            extractSSID(ieBody, ieLen, ssid, sizeof(ssid));
        }
        if (isProbeResponse)
            flags |= ALPR_FLAG_PROBE_RESP;
    }
    else if (isProbeRequest && bodyLen > 0)
    {
        extractSSID(body, bodyLen, ssid, sizeof(ssid));
    }

    if (strlen(ssid) > 0 && isALPRSSID(ssid))
    {
        flags |= ALPR_FLAG_SSID_MATCH;
        if (isBeacon)
            flags |= ALPR_FLAG_BEACON_MATCH;
    }

if (isProbeRequest && bodyLen > 0)
{
    int wildcard = isWildcardProbeIE(body, bodyLen);

    if (wildcard == -1 && bodyLen > 4)
    {
        wildcard = isWildcardProbeIE(body, bodyLen - 4);
    }

    if (wildcard == 1)
    {
        flags |= ALPR_FLAG_WILDCARD_PROBE;
    }

    bool exactFlockProbe =
        (vendor == VENDOR_FLOCK) &&
        (wildcard == 1) &&
        alprProbeBodyFlockIeSigPrimary(body, bodyLen);

    if (exactFlockProbe)
    {
        flags |= ALPR_FLAG_LITEON_IE;
        flags |= ALPR_FLAG_PACK_SIGNATURE;
    }
    else
    {
        if (hasLiteonVendorIE(body, bodyLen))
            flags |= ALPR_FLAG_LITEON_IE;

        if (hasPackSignature(body, bodyLen))
            flags |= ALPR_FLAG_PACK_SIGNATURE;
    }
}

    if (flags == ALPR_FLAG_NONE)
        return;
    if (flags == ALPR_FLAG_WILDCARD_PROBE)
        return;

#if ALPR_DEBUG_LOG_ALL
    if (ouiMatch || (ssid[0] && (flags != ALPR_FLAG_NONE)))
    {
        Serial.printf("[ALPR-DBG] MAC=%02x:%02x:%02x:%02x:%02x:%02x [%s] SSID='%s' sub=%d flags=0x%04x rssi=%d ch=%d\n",
                      hdr->addr2[0], hdr->addr2[1], hdr->addr2[2],
                      hdr->addr2[3], hdr->addr2[4], hdr->addr2[5],
                      alprVendorName(vendor),
                      ssid[0] ? ssid : "(none)",
                      subtype, flags, rssi, ch);
    }
#endif

    enqueueAlert(hdr->addr2, rssi, ch, flags, ssid, subtype);
}

static void processAlert(const ALPRAlert &alert)
{
    char macStr[18];
    snprintf(macStr, sizeof(macStr), "%02x:%02x:%02x:%02x:%02x:%02x",
             alert.mac[0], alert.mac[1], alert.mac[2],
             alert.mac[3], alert.mac[4], alert.mac[5]);

    String macString = String(macStr);
    uint32_t now = millis();

    if (!_seenTable)
        return;

int existing = findSeen(macString);
ALPRSeenEntry *entry = nullptr;
bool isNew = false;
uint32_t previousLastSeen = 0;

    if (existing >= 0)
    {
        entry = &_seenTable[existing];
        previousLastSeen = entry->lastSeen;
        entry->flags |= alert.flags;
        if (entry->count < 0xFFFF)
            entry->count++;

        if (entry->ssid[0] == '\0' && alert.ssid[0] != '\0')
        {
            strlcpy(entry->ssid, alert.ssid, sizeof(entry->ssid));
            entry->deviceType = alprGuessDeviceType(entry->vendor, entry->ssid);
        }

        if (entry->serial[0] == '\0' && alert.serial[0] != '\0')
        {
            strlcpy(entry->serial, alert.serial, sizeof(entry->serial));
        }

        if (alert.frameSubtype == 0xFF)
        {
            entry->isBLE = true;
        }
    }

    else
    {
        if (_seenCount >= ALPR_SEEN_CAPACITY - 1)
            return;

        uint32_t h = fnvHash(macString);
        uint32_t idx = h % ALPR_SEEN_CAPACITY;

        for (int probe = 0; probe < ALPR_SEEN_CAPACITY; probe++)
        {
            uint32_t i = (idx + probe) % ALPR_SEEN_CAPACITY;
            if (_seenTable[i].state == ALPR_SEEN_EMPTY)
            {
                entry = &_seenTable[i];
                memset(entry, 0, sizeof(ALPRSeenEntry));
                entry->state = ALPR_SEEN_USED;
                entry->hash = h;
                strlcpy(entry->mac, macStr, sizeof(entry->mac));

                if (alert.ssid[0])
                {
                    strlcpy(entry->ssid, alert.ssid, sizeof(entry->ssid));
                }

                if (alert.serial[0])
                {
                    strlcpy(entry->serial, alert.serial, sizeof(entry->serial));
                }

                entry->firstSeen = now;
                entry->count = 1;
                entry->flags = alert.flags;
                entry->vendor = alprIdentifyVendor(alert.mac);
                entry->deviceType = alprGuessDeviceType(entry->vendor, entry->ssid);
                entry->firstLat = gpsData.valid ? gpsData.latitude : 0.0f;
                entry->firstLon = gpsData.valid ? gpsData.longitude : 0.0f;
                entry->isBLE = (alert.frameSubtype == 0xFF);

                _seenCount++;
                isNew = true;
                break;
            }
        }

        if (!entry)
            return;
    }

    entry->lastSeen = now;
    entry->lastRSSI = alert.rssi;
    entry->lastChannel = alert.channel;

    if (alert.frameSubtype == 4)
        entry->probeReqCount++;
    else if (alert.frameSubtype == 5)
        entry->probeRespCount++;
    else if (alert.frameSubtype == 8)
    {
        entry->beaconCount++;
        _countBeacons++;
    }

    if (gpsData.valid)
    {
        entry->lat = gpsData.latitude;
        entry->lon = gpsData.longitude;
    }

    if (alert.frameSubtype == 4)
    {
        updateBehavioralTracking(*entry, alert.rssi, alert.channel, now);
    }

    bool isBLE = (alert.frameSubtype == 0xFF);
    if (isBLE && (alert.flags & (ALPR_FLAG_BLE_XUNTONG |
                                 ALPR_FLAG_BLE_PENGUIN |
                                 ALPR_FLAG_BLE_FS_BATTERY)))
    {
        if (entry->vendor == VENDOR_UNKNOWN)
        {
            entry->vendor = VENDOR_FLOCK;
            entry->deviceType = TYPE_ALPR;
        }
    }

    ALPRClassification oldClass = entry->classification;
    updateFlags(*entry, now);

    #if ALPR_DEBUG_LOG_ALL
    Serial.printf(
        "[ALPR-DBG] mac=%s vendor=%s flags=0x%04x conf=%u class=%s "
        "rssi=%d ch=%u probes=%u\n",
        entry->mac,
        alprVendorName(entry->vendor),
        entry->flags,
        entry->confidence,
        alprClassificationName(entry->classification),
        entry->lastRSSI,
        entry->lastChannel,
        entry->probeReqCount
    );
#endif

    alprDetectionsLogged++;
    alprLastDetectionTime = now;
    alprLastRSSI = alert.rssi;
    alprLastChannel = alert.channel;
    strncpy(alprLastMAC, macStr, 17);
    alprLastMAC[17] = '\0';

    if (entry->ssid[0])
    {
        strncpy(alprLastSSID, entry->ssid, 32);
        alprLastSSID[32] = '\0';
    }

    if (entry->serial[0])
    {
        strncpy(alprLastSerial, entry->serial, 23);
        alprLastSerial[23] = '\0';
    }

    alprLastConfidence = entry->confidence;
    alprLastClassification = entry->classification;
    alprLastVendor = entry->vendor;
    alprLastType = entry->deviceType;

    bool worthReporting = false;

    if (isNew && entry->classification >= ALPR_CLASS_POSSIBLE)
    {
        worthReporting = true;
    }
    else if (entry->classification > oldClass)
    {
        worthReporting = true;
    }
    else if (!isNew &&
             previousLastSeen > 0 &&
             (now - previousLastSeen) > ALPR_REDISCOVER_MS &&
             entry->classification >= ALPR_CLASS_PROBABLE)
    {
        worthReporting = true;
    }

    if (worthReporting && oldClass != entry->classification)
    {
        if (oldClass == ALPR_CLASS_DEFINITE)
            _countDefinite--;
        else if (oldClass == ALPR_CLASS_LIKELY)
            _countLikely--;
        else if (oldClass == ALPR_CLASS_PROBABLE)
            _countProbable--;
        else if (oldClass == ALPR_CLASS_POSSIBLE)
            _countPossible--;

        if (entry->classification == ALPR_CLASS_DEFINITE)
            _countDefinite++;
        else if (entry->classification == ALPR_CLASS_LIKELY)
            _countLikely++;
        else if (entry->classification == ALPR_CLASS_PROBABLE)
            _countProbable++;
        else if (entry->classification == ALPR_CLASS_POSSIBLE)
            _countPossible++;
    }

    if (worthReporting && entry->classification >= ALPR_CLASS_PROBABLE)
    {
        alprJustDetected = true;
    }

#if ALPR_DEBUG_LOG_ALL
    if ((isNew || (oldClass != entry->classification)) &&
        entry->classification >= ALPR_CLASS_POSSIBLE)
    {
        logDetection(*entry);
    }
    else if (entry->count % 20 == 0 &&
             entry->classification >= ALPR_CLASS_POSSIBLE)
    {
        logDetection(*entry);
    }
#else
    if (worthReporting && entry->classification >= ALPR_CLASS_PROBABLE)
    {
        logDetection(*entry);
    }
#endif
}

static void updateChannel()
{
    if (!alprDetectorActive)
        return;
    if (millis() - _lastHop < ALPR_CHANNEL_DWELL_MS)
        return;

    _channelIndex = (_channelIndex + 1) % alpr_channel_count;
    alprCurrentChannel = alpr_channels[_channelIndex];
    esp_wifi_set_channel(alprCurrentChannel, WIFI_SECOND_CHAN_NONE);
    _lastHop = millis();
}

static void drainAlertQueue()
{
    while (true)
    {
        portENTER_CRITICAL(&_queueMux);
        if (_alertTail == _alertHead)
        {
            portEXIT_CRITICAL(&_queueMux);
            break;
        }
        ALPRAlert e;
        memcpy(&e, (const void *)&_alertQueue[_alertTail], sizeof(ALPRAlert));
        _alertTail = (_alertTail + 1) % ALPR_ALERT_QUEUE_SIZE;
        portEXIT_CRITICAL(&_queueMux);

        processAlert(e);
    }
}

void alprDetectorBegin()
{
    if (alprDetectorActive)
        return;

    if (!gpsActive)
        Serial.println("[ALPR] Warning — GPS not active.");
    if (!sdActive)
        Serial.println("[ALPR] Warning — SD card not mounted.");

    allocSeen();

    alprDetectorActive = true;
    alprDetectionsLogged = 0;
    alprLastDetectionTime = 0;
    alprJustDetected = false;
    _sessionPart = 0;
    _channelIndex = 0;
    _countDefinite = 0;
    _countLikely = 0;
    _countProbable = 0;
    _countPossible = 0;
    _countBeacons = 0;
    _alertHead = 0;
    _alertTail = 0;
    alprLastVendor = VENDOR_UNKNOWN;
    alprLastType = TYPE_UNKNOWN;
    memset(alprLastMAC, 0, sizeof(alprLastMAC));
    memset(alprLastSSID, 0, sizeof(alprLastSSID));
    memset(alprLastSerial, 0, sizeof(alprLastSerial));
    clearSeen();

    LocalTime lt = gpsGetLocalTime();
    char ts[32];
    if (lt.valid)
    {
        snprintf(ts, sizeof(ts), "%04d%02d%02d_%02d%02d%02d",
                 lt.year, lt.month, lt.day, lt.hour, lt.minute, lt.second);
    }
    else
    {
        snprintf(ts, sizeof(ts), "%04d%02d%02d_%02d%02d%02d",
                 gpsData.year, gpsData.month, gpsData.day,
                 gpsData.hour, gpsData.minute, gpsData.second);
    }
    _sessionTimestamp = String(ts);

    Serial.println("[ALPR] Enabling promiscuous mode on existing WiFi stack");

    wifi_promiscuous_filter_t filt = {
        .filter_mask = WIFI_PROMIS_FILTER_MASK_MGMT};
    esp_wifi_set_promiscuous_filter(&filt);
    esp_wifi_set_promiscuous_rx_cb(&alprWifiSniffer);

    esp_err_t err = esp_wifi_set_promiscuous(true);
    if (err != ESP_OK)
    {
        Serial.printf("[ALPR] set_promiscuous failed: %d\n", err);
        freeSeen();
        alprDetectorActive = false;
        return;
    }

    alprCurrentChannel = alpr_channels[0];
    esp_wifi_set_channel(alprCurrentChannel, WIFI_SECOND_CHAN_NONE);
    _lastHop = millis();

#if ALPR_ENABLE_BLE
    startBLEScan();
#endif

    Serial.println("[ALPR] ==========================================");
    Serial.println("[ALPR]  ALPR / SURVEILLANCE DETECTOR ACTIVE");
    Serial.printf("[ALPR]  GPS: %s\n", gpsActive ? "YES" : "NO");
    Serial.printf("[ALPR]  SD:  %s\n", sdActive ? "YES" : "NO");
    Serial.printf("[ALPR]  Dedup capacity: %d MACs\n", ALPR_SEEN_CAPACITY);
    Serial.printf("[ALPR]  Vendor DB: %d OUIs, %d vendors\n",
                  (int)VENDOR_DB_COUNT, (int)(VENDOR_COUNT - 1));
    Serial.println("[ALPR] ==========================================");
#if ALPR_ENABLE_BLE
    Serial.println("[ALPR]  BLE scanning: ENABLED (coexists with WiFi)");
#endif
}

void alprDetectorEnd()
{
    if (!alprDetectorActive)
        return;

    alprDetectorActive = false;
    delay(50);

#if ALPR_ENABLE_BLE
    stopBLEScan();
#endif

    Serial.println("[ALPR] Disabling promiscuous mode");
    esp_wifi_set_promiscuous(false);
    esp_wifi_set_promiscuous_rx_cb(NULL);

    Serial.println("[ALPR] ==========================================");
    Serial.println("[ALPR]  ALPR DETECTOR STOPPED - Summary");
    Serial.printf("[ALPR]  Total detections: %lu\n", (unsigned long)alprDetectionsLogged);
    Serial.printf("[ALPR]  Unique MACs:      %d\n", _seenCount);
    Serial.printf("[ALPR]  Beacons seen:     %lu\n", (unsigned long)_countBeacons);
    Serial.printf("[ALPR]  DEFINITE:         %lu\n", (unsigned long)_countDefinite);
    Serial.printf("[ALPR]  LIKELY:           %lu\n", (unsigned long)_countLikely);
    Serial.printf("[ALPR]  PROBABLE:         %lu\n", (unsigned long)_countProbable);
    Serial.printf("[ALPR]  POSSIBLE:         %lu\n", (unsigned long)_countPossible);
    Serial.printf("[ALPR]  Session parts:    %lu\n", (unsigned long)(_sessionPart + 1));
    Serial.println("[ALPR] ==========================================");

    freeSeen();
}

void alprDetectorUpdate()
{
    if (!alprDetectorActive)
        return;
    updateChannel();
    drainAlertQueue();
}

bool isAlprDetectorActive() { return alprDetectorActive; }
uint32_t alprGetUniqueDeviceCount() { return _seenCount; }
uint32_t alprGetTotalDetections() { return alprDetectionsLogged; }
uint32_t alprGetDefiniteCount() { return _countDefinite; }
uint32_t alprGetLikelyCount() { return _countLikely; }
uint32_t alprGetProbableCount() { return _countProbable; }
uint32_t alprGetPossibleCount() { return _countPossible; }
uint32_t alprGetBeaconCount() { return _countBeacons; }

void alprDetectorPrintStatus()
{
    Serial.println();
    Serial.println("[ALPR] ==========================================");
    Serial.printf("[ALPR]  Active:      %s\n", alprDetectorActive ? "YES" : "NO");
    Serial.printf("[ALPR]  Current Ch:  %d\n", alprCurrentChannel);
    Serial.printf("[ALPR]  Detections:  %lu total\n", (unsigned long)alprDetectionsLogged);
    Serial.printf("[ALPR]  Unique MACs: %d / %d\n", _seenCount, ALPR_SEEN_CAPACITY);
    Serial.printf("[ALPR]  Beacons:     %lu\n", (unsigned long)_countBeacons);
    Serial.printf("[ALPR]  DEFINITE:    %lu\n", (unsigned long)_countDefinite);
    Serial.printf("[ALPR]  LIKELY:      %lu\n", (unsigned long)_countLikely);
    Serial.printf("[ALPR]  PROBABLE:    %lu\n", (unsigned long)_countProbable);
    Serial.printf("[ALPR]  POSSIBLE:    %lu\n", (unsigned long)_countPossible);
    if (alprLastMAC[0])
    {
        Serial.printf("[ALPR]  Last: %s [%s %s] '%s' (%s, %d%% conf)\n",
                      alprLastMAC,
                      alprVendorName(alprLastVendor),
                      alprDeviceTypeName(alprLastType),
                      alprLastSSID[0] ? alprLastSSID : "(no ssid)",
                      alprClassificationName(alprLastClassification),
                      (int)alprLastConfidence);
    }
    Serial.println("[ALPR] ==========================================");
    Serial.println();
}
#endif