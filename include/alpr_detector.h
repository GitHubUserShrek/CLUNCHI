#pragma once
#include <Arduino.h>

#if defined(BOARD_XIAO_C5)

#define ALPR_ENABLE_BLE 1

#define ALPR_SEEN_CAPACITY       2048
#define ALPR_CHANNEL_DWELL_MS    250
#define ALPR_ALERT_COOLDOWN_MS   5000
#define ALPR_REDISCOVER_MS       30000
#define ALPR_RSSI_MIN            -95

#ifndef ALPR_DEBUG_LOG_ALL
#define ALPR_DEBUG_LOG_ALL 0
#endif

#define ALPR_CONFIDENCE_DEFINITE  90
#define ALPR_CONFIDENCE_LIKELY    70
#define ALPR_CONFIDENCE_PROBABLE  50
#define ALPR_CONFIDENCE_POSSIBLE  30


enum ALPRVendor : uint8_t {
    VENDOR_UNKNOWN = 0,
    VENDOR_FLOCK,
    VENDOR_MOTOROLA,
    VENDOR_AXON,
    VENDOR_GENETEC,
    VENDOR_REKOR,
    VENDOR_HIKVISION,
    VENDOR_DAHUA,
    VENDOR_AXIS,
    VENDOR_BOSCH,
    VENDOR_NEOLOGY,
    VENDOR_PERCEPTICS,
    VENDOR_ELSAG,
    VENDOR_RING,
    VENDOR_NEST,
    VENDOR_WYZE,
    VENDOR_REOLINK,
    VENDOR_ARLO,
    VENDOR_UBIQUITI,
    VENDOR_COUNT
};

enum ALPRDeviceType : uint8_t {
    TYPE_UNKNOWN = 0,
    TYPE_ALPR,
    TYPE_SECURITY_CAM,
    TYPE_DOORBELL,
    TYPE_BODY_CAM,
    TYPE_TRAFFIC_CAM,
    TYPE_COUNT
};

enum ALPRDetectionFlags : uint16_t {
    ALPR_FLAG_NONE            = 0,
    ALPR_FLAG_OUI_MATCH       = 1 << 0,
    ALPR_FLAG_WILDCARD_PROBE  = 1 << 1,
    ALPR_FLAG_LITEON_IE       = 1 << 2,
    ALPR_FLAG_PACK_SIGNATURE  = 1 << 3,
    ALPR_FLAG_BURST_PATTERN   = 1 << 4,
    ALPR_FLAG_STATIONARY_RSSI = 1 << 5,
    ALPR_FLAG_SUSTAINED_PROBE = 1 << 6,
    ALPR_FLAG_CHANNEL_HOPPING = 1 << 7,
    ALPR_FLAG_SSID_MATCH      = 1 << 8,
    ALPR_FLAG_BEACON_MATCH    = 1 << 9,
    ALPR_FLAG_PROBE_RESP      = 1 << 10,
    ALPR_FLAG_BLE_XUNTONG     = 1 << 11,
    ALPR_FLAG_BLE_PENGUIN     = 1 << 12,
    ALPR_FLAG_BLE_FS_BATTERY  = 1 << 13,
    ALPR_FLAG_BLE_10DIGIT     = 1 << 14,
    ALPR_FLAG_BLE_NAME_MATCH  = 1 << 15,
};

enum ALPRClassification : uint8_t {
    ALPR_CLASS_IGNORE = 0,
    ALPR_CLASS_POSSIBLE,
    ALPR_CLASS_PROBABLE,
    ALPR_CLASS_LIKELY,
    ALPR_CLASS_DEFINITE
};

extern bool alprDetectorActive;
extern uint32_t alprDetectionsLogged;
extern uint32_t alprLastDetectionTime;
extern int8_t alprLastRSSI;
extern uint8_t alprLastChannel;
extern char alprLastMAC[18];
extern char alprLastSSID[33];
extern char alprLastSerial[24];
extern uint8_t alprCurrentChannel;
extern bool alprJustDetected;
extern uint8_t alprLastConfidence;
extern ALPRClassification alprLastClassification;
extern ALPRVendor alprLastVendor;
extern ALPRDeviceType alprLastType;

void alprDetectorBegin();
void alprDetectorEnd();
void alprDetectorUpdate();
void alprDetectorPrintStatus();
bool isAlprDetectorActive();

uint32_t alprGetUniqueDeviceCount();
uint32_t alprGetTotalDetections();
uint32_t alprGetDefiniteCount();
uint32_t alprGetLikelyCount();
uint32_t alprGetProbableCount();
uint32_t alprGetPossibleCount();
uint32_t alprGetBeaconCount();

const char* alprClassificationName(ALPRClassification c);
const char* alprVendorName(ALPRVendor v);
const char* alprDeviceTypeName(ALPRDeviceType t);
ALPRVendor alprIdentifyVendor(const uint8_t* mac);
ALPRDeviceType alprGuessDeviceType(ALPRVendor v, const char* ssid);
bool isALPRSSID(const char* ssid);

#else

inline bool isAlprDetectorActive() { return false; }
inline void alprDetectorBegin() {}
inline void alprDetectorEnd() {}
inline void alprDetectorUpdate() {}
inline void alprDetectorPrintStatus() {}

static const bool alprDetectorActive = false;
static const uint32_t alprDetectionsLogged = 0;
static const bool alprJustDetected = false;

#endif