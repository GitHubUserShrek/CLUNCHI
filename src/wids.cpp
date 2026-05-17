#include "wids.h"
#include "wifi_manager.h"
#include "oui_lookup.h"
#include "sd_manager.h"
#include "gps_manager.h"
#include <WiFi.h>
#include <SD.h>
#include "esp_wifi.h"

ThreatEvent widsLog[WIDS_LOG_SIZE];
int         widsLogCount   = 0;
int         widsLogHead    = 0;
uint32_t    widsTotalCount = 0;
uint32_t    widsLastTime   = 0;
bool        widsActive     = false;

static uint8_t  _ourBSSID[6];
static String   _ourSSID         = "";
static bool     _hasOurBSSID     = false;
static volatile bool _newAlert   = false;
static String   _sessionFilePath = "";
static const uint8_t BROADCAST_MAC[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

static uint32_t _lastBeaconFloodCheck = 0;
static uint32_t _lastCtsFloodCheck    = 0;
static uint16_t _ctsJamCount          = 0;
static uint32_t _lastDeauthTime       = 0; 

#define MAX_TRACKED_BSSIDS 40
static uint8_t  _trackedBssids[MAX_TRACKED_BSSIDS][6];
static uint16_t _trackedBssidCount = 0;

static int getLogIndex(int virtualIndex) {
    if (widsLogCount == 0) return -1;
    int idx = widsLogHead - widsLogCount + virtualIndex;
    if (idx < 0) idx += WIDS_LOG_SIZE;
    return idx % WIDS_LOG_SIZE;
}

static String getWidsTimeString() {
    LocalTime lt = gpsGetLocalTime();
    char buf[16];
    if (lt.valid) {
        snprintf(buf, sizeof(buf), "%02d:%02d:%02d", lt.hour, lt.minute, lt.second);
    } else {
        snprintf(buf, sizeof(buf), "%02d:%02d:%02d", gpsData.hour, gpsData.minute, gpsData.second);
    }
    return String(buf);
}

const char* widsAttackTypeString(AttackType type) {
    switch(type) {
        case ATTACK_DEAUTH:            return "DEAUTH";
        case ATTACK_DISASSOC:          return "DISASSOC";
        case ATTACK_EVIL_TWIN:         return "EVIL_TWIN";
        case ATTACK_CTS_JAMMING:       return "CTS_JAMMING";
        case ATTACK_HANDSHAKE_CAPTURE: return "HANDSHAKE_CAPTURE";
        case ATTACK_BEACON_FLOOD:      return "BEACON_FLOOD";
        default:                       return "UNKNOWN";
    }
}

static void initSdLogFile() {
    if (!sdActive) return;
    
    if (!SD.exists("/wids")) {
        SD.mkdir("/wids");
    }

    char dateBuf[16]; LocalTime lt = gpsGetLocalTime();
    if (lt.valid) snprintf(dateBuf, sizeof(dateBuf), "%04d%02d%02d", lt.year, lt.month, lt.day);
    else if (gpsData.year > 2000) snprintf(dateBuf, sizeof(dateBuf), "%04d%02d%02d", gpsData.year, gpsData.month, gpsData.day);
    else strcpy(dateBuf, "NODATE");

    _sessionFilePath = "/wids/WIDS_" + String(dateBuf) + ".csv";
    
    bool isNew = !SD.exists(_sessionFilePath.c_str());
    File f = SD.open(_sessionFilePath.c_str(), FILE_APPEND);
    if (f) {
        if (isNew) {
            f.println("time,lat,lon,attack_type,source_mac,source_vendor,dest_mac,dest_vendor,bssid,param,rssi,burst_count,flags,notes");
        }
        f.close();
        Serial.printf("[WIDS] Attack log ready on SD: %s\n", _sessionFilePath.c_str());
    } else {
        Serial.printf("[WIDS] Error: Could not create SD file %s\n", _sessionFilePath.c_str());
        _sessionFilePath = ""; 
    }
}

static void logThreatEvent(AttackType type, uint8_t* src, uint8_t* dst, uint8_t* bssid, uint16_t param, int rssi, const char* notes) {
    uint32_t now = millis();
    widsTotalCount++;
    widsLastTime = now;

    if (widsLogCount > 0) {
        int lastIdx = (widsLogHead - 1 + WIDS_LOG_SIZE) % WIDS_LOG_SIZE;
        ThreatEvent& last = widsLog[lastIdx];
        
        if (last.type == type && (now - last.timestamp < 3000) && 
            memcmp(last.source, src, 6) == 0 && memcmp(last.destination, dst, 6) == 0) {
            last.burstCount++;
            last.timestamp = now;
            last.rssi = (last.rssi + rssi) / 2;
            _newAlert = true;
            return;
        }
    }

    ThreatEvent& e = widsLog[widsLogHead];
    e.type = type;
    memcpy(e.destination, dst,   6);
    memcpy(e.source,      src,   6);
    memcpy(e.bssid,       bssid, 6);
    e.param          = param;
    e.rssi           = rssi;
    e.timestamp      = now;
    e.firstTimestamp = now;
    e.burstCount     = 1;
    e.isBroadcast    = (memcmp(dst, BROADCAST_MAC, 6) == 0);
    e.isTargeted     = _hasOurBSSID && (memcmp(bssid, _ourBSSID, 6) == 0 || memcmp(dst, _ourBSSID, 6) == 0);
    e.latitude       = gpsData.latitude;
    e.longitude      = gpsData.longitude;
    e.gpsValid       = gpsData.valid;
    
    if (notes) strncpy(e.customNotes, notes, sizeof(e.customNotes) - 1);
    else memset(e.customNotes, 0, sizeof(e.customNotes));

    widsLogHead = (widsLogHead + 1) % WIDS_LOG_SIZE;
    if (widsLogCount < WIDS_LOG_SIZE) widsLogCount++;

    _newAlert = true;
}

static void _widsSniffer(void* buf, wifi_promiscuous_pkt_type_t type) {
    wifi_promiscuous_pkt_t* pkt = (wifi_promiscuous_pkt_t*)buf;
    uint8_t* payload = pkt->payload;
    uint16_t len = pkt->rx_ctrl.sig_len;
    int rssi = pkt->rx_ctrl.rssi;
    uint32_t now = millis();

    if (len < 14) return; 

    uint8_t frameType    = (payload[0] >> 2) & 0x03;
    uint8_t frameSubtype = (payload[0] >> 4) & 0x0F;

    if (frameType == 0) { 
        if (len < 24) return; 

        uint8_t* destMac   = &payload[4];
        uint8_t* sourceMac = &payload[10];
        uint8_t* bssidMac  = &payload[16];

        if (frameSubtype == 12 || frameSubtype == 10) {
            uint16_t reason = payload[24] | (payload[25] << 8);
            AttackType aType = (frameSubtype == 12) ? ATTACK_DEAUTH : ATTACK_DISASSOC;
            _lastDeauthTime = now; 
            logThreatEvent(aType, sourceMac, destMac, bssidMac, reason, rssi, NULL);
            return;
        }

        if (frameSubtype == 8 && _hasOurBSSID && len > 36) {
            if (now - _lastBeaconFloodCheck > 1000) {
                if (_trackedBssidCount >= 25) {
                    logThreatEvent(ATTACK_BEACON_FLOOD, (uint8_t*)BROADCAST_MAC, (uint8_t*)BROADCAST_MAC, (uint8_t*)BROADCAST_MAC, _trackedBssidCount, rssi, "SSID_Flood");
                }
                _trackedBssidCount = 0;
                _lastBeaconFloodCheck = now;
            } else {
                bool found = false;
                for (int i = 0; i < _trackedBssidCount; i++) {
                    if (memcmp(_trackedBssids[i], bssidMac, 6) == 0) { found = true; break; }
                }
                if (!found && _trackedBssidCount < MAX_TRACKED_BSSIDS) {
                    memcpy(_trackedBssids[_trackedBssidCount], bssidMac, 6);
                    _trackedBssidCount++;
                }
            }

            uint8_t tagNumber = payload[36]; uint8_t tagLen = payload[37];
            if (tagNumber == 0 && tagLen > 0 && tagLen <= 32 && (38 + tagLen < len)) {
                char ssidBuf[33]; memcpy(ssidBuf, &payload[38], tagLen); ssidBuf[tagLen] = '\0';
                if (_ourSSID.equals(ssidBuf) && memcmp(bssidMac, _ourBSSID, 6) != 0) {
                    logThreatEvent(ATTACK_EVIL_TWIN, sourceMac, destMac, bssidMac, 0, rssi, ssidBuf);
                }
            }
            return;
        }
    }

    if (frameType == 1 && frameSubtype == 12) { 
        uint16_t duration = payload[1] | (payload[2] << 8);
        if (duration > 30000) {
            if (now - _lastCtsFloodCheck > 1000) {
                if (_ctsJamCount > 20) {
                    uint8_t* targetMac = &payload[4];
                    logThreatEvent(ATTACK_CTS_JAMMING, (uint8_t*)BROADCAST_MAC, targetMac, targetMac, duration, rssi, "NAV_Jamming");
                }
                _ctsJamCount = 0; _lastCtsFloodCheck = now;
            } else { _ctsJamCount++; }
        }
        return;
    }

    if (frameType == 2 && (frameSubtype == 0 || frameSubtype == 8)) { 
        if (len < 24) return; 

        uint16_t hdrLen = (frameSubtype == 8) ? 26 : 24;
        if (len < hdrLen + 8) return;
        uint8_t* data = &payload[hdrLen];
        
        if (data[0] == 0xAA && data[1] == 0xAA && data[6] == 0x88 && data[7] == 0x8E) {
            if (_lastDeauthTime > 0 && (now - _lastDeauthTime < 5000)) {
                uint8_t* destMac = &payload[4]; uint8_t* sourceMac = &payload[10]; uint8_t* bssidMac = &payload[16];
                logThreatEvent(ATTACK_HANDSHAKE_CAPTURE, sourceMac, destMac, bssidMac, 0, rssi, "Forced_WPA_Capture");
            }
        }
        return;
    }
}

void widsBegin() {
    if (widsActive) return;
    if (!wifiConnected()) { Serial.println("[WIDS] Cannot start — not connected to WiFi."); return; }

    _ourSSID = WiFi.SSID();
    uint8_t* bssid = WiFi.BSSID();
    if (bssid) { memcpy(_ourBSSID, bssid, 6); _hasOurBSSID = true; } else { _hasOurBSSID = false; }

    // ---> FIX: Removed log wiping! Only reset transient sniffer state. <---
    _newAlert          = false;
    _trackedBssidCount = 0; 
    _lastDeauthTime    = 0;

    initSdLogFile();

    wifi_promiscuous_filter_t filter = { .filter_mask = WIFI_PROMIS_FILTER_MASK_MGMT | WIFI_PROMIS_FILTER_MASK_CTRL | WIFI_PROMIS_FILTER_MASK_DATA };
    esp_wifi_set_promiscuous_filter(&filter);
    esp_wifi_set_promiscuous_rx_cb(_widsSniffer);
    esp_wifi_set_promiscuous(true);

    widsActive = true;
    Serial.printf("[WIDS] CLUNCHI Tactical IDS ACTIVE on Ch:%d | Protecting SSID: %s\n", WiFi.channel(), _ourSSID.c_str());
}

void widsEnd() {
    if (!widsActive) return;
    esp_wifi_set_promiscuous(false); widsActive = false;
    Serial.printf("[WIDS] Detector stopped. Total anomaly frames: %lu\n", (unsigned long)widsTotalCount);
}

bool isWidsActive() { return widsActive; }

void widsUpdate() {
    if (!widsActive) return;
    if (!wifiConnected()) { widsEnd(); return; }

    if (_newAlert) {
        _newAlert = false;
        if (widsLogCount > 0) {
            int lastIdx = (widsLogHead - 1 + WIDS_LOG_SIZE) % WIDS_LOG_SIZE;
            ThreatEvent& e = widsLog[lastIdx];
            
            if (e.burstCount == 1 || e.burstCount % 25 == 0) {
                String srcMac = widsMacToString(e.source); String dstMac = widsMacToString(e.destination);
                String srcVendor = lookupOUI(srcMac); String dstVendor = lookupOUI(dstMac);
                if (srcVendor == "") srcVendor = "Unknown"; if (dstVendor == "") dstVendor = "Unknown";

                const char* typeStr = widsAttackTypeString(e.type);
                String flags = e.isBroadcast ? "BROADCAST" : (e.isTargeted ? "TARGETED" : "GENERAL");

                Serial.printf("[WIDS ALERT] %s (x%d) | Src: %s (%s) → Dst: %s (%s) [%s] | %d dBm | Notes: %s\n",
                              typeStr, e.burstCount, srcMac.c_str(), srcVendor.c_str(), dstMac.c_str(), dstVendor.c_str(), flags.c_str(), e.rssi, e.customNotes);

                if (sdActive && _sessionFilePath == "") {
                    initSdLogFile();
                }

                if (sdActive && _sessionFilePath != "") {
                    File f = SD.open(_sessionFilePath.c_str(), FILE_APPEND);
                    if (f) {
                        char line[512]; 
                        snprintf(line, sizeof(line), "%s,%.6f,%.6f,%s,%s,%s,%s,%s,%s,%d,%d,%d,%s,%s",
                                 getWidsTimeString().c_str(), e.gpsValid ? e.latitude : 0.0, e.gpsValid ? e.longitude : 0.0,
                                 typeStr, srcMac.c_str(), srcVendor.c_str(), dstMac.c_str(), dstVendor.c_str(),
                                 widsMacToString(e.bssid).c_str(), e.param, e.rssi, e.burstCount, flags.c_str(), e.customNotes);
                        f.println(line); 
                        f.flush(); 
                        f.close();
                    } else {
                        Serial.printf("[WIDS] SD Write Error on %s\n", _sessionFilePath.c_str());
                    }
                }
            }
        }
    }

    static uint32_t lastStatusPrint = 0; uint32_t now = millis();
    if (now - lastStatusPrint > 60000) {
        lastStatusPrint = now;
        if (widsTotalCount > 0) Serial.printf("[WIDS Status] %lu attack frames logged. Last incident %lus ago.\n", (unsigned long)widsTotalCount, (unsigned long)((now - widsLastTime) / 1000));
    }
}

bool widsHasRecentAlert(uint32_t withinMs) {
    if (widsTotalCount == 0) return false; return (millis() - widsLastTime) <= withinMs;
}

uint32_t widsRecentCount(uint32_t withinMs) {
    uint32_t count = 0; uint32_t cutoff = millis() - withinMs;
    for (int i = 0; i < widsLogCount; i++) {
        int idx = getLogIndex(i); if (widsLog[idx].timestamp >= cutoff) count += widsLog[idx].burstCount;
    }
    return count;
}

uint8_t widsThreatScore() {
    if (widsTotalCount == 0) return 0; 
    uint8_t score = 0; 
    uint32_t now = millis();
    uint32_t msSinceLast = now - widsLastTime;

    if (msSinceLast < 2000) score += 40; else if (msSinceLast < 10000) score += 20; else if (msSinceLast < 60000) score += 10;
    uint32_t recent = widsRecentCount(10000);
    if (recent >= 50) score += 50; else if (recent >= 15) score += 30; else if (recent >= 5) score += 15;

    for (int i = 0; i < widsLogCount; i++) {
        int idx = getLogIndex(i);
        if (now - widsLog[idx].timestamp < 30000) { 
            if (widsLog[idx].isTargeted) score += 30;
            if (widsLog[idx].type == ATTACK_EVIL_TWIN) score += 40;
            if (widsLog[idx].type == ATTACK_HANDSHAKE_CAPTURE) score += 35;
            if (widsLog[idx].type == ATTACK_BEACON_FLOOD) score += 30; 
            if (widsLog[idx].type == ATTACK_CTS_JAMMING) score += 30;  
            if (widsLog[idx].type == ATTACK_DEAUTH || widsLog[idx].type == ATTACK_DISASSOC) score += 25;
        }
    }
    return (score > 100) ? 100 : score;
}

bool widsUnderAttack() { return (widsThreatScore() >= 60); }

uint8_t widsUniqueSourceCount() {
    uint8_t unique[WIDS_LOG_SIZE][6]; uint8_t count = 0;
    for (int i = 0; i < widsLogCount; i++) {
        int idx = getLogIndex(i); bool found = false;
        for (int j = 0; j < count; j++) { if (memcmp(unique[j], widsLog[idx].source, 6) == 0) { found = true; break; } }
        if (!found && count < WIDS_LOG_SIZE) { memcpy(unique[count], widsLog[idx].source, 6); count++; }
    }
    return count;
}

int widsMostActiveSourceIndex() {
    if (widsLogCount == 0) return -1;
    uint8_t macs[WIDS_LOG_SIZE][6]; uint32_t counts[WIDS_LOG_SIZE] = {0}; uint8_t macCount = 0;
    for (int i = 0; i < widsLogCount; i++) {
        int idx = getLogIndex(i); bool found = false;
        for (int j = 0; j < macCount; j++) { if (memcmp(macs[j], widsLog[idx].source, 6) == 0) { counts[j] += widsLog[idx].burstCount; found = true; break; } }
        if (!found && macCount < WIDS_LOG_SIZE) { memcpy(macs[macCount], widsLog[idx].source, 6); counts[macCount] = widsLog[idx].burstCount; macCount++; }
    }
    uint32_t maxCount = 0; int maxMacIdx = 0;
    for (int i = 0; i < macCount; i++) { if (counts[i] > maxCount) { maxCount = counts[i]; maxMacIdx = i; } }
    for (int i = 0; i < widsLogCount; i++) { int idx = getLogIndex(i); if (memcmp(widsLog[idx].source, macs[maxMacIdx], 6) == 0) return idx; }
    return getLogIndex(0);
}

void widsPrintInfo() {
    Serial.println("\n[CLUNCHI Tactical WIDS] ===============================");
    Serial.printf(" Status:         %s\n", widsActive ? "ACTIVE" : "INACTIVE");
    if (widsActive || widsTotalCount > 0) {
        Serial.printf(" Monitoring Ch:  %d\n", WiFi.channel());
        Serial.printf(" Protected AP:   %s (%s)\n", _ourSSID.c_str(), _hasOurBSSID ? widsMacToString(_ourBSSID).c_str() : "None");
        Serial.printf(" SD Logging:     %s\n", sdActive ? (_sessionFilePath != "" ? _sessionFilePath.c_str() : "PENDING") : "DISABLED");
        Serial.printf(" Total Incidents:%lu frames\n", (unsigned long)widsTotalCount);
        if (widsTotalCount > 0) Serial.printf(" Last Attack:    %lus ago\n", (unsigned long)((millis() - widsLastTime) / 1000));
        Serial.printf(" Threat Score:   %d/100 (%s)\n", widsThreatScore(), widsUnderAttack() ? "ATTACK DETECTED!" : "Normal");
        Serial.printf(" Unique Attackers: %d\n", widsUniqueSourceCount());
    }
    Serial.println("=======================================================\n");
}

void widsPrintLog() {
    if (widsLogCount == 0) { Serial.println("[CLUNCHI] Attack log is empty."); return; }
    Serial.printf("\n--- CLUNCHI Incident Log (Last %d Events) ---\n", widsLogCount);
    for (int i = 0; i < widsLogCount; i++) {
        int idx = getLogIndex(i); ThreatEvent& e = widsLog[idx];
        uint32_t ago = (millis() - e.timestamp) / 1000; uint32_t duration = (e.timestamp - e.firstTimestamp) / 1000;
        String srcMac = widsMacToString(e.source); String dstMac = widsMacToString(e.destination);
        String srcVendor = lookupOUI(srcMac); String dstVendor = lookupOUI(dstMac);
        if (srcVendor == "") srcVendor = "Unknown"; if (dstVendor == "") dstVendor = "Unknown";

        String flags = "";
        if (e.isBroadcast) flags += "[BROADCAST] "; if (e.isTargeted) flags += "[TARGETED] ";
        if (e.gpsValid) flags += String("[GPS: ") + String(e.latitude, 4) + "," + String(e.longitude, 4) + "] ";

        Serial.printf("[%2d] %-17s | x%-4d pkts | %3lus ago | %s (%s) → %s (%s)\n", 
                      i + 1, widsAttackTypeString(e.type), e.burstCount, ago, srcMac.c_str(), srcVendor.c_str(), dstMac.c_str(), dstVendor.c_str());
        Serial.printf("     └─ Param: %d (%s) | RSSI: %ddBm | Duration: %lus | Notes: %s %s\n",
                      e.param, widsReasonString(e.param), e.rssi, duration, e.customNotes, flags.c_str());
    }
    Serial.println("-------------------------------------------\n");
}

String widsMacToString(const uint8_t* mac) {
    char buf[18]; snprintf(buf, sizeof(buf), "%02x:%02x:%02x:%02x:%02x:%02x", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]); return String(buf);
}

const char* widsReasonString(uint16_t reason) {
    switch (reason) {
        case 1:  return "Unspecified"; case 2:  return "Prev auth invalid"; case 3:  return "Station leaving"; case 4:  return "Inactivity timeout";
        case 6:  return "Class 2 non-auth"; case 7:  return "Class 3 non-assoc"; case 8:  return "Station left BSS"; case 15: return "TSF invalid";
        default: return (reason > 500) ? "Microseconds (NAV)" : "Unknown/Custom";
    }
}