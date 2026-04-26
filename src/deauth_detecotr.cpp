#include "deauth_detector.h"
#include "wifi_manager.h"
#include <WiFi.h>
#include "esp_wifi.h"

DeauthEvent deauthLog[DEAUTH_LOG_SIZE];
int         deauthLogCount       = 0;
uint32_t    deauthTotalCount     = 0;
uint32_t    deauthLastTime       = 0;
bool        deauthDetectorActive = false;

static uint8_t  _ourBSSID[6];
static bool     _hasOurBSSID    = false;
static volatile uint32_t _lastLogTime = 0;
static const uint32_t    LOG_THROTTLE_MS = 500;
static volatile bool     _newAlert = false;


// ── Sniffer callback ─────────────────────────────────
static void _deauthSniffer(void* buf, wifi_promiscuous_pkt_type_t type) {
    if (type != WIFI_PKT_MGMT) return;

    static uint8_t _mgmtDrop = 0;
    if (++_mgmtDrop % 2 != 0) return;

    wifi_promiscuous_pkt_t* pkt = (wifi_promiscuous_pkt_t*)buf;
    uint8_t* payload = pkt->payload;
    uint16_t len = pkt->rx_ctrl.sig_len;

    if (len < 26) return;

    uint8_t frameType    = (payload[0] >> 2) & 0x03;
    uint8_t frameSubtype = (payload[0] >> 4) & 0x0F;

    if (frameType != 0) return;
    if (frameSubtype != 12 && frameSubtype != 10) return;

    deauthTotalCount++;
    uint32_t now = millis();
    deauthLastTime = now;

    if (now - _lastLogTime < LOG_THROTTLE_MS) return;
    _lastLogTime = now;

    if (deauthLogCount >= DEAUTH_LOG_SIZE) {
        for (int i = 0; i < DEAUTH_LOG_SIZE - 1; i++) {
            deauthLog[i] = deauthLog[i + 1];
        }
    }
    int idx = (deauthLogCount < DEAUTH_LOG_SIZE)
                  ? deauthLogCount
                  : DEAUTH_LOG_SIZE - 1;

    memcpy(deauthLog[idx].destination, &payload[4],  6);
    memcpy(deauthLog[idx].source,      &payload[10], 6);
    memcpy(deauthLog[idx].bssid,       &payload[16], 6);
    deauthLog[idx].reasonCode = payload[24] | (payload[25] << 8);
    deauthLog[idx].rssi       = pkt->rx_ctrl.rssi;
    deauthLog[idx].timestamp  = now;
    deauthLog[idx].isDisassoc = (frameSubtype == 10);
    deauthLog[idx].isTargeted = _hasOurBSSID &&
                                (memcmp(&payload[16], _ourBSSID, 6) == 0);

    if (deauthLogCount < DEAUTH_LOG_SIZE) deauthLogCount++;

    _newAlert = true;
}


// ── Lifecycle ────────────────────────────────────────
void deauthDetectorBegin() {
    if (deauthDetectorActive) return;
    if (!wifiConnected()) {
        Serial.println("[Deauth] Cannot start — not connected to WiFi.");
        return;
    }

    uint8_t* bssid = WiFi.BSSID();
    if (bssid) {
        memcpy(_ourBSSID, bssid, 6);
        _hasOurBSSID = true;
    } else {
        _hasOurBSSID = false;
    }

    deauthTotalCount = 0;
    deauthLogCount   = 0;
    deauthLastTime   = 0;
    _lastLogTime     = 0;
    _newAlert        = false;

    wifi_promiscuous_filter_t filter = {
        .filter_mask = WIFI_PROMIS_FILTER_MASK_MGMT
    };
    esp_wifi_set_promiscuous_filter(&filter);
    esp_wifi_set_promiscuous_rx_cb(_deauthSniffer);
    esp_wifi_set_promiscuous(true);

    deauthDetectorActive = true;

    if (_hasOurBSSID) {
        Serial.printf(
            "[Deauth] Detector ACTIVE on Ch:%d | BSSID: "
            "%02x:%02x:%02x:%02x:%02x:%02x\n",
            WiFi.channel(),
            _ourBSSID[0], _ourBSSID[1], _ourBSSID[2],
            _ourBSSID[3], _ourBSSID[4], _ourBSSID[5]);
    } else {
        Serial.printf("[Deauth] Detector ACTIVE on Ch:%d | BSSID: unknown\n",
                      WiFi.channel());
    }
}

void deauthDetectorEnd() {
    if (!deauthDetectorActive) return;
    esp_wifi_set_promiscuous(false);
    deauthDetectorActive = false;
    Serial.printf("[Deauth] Detector stopped. Total frames: %lu\n",
                  (unsigned long)deauthTotalCount);
}

bool isDeauthDetectorActive() { return deauthDetectorActive; }


// ── Update ───────────────────────────────────────────
void deauthDetectorUpdate() {
    if (!deauthDetectorActive) return;

    if (!wifiConnected()) {
        deauthDetectorEnd();
        return;
    }

    if (_newAlert) {
        _newAlert = false;
        if (deauthLogCount > 0) {
            DeauthEvent& e = deauthLog[deauthLogCount - 1];
            const char* frameName = e.isDisassoc ? "DISASSOC" : "DEAUTH";
            Serial.printf(
                "[Deauth] !!! %s: %02x:%02x:%02x:%02x:%02x:%02x → "
                "%02x:%02x:%02x:%02x:%02x:%02x | "
                "Reason:%d (%s) | %d dBm%s\n",
                frameName,
                e.source[0], e.source[1], e.source[2],
                e.source[3], e.source[4], e.source[5],
                e.destination[0], e.destination[1], e.destination[2],
                e.destination[3], e.destination[4], e.destination[5],
                e.reasonCode, deauthReasonString(e.reasonCode),
                e.rssi,
                e.isTargeted ? " [TARGETED!]" : "");
        }
    }

    static uint32_t lastStatusPrint = 0;
    uint32_t now = millis();
    if (now - lastStatusPrint > 120000) {
        lastStatusPrint = now;
        if (deauthTotalCount > 0) {
            Serial.printf(
                "[Deauth] Monitoring... %lu frames seen, last %lus ago\n",
                (unsigned long)deauthTotalCount,
                (unsigned long)((now - deauthLastTime) / 1000));
        } else {
            Serial.println("[Deauth] Monitoring... channel is clean.");
        }
    }
}


// ── Alert queries ────────────────────────────────────
bool deauthHasRecentAlert(uint32_t withinMs) {
    if (deauthTotalCount == 0) return false;
    return (millis() - deauthLastTime) <= withinMs;
}

uint32_t deauthRecentCount(uint32_t withinMs) {
    uint32_t count  = 0;
    uint32_t cutoff = millis() - withinMs;
    for (int i = 0; i < deauthLogCount; i++) {
        if (deauthLog[i].timestamp >= cutoff) count++;
    }
    return count;
}


// ── Threat analysis ──────────────────────────────────
uint8_t deauthThreatScore() {
    if (deauthTotalCount == 0) return 0;

    uint8_t score = 0;

    // Recency
    uint32_t msSinceLast = millis() - deauthLastTime;
    if (msSinceLast < 1000)       score += 30;
    else if (msSinceLast < 5000)  score += 20;
    else if (msSinceLast < 30000) score += 10;

    // Volume in last 10 seconds
    uint32_t recent = deauthRecentCount(10000);
    if (recent >= 10)     score += 40;
    else if (recent >= 5) score += 25;
    else if (recent >= 2) score += 10;

    // Targeting
    for (int i = 0; i < deauthLogCount; i++) {
        if (deauthLog[i].isTargeted) {
            score += 30;
            break;
        }
    }

    return (score > 100) ? 100 : score;
}

bool deauthUnderAttack() {
    if (deauthThreatScore() < 70) return false;
    for (int i = 0; i < deauthLogCount; i++) {
        if (deauthLog[i].isTargeted) return true;
    }
    return false;
}

uint8_t deauthUniqueSourceCount() {
    uint8_t unique[DEAUTH_LOG_SIZE][6];
    uint8_t count = 0;

    for (int i = 0; i < deauthLogCount; i++) {
        bool found = false;
        for (int j = 0; j < count; j++) {
            if (memcmp(unique[j], deauthLog[i].source, 6) == 0) {
                found = true;
                break;
            }
        }
        if (!found) {
            memcpy(unique[count], deauthLog[i].source, 6);
            count++;
        }
    }
    return count;
}

int deauthMostActiveSourceIndex() {
    if (deauthLogCount == 0) return -1;

    uint8_t macs[DEAUTH_LOG_SIZE][6];
    uint8_t counts[DEAUTH_LOG_SIZE] = {0};
    uint8_t macCount = 0;

    for (int i = 0; i < deauthLogCount; i++) {
        bool found = false;
        for (int j = 0; j < macCount; j++) {
            if (memcmp(macs[j], deauthLog[i].source, 6) == 0) {
                counts[j]++;
                found = true;
                break;
            }
        }
        if (!found) {
            memcpy(macs[macCount], deauthLog[i].source, 6);
            counts[macCount] = 1;
            macCount++;
        }
    }

    uint8_t maxCount = 0;
    int maxMacIdx = 0;
    for (int i = 0; i < macCount; i++) {
        if (counts[i] > maxCount) {
            maxCount  = counts[i];
            maxMacIdx = i;
        }
    }

    for (int i = 0; i < deauthLogCount; i++) {
        if (memcmp(deauthLog[i].source, macs[maxMacIdx], 6) == 0) {
            return i;
        }
    }
    return 0;
}


// ── Debug ────────────────────────────────────────────
void deauthPrintInfo() {
    Serial.println();
    Serial.println("[Deauth] ==========================================");
    Serial.printf("[Deauth]  Status:     %s\n",
                  deauthDetectorActive ? "ACTIVE" : "INACTIVE");
    if (deauthDetectorActive || deauthTotalCount > 0) {
        Serial.printf("[Deauth]  Channel:    %d\n", WiFi.channel());
        Serial.printf("[Deauth]  BSSID:      %s\n",
                      _hasOurBSSID
                          ? deauthMacToString(_ourBSSID).c_str()
                          : "unknown");
        Serial.printf("[Deauth]  Total:      %lu frames\n",
                      (unsigned long)deauthTotalCount);
        if (deauthTotalCount > 0) {
            Serial.printf("[Deauth]  Last seen:  %lus ago\n",
                          (unsigned long)((millis() - deauthLastTime) / 1000));
        }
        Serial.printf("[Deauth]  Recent(10s): %lu\n",
                      (unsigned long)deauthRecentCount(10000));
        Serial.printf("[Deauth]  Threat Score: %d/100\n", deauthThreatScore());
        Serial.printf("[Deauth]  Under Attack: %s\n",
                      deauthUnderAttack() ? "YES" : "NO");
        Serial.printf("[Deauth]  Unique Sources: %d\n",
                      deauthUniqueSourceCount());
    }
    Serial.println("[Deauth] ==========================================");
    Serial.println();
}

void deauthPrintLog() {
    if (deauthLogCount == 0) {
        Serial.println("[Deauth] No deauth frames logged.");
        return;
    }

    Serial.println();
    Serial.println("[Deauth] --- Deauth Log ---");
    for (int i = 0; i < deauthLogCount; i++) {
        DeauthEvent& e = deauthLog[i];
        const char* frameName = e.isDisassoc ? "DISASSOC" : "DEAUTH";
        Serial.printf("[Deauth] %2d: [%s] %s → %s\n", i + 1, frameName,
                      deauthMacToString(e.source).c_str(),
                      deauthMacToString(e.destination).c_str());
        Serial.printf("[Deauth]      BSSID: %s | Reason: %d (%s)\n",
                      deauthMacToString(e.bssid).c_str(),
                      e.reasonCode, deauthReasonString(e.reasonCode));
        Serial.printf("[Deauth]      RSSI: %d dBm | %lus ago%s\n",
                      e.rssi,
                      (unsigned long)((millis() - e.timestamp) / 1000),
                      e.isTargeted ? " [TARGETED!]" : "");
    }
    Serial.println("[Deauth] ------------------");
    Serial.println();
}


// ── Helpers ──────────────────────────────────────────
String deauthMacToString(const uint8_t* mac) {
    char buf[18];
    snprintf(buf, sizeof(buf), "%02x:%02x:%02x:%02x:%02x:%02x",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    return String(buf);
}

const char* deauthReasonString(uint16_t reason) {
    switch (reason) {
        case 1:  return "Unspecified";
        case 2:  return "Prev auth invalid";
        case 3:  return "Station leaving";
        case 4:  return "Inactivity timeout";
        case 5:  return "AP overloaded";
        case 6:  return "Class 2 non-auth";
        case 7:  return "Class 3 non-assoc";
        case 8:  return "Station left BSS";
        case 9:  return "Station not auth";
        case 10: return "Power cap invalid";
        case 11: return "Channels unsupported";
        case 12: return "Moving to other BSS";
        case 13: return "Auth invalid";
        case 14: return "Cipher rejected";
        case 15: return "TSF invalid";
        case 16: return "QoS unsupported";
        case 17: return "Bandwidth invalid";
        case 18: return "Channel invalid";
        case 22: return "Probe delayed";
        case 23: return "PHY invalid";
        case 24: return "AP no longer valid";
        case 25: return "Poor conditions";
        case 26: return "AP switched channels";
        case 30: return "Rejected spectrum";
        case 31: return "Rejected data rate";
        case 32: return "Rejected PHY";
        default: return "Unknown";
    }
}