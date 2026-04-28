#include "wardriving.h"
#include "gps_manager.h"
#include "sd_manager.h"
#include "wifi_manager.h"
#include <WiFi.h>
#include <SD.h>

#define WD_SCAN_INTERVAL_MS   10000
#define WD_MAX_NETWORKS       50

bool     wardrivingActive         = false;
uint32_t wardrivingNetworksLogged = 0;
static String _sessionTimestamp = "";
static uint32_t _sessionPart = 0;

static uint32_t _lastScanTime  = 0;
static bool     _scanRequested = false;
static bool     _scanning      = false;

#define WD_SEEN_CAPACITY 4096
#define WD_SEEN_EMPTY    0
#define WD_SEEN_USED     1

static struct {
    uint8_t  state;
    uint32_t hash;
    String   bssid;
} _seenTable[WD_SEEN_CAPACITY];

static int _seenCount = 0;

static uint32_t fnvHash(const String& s) {
    uint32_t h = 2166136261u;
    for (int i = 0; i < (int)s.length(); i++) {
        h ^= (uint8_t)s[i];
        h *= 16777619u;
    }
    return h;
}

static bool alreadySeen(const String& bssid) {
    if (_seenCount >= WD_SEEN_CAPACITY) return false;
    uint32_t h = fnvHash(bssid);
    uint32_t idx = h % WD_SEEN_CAPACITY;
    for (int probe = 0; probe < WD_SEEN_CAPACITY; probe++) {
        uint32_t i = (idx + probe) % WD_SEEN_CAPACITY;
        if (_seenTable[i].state == WD_SEEN_EMPTY) return false;
        if (_seenTable[i].hash == h && _seenTable[i].bssid == bssid) return true;
    }
    return false;
}

static void markSeen(const String& bssid) {
    if (_seenCount >= WD_SEEN_CAPACITY - 1) return;
    uint32_t h = fnvHash(bssid);
    uint32_t idx = h % WD_SEEN_CAPACITY;
    for (int probe = 0; probe < WD_SEEN_CAPACITY; probe++) {
        uint32_t i = (idx + probe) % WD_SEEN_CAPACITY;
        if (_seenTable[i].state == WD_SEEN_EMPTY) {
            _seenTable[i].state = WD_SEEN_USED;
            _seenTable[i].hash  = h;
            _seenTable[i].bssid = bssid;
            _seenCount++;
            return;
        }
        if (_seenTable[i].hash == h && _seenTable[i].bssid == bssid) return;
    }
}

static void clearSeen() {
    for (int i = 0; i < WD_SEEN_CAPACITY; i++) {
        _seenTable[i].state = WD_SEEN_EMPTY;
        _seenTable[i].hash  = 0;
        _seenTable[i].bssid = "";
    }
    _seenCount = 0;
}

static void checkAndRotateSession() {
    if (_seenCount < WD_SEEN_CAPACITY - 100) return;

    _sessionPart++;
    clearSeen();

    Serial.println("[Wardriving] ──────────────────────────────────────");
    Serial.printf("[Wardriving]  Dedup table full — auto-rotating to part %lu\n",
                  (unsigned long)_sessionPart);
    Serial.printf("[Wardriving]  Total logged so far: %lu\n",
                  (unsigned long)wardrivingNetworksLogged);
    Serial.println("[Wardriving] ──────────────────────────────────────");
}

static String getDateString() {
    if (gpsData.year > 0) {
        char buf[16];
        snprintf(buf, sizeof(buf), "%04d%02d%02d",
                 gpsData.year, gpsData.month, gpsData.day);
        return String(buf);
    }
    return "NODATE";
}

static String getTimeString() {
    char buf[16];
    snprintf(buf, sizeof(buf), "%02d:%02d:%02d",
             gpsData.hour, gpsData.minute, gpsData.second);
    return String(buf);
}

static File openNetworkLog() {
    if (!sdActive) return File();

    if (!SD.exists("/wardriving")) {
        SD.mkdir("/wardriving");
    }

    String path;
    if (_sessionPart == 0) {
        path = "/wardriving/WD_" + _sessionTimestamp + ".csv";
    } else {
        char partBuf[8];
        snprintf(partBuf, sizeof(partBuf), "_P%lu", (unsigned long)_sessionPart);
        path = "/wardriving/WD_" + _sessionTimestamp + String(partBuf) + ".csv";
    }

    bool isNew = !SD.exists(path.c_str());
    File f = SD.open(path.c_str(), FILE_APPEND);

    if (f && isNew) {
        f.println("time,lat,lon,alt,speed,sats,hdop,ssid,bssid,rssi,channel,encryption");
        Serial.printf("[Wardriving] Created new log: %s\n", path.c_str());
    }

    return f;
}

static void logScanResults() {
    int n = WiFi.scanComplete();
    if (n <= 0) return;

    checkAndRotateSession();

    File f = openNetworkLog();
    bool hasFile = (f == true);

    int logged = 0;
    int limit = min(n, WD_MAX_NETWORKS);
    for (int i = 0; i < limit; i++) {
        String bssid = WiFi.BSSIDstr(i);

        if (alreadySeen(bssid)) continue;

        String ssid  = WiFi.SSID(i);
        int32_t rssi = WiFi.RSSI(i);
        uint8_t ch   = WiFi.channel(i);
        String enc;

        switch (WiFi.encryptionType(i)) {
            case WIFI_AUTH_OPEN:            enc = "OPEN"; break;
            case WIFI_AUTH_WEP:             enc = "WEP"; break;
            case WIFI_AUTH_WPA_PSK:         enc = "WPA"; break;
            case WIFI_AUTH_WPA2_PSK:        enc = "WPA2"; break;
            case WIFI_AUTH_WPA_WPA2_PSK:    enc = "WPA/WPA2"; break;
            case WIFI_AUTH_WPA3_PSK:        enc = "WPA3"; break;
            case WIFI_AUTH_WPA2_WPA3_PSK:   enc = "WPA2/WPA3"; break;
            case WIFI_AUTH_WPA2_ENTERPRISE: enc = "WPA2-ENT"; break;
            default:                        enc = "OTHER"; break;
        }

        ssid.replace(",", " ");

        char line[256];
        snprintf(line, sizeof(line),
                 "%s,%.6f,%.6f,%.1f,%.1f,%d,%d,%s,%s,%d,%d,%s",
                 getTimeString().c_str(),
                 gpsData.latitude,
                 gpsData.longitude,
                 gpsData.altitude,
                 gpsData.speed,
                 gpsData.satellites,
                 gpsData.hdop,
                 ssid.c_str(),
                 bssid.c_str(),
                 (int)rssi,
                 (int)ch,
                 enc.c_str());

        if (hasFile) {
            f.println(line);
        }

        Serial.printf("[WD] %s\n", line);

        markSeen(bssid);
        logged++;
        wardrivingNetworksLogged++;
    }

    if (hasFile) {
        f.close();
    }
    WiFi.scanDelete();
    _scanning = false;

    if (logged > 0) {
        Serial.printf("[Wardriving] Logged %d new networks (total: %lu, seen: %d/%d, part: %lu)%s\n",
                      logged, (unsigned long)wardrivingNetworksLogged,
                      _seenCount, WD_SEEN_CAPACITY,
                      (unsigned long)_sessionPart,
                      hasFile ? "" : " [SERIAL ONLY — no SD]");
    }
}

static void startScan() {
    if (_scanning) return;
    WiFi.scanNetworks(true, false, true);
    _scanning = true;
    _lastScanTime = millis();
}

void wardrivingBegin() {
    if (wardrivingActive) return;

    if (!gpsActive) {
        Serial.println("[Wardriving] Warning — GPS not active.");
    }

    if (!sdActive) {
        Serial.println("[Wardriving] Warning — SD card not mounted. Logging to serial only.");
    }

    wardrivingActive         = true;
    wardrivingNetworksLogged = 0;
    _lastScanTime            = 0;
    _scanRequested           = false;
    _scanning                = false;
    _sessionPart             = 0;
    clearSeen();

    char ts[32];
    snprintf(ts, sizeof(ts), "%04d%02d%02d_%02d%02d%02d",
             gpsData.year, gpsData.month, gpsData.day,
             gpsData.hour, gpsData.minute, gpsData.second);
    _sessionTimestamp = String(ts);

    Serial.println("[Wardriving] ==========================================");
    Serial.println("[Wardriving]  WARDRIVING MODE ACTIVE");
    Serial.printf("[Wardriving]  GPS: %s\n", gpsActive ? "YES" : "NO");
    Serial.printf("[Wardriving]  SD:  %s\n", sdActive ? "YES (logging to file)" : "NO (serial only)");
    Serial.printf("[Wardriving]  Dedup capacity: %d BSSIDs (auto-rotate)\n", WD_SEEN_CAPACITY);
    Serial.printf("[Wardriving]  Auto scan every %d seconds\n",
                  WD_SCAN_INTERVAL_MS / 1000);
    Serial.println("[Wardriving] ==========================================");

    startScan();
}

void wardrivingEnd() {
    if (!wardrivingActive) return;
    wardrivingActive = false;

    WiFi.scanDelete();
    _scanning = false;

    Serial.println("[Wardriving] ==========================================");
    Serial.println("[Wardriving]  WARDRIVING MODE STOPPED");
    Serial.printf("[Wardriving]  Networks logged: %lu\n",
                  (unsigned long)wardrivingNetworksLogged);
    Serial.printf("[Wardriving]  Session parts:   %lu\n",
                  (unsigned long)(_sessionPart + 1));
    Serial.printf("[Wardriving]  Unique BSSIDs seen (last part): %d\n", _seenCount);
    Serial.println("[Wardriving] ==========================================");
}

void wardrivingForceScan() {
    if (!wardrivingActive) return;
    _scanRequested = true;
    Serial.println("[Wardriving] Manual scan requested!");
}

void wardrivingUpdate() {
    if (!wardrivingActive) return;

    uint32_t now = millis();

    if (_scanning) {
        int result = WiFi.scanComplete();
        if (result >= 0) {
            logScanResults();
        } else if (result == WIFI_SCAN_FAILED) {
            _scanning = false;
            Serial.println("[Wardriving] Scan failed.");
        }
    }

    if (!_scanning && (now - _lastScanTime >= WD_SCAN_INTERVAL_MS)) {
        startScan();
    }

    if (!_scanning && _scanRequested) {
        _scanRequested = false;
        startScan();
    }
}

bool isWardrivingActive() {
    return wardrivingActive;
}

void wardrivingPrintStatus() {
    Serial.println();
    Serial.println("[Wardriving] ==========================================");
    Serial.printf("[Wardriving]  Active:      %s\n", wardrivingActive ? "YES" : "NO");
    Serial.printf("[Wardriving]  GPS Lock:    %s\n", gpsData.valid ? "YES" : "NO");
    Serial.printf("[Wardriving]  SD Card:     %s\n", sdActive ? "MOUNTED" : "NOT MOUNTED (serial only)");
    Serial.printf("[Wardriving]  Networks:    %lu logged\n",
                  (unsigned long)wardrivingNetworksLogged);
    Serial.printf("[Wardriving]  Seen BSSIDs: %d / %d\n", _seenCount, WD_SEEN_CAPACITY);
    Serial.printf("[Wardriving]  Session part: %lu\n", (unsigned long)(_sessionPart + 1));
    Serial.println("[Wardriving] ==========================================");
    Serial.println();
}