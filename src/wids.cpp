#include "wids.h"
#include "wifi_manager.h"
#include "oui_lookup.h"
#include "sd_manager.h"
#include "gps_manager.h"
#include "esp_wifi.h"
#include <WiFi.h>
#include <SD.h>

#define BEACON_FLOOD_THRESHOLD 35
#define CTS_DURATION_SUSPICIOUS 20000
#define CTS_JAM_CLOSE_RANGE_MIN 50
#define CTS_JAM_FAR_RANGE_MIN 100
#define CTS_JAM_CLOSE_RSSI -70
#define HANDSHAKE_WINDOW_MS 5000
#define BURST_DEDUP_MS 3000

#define MAX_TRACKED_BSSIDS 40
static const uint8_t BROADCAST_MAC[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

ThreatEvent widsLog[WIDS_LOG_SIZE];
int widsLogCount = 0;
int widsLogHead = 0;
uint32_t widsTotalCount = 0;
uint32_t widsLastTime = 0;
bool widsActive = false;

static uint8_t _ourBSSID[6];
static String _ourSSID = "";
static bool _hasOurBSSID = false;
static bool _ourNetSecured = true;

static volatile bool _newAlert = false;
static String _sessionFilePath = "";
static uint32_t _lastDeauthTime = 0;

static uint32_t _lastBeaconFloodCheck = 0;
static uint8_t _trackedBssids[MAX_TRACKED_BSSIDS][6];
static uint16_t _trackedBssidCount = 0;

static uint16_t _ctsHighDurCount = 0;
static uint16_t _ctsTotalCount = 0;
static uint32_t _lastCtsCheck = 0;
static uint8_t _ctsWorstSource[6] = {0};
static uint32_t _ctsMaxDuration = 0;
static int _ctsWorstRssi = -127;

#define WIDS_QUEUE_SIZE 32

struct WidsAlert
{
    AttackType type;
    uint8_t src[6];
    uint8_t dst[6];
    uint8_t bssid[6];
    uint16_t param;
    int8_t rssi;
    uint32_t timestamp;
    char notes[24];
};

static volatile WidsAlert _alertQueue[WIDS_QUEUE_SIZE];
static volatile size_t _alertHead = 0;
static volatile size_t _alertTail = 0;
static portMUX_TYPE _queueMux = portMUX_INITIALIZER_UNLOCKED;

static portMUX_TYPE _widsLogMux = portMUX_INITIALIZER_UNLOCKED;
static portMUX_TYPE widsMux = portMUX_INITIALIZER_UNLOCKED;

static String getWidsTimeString()
{
    LocalTime lt = gpsGetLocalTime();
    char buf[16];
    if (lt.valid)
        snprintf(buf, sizeof(buf), "%02d:%02d:%02d", lt.hour, lt.minute, lt.second);
    else
        snprintf(buf, sizeof(buf), "%02d:%02d:%02d", gpsData.hour, gpsData.minute, gpsData.second);
    return String(buf);
}

String widsMacToString(const uint8_t *mac)
{
    char buf[18];
    widsMacToBuf(mac, buf);
    return String(buf);
}

const char *widsAttackTypeString(AttackType type)
{
    switch (type)
    {
    case ATTACK_DEAUTH:
        return "DEAUTH";
    case ATTACK_DISASSOC:
        return "DISASSOC";
    case ATTACK_EVIL_TWIN:
        return "EVIL_TWIN";
    case ATTACK_CTS_JAMMING:
        return "CTS_JAMMING";
    case ATTACK_HANDSHAKE_CAPTURE:
        return "HANDSHAKE_CAPTURE";
    case ATTACK_BEACON_FLOOD:
        return "BEACON_FLOOD";
    default:
        return "UNKNOWN";
    }
}

const char *widsReasonString(uint16_t reason)
{
    switch (reason)
    {
    case 1:
        return "Unspecified";
    case 2:
        return "Prev auth invalid";
    case 3:
        return "Station leaving";
    case 4:
        return "Inactivity timeout";
    case 6:
        return "Class 2 non-auth";
    case 7:
        return "Class 3 non-assoc";
    case 8:
        return "Station left BSS";
    case 15:
        return "TSF invalid";
    default:
        return (reason > 500) ? "Microseconds (NAV)" : "Unknown/Custom";
    }
}

void widsMacToBuf(const uint8_t *mac, char *outBuf)
{
    snprintf(outBuf, 18, "%02x:%02x:%02x:%02x:%02x:%02x",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}

static int getLogIndex(int virtualIndex)
{
    if (widsLogCount == 0)
        return -1;
    int idx = widsLogHead - widsLogCount + virtualIndex;
    if (idx < 0)
        idx += WIDS_LOG_SIZE;
    return idx % WIDS_LOG_SIZE;
}

static void initSdLogFile()
{
    if (!sdActive)
        return;

    if (!SD.exists("/wids"))
        SD.mkdir("/wids");

    char dateBuf[16];
    LocalTime lt = gpsGetLocalTime();
    if (lt.valid)
        snprintf(dateBuf, sizeof(dateBuf), "%04d%02d%02d", lt.year, lt.month, lt.day);
    else if (gpsData.year > 2000)
        snprintf(dateBuf, sizeof(dateBuf), "%04d%02d%02d", gpsData.year, gpsData.month, gpsData.day);
    else
        strcpy(dateBuf, "NODATE");

    _sessionFilePath = "/wids/WIDS_" + String(dateBuf) + ".csv";

    bool isNew = !SD.exists(_sessionFilePath.c_str());
    File f = SD.open(_sessionFilePath.c_str(), FILE_APPEND);
    if (f)
    {
        if (isNew)
            f.println("time,lat,lon,attack_type,source_mac,source_vendor,dest_mac,dest_vendor,bssid,param,rssi,burst_count,flags,notes");
        f.close();
        Serial.printf("[WIDS] Attack log ready on SD: %s\n", _sessionFilePath.c_str());
    }
    else
    {
        Serial.printf("[WIDS] Error: Could not create SD file %s\n", _sessionFilePath.c_str());
        _sessionFilePath = "";
    }
}

static void processQueuedThreat(const WidsAlert &alert)
{
    widsTotalCount++;
    widsLastTime = alert.timestamp;

    portENTER_CRITICAL(&_widsLogMux);

    if (widsLogCount > 0)
    {
        int lastIdx = (widsLogHead - 1 + WIDS_LOG_SIZE) % WIDS_LOG_SIZE;
        ThreatEvent &last = widsLog[lastIdx];

        if (last.type == alert.type &&
            (alert.timestamp - last.timestamp < BURST_DEDUP_MS) &&
            memcmp(last.source, alert.src, 6) == 0 &&
            memcmp(last.destination, alert.dst, 6) == 0)
        {
            last.burstCount++;
            last.timestamp = alert.timestamp;
            last.rssi = (last.rssi + alert.rssi) / 2;
            _newAlert = true;
            portEXIT_CRITICAL(&_widsLogMux);
            return;
        }
    }

    ThreatEvent &e = widsLog[widsLogHead];
    e.type = alert.type;
    memcpy(e.destination, alert.dst, 6);
    memcpy(e.source, alert.src, 6);
    memcpy(e.bssid, alert.bssid, 6);
    e.param = alert.param;
    e.rssi = alert.rssi;
    e.timestamp = alert.timestamp;
    e.firstTimestamp = alert.timestamp;
    e.burstCount = 1;
    e.isBroadcast = (memcmp(alert.dst, BROADCAST_MAC, 6) == 0);
    e.isTargeted = _hasOurBSSID &&
                   (memcmp(alert.bssid, _ourBSSID, 6) == 0 ||
                    memcmp(alert.dst, _ourBSSID, 6) == 0);

    e.latitude = gpsData.latitude;
    e.longitude = gpsData.longitude;
    e.gpsValid = gpsData.valid;

    strncpy(e.customNotes, alert.notes, sizeof(e.customNotes) - 1);
    e.customNotes[sizeof(e.customNotes) - 1] = '\0';

    widsLogHead = (widsLogHead + 1) % WIDS_LOG_SIZE;
    if (widsLogCount < WIDS_LOG_SIZE)
        widsLogCount++;

    _newAlert = true;

    portEXIT_CRITICAL(&_widsLogMux);
}

static void IRAM_ATTR enqueueThreat(AttackType type, uint8_t *src, uint8_t *dst,
                                    uint8_t *bssid, uint16_t param, int rssi,
                                    const char *notes)
{
    portENTER_CRITICAL_ISR(&_queueMux);

    size_t next = (_alertHead + 1) % WIDS_QUEUE_SIZE;
    if (next == _alertTail)
    {
        portEXIT_CRITICAL_ISR(&_queueMux);
        return;
    }

    WidsAlert *a = (WidsAlert *)&_alertQueue[_alertHead];
    a->type = type;
    memcpy((void *)a->src, src, 6);
    memcpy((void *)a->dst, dst, 6);
    memcpy((void *)a->bssid, bssid, 6);
    a->param = param;
    a->rssi = (int8_t)rssi;
    a->timestamp = millis();

    if (notes)
    {
        strncpy((char *)a->notes, notes, sizeof(a->notes) - 1);
        ((char *)a->notes)[sizeof(a->notes) - 1] = '\0';
    }
    else
    {
        ((char *)a->notes)[0] = '\0';
    }

    _alertHead = next;
    portEXIT_CRITICAL_ISR(&_queueMux);
}

static void drainThreatQueue()
{
    while (true)
    {
        portENTER_CRITICAL(&_queueMux);
        if (_alertTail == _alertHead)
        {
            portEXIT_CRITICAL(&_queueMux);
            break;
        }

        WidsAlert alert;
        memcpy(&alert, (const void *)&_alertQueue[_alertTail], sizeof(WidsAlert));
        _alertTail = (_alertTail + 1) % WIDS_QUEUE_SIZE;
        portEXIT_CRITICAL(&_queueMux);

        processQueuedThreat(alert);
    }
}

static void handleDeauthFrame(uint8_t *payload, uint16_t len, uint8_t subtype, int rssi, uint32_t now)
{
    uint8_t *destMac = &payload[4];
    uint8_t *sourceMac = &payload[10];
    uint8_t *bssidMac = &payload[16];
    uint16_t reason = payload[24] | (payload[25] << 8);
    AttackType aType = (subtype == 12) ? ATTACK_DEAUTH : ATTACK_DISASSOC;

    bool targetingUs = _hasOurBSSID &&
                       (memcmp(bssidMac, _ourBSSID, 6) == 0 ||
                        memcmp(destMac, _ourBSSID, 6) == 0);

    if (!targetingUs)
        return;

    bool legitimateReason = (reason == 1 || reason == 3 || reason == 4 ||
                             reason == 6 || reason == 7 || reason == 8);

    if (!legitimateReason)
    {
        _lastDeauthTime = now;
        enqueueThreat(aType, sourceMac, destMac, bssidMac, reason, rssi, "Targeted");
    }
    else if (rssi > -60)
    {
        _lastDeauthTime = now;
        enqueueThreat(aType, sourceMac, destMac, bssidMac, reason, rssi, "Close_Range");
    }
}

static void handleBeaconFrame(uint8_t *payload, uint16_t len, int rssi, uint32_t now)
{
    if (!_hasOurBSSID || len <= 36)
        return;

    uint8_t *sourceMac = &payload[10];
    uint8_t *destMac = &payload[4];
    uint8_t *bssidMac = &payload[16];

    if (now - _lastBeaconFloodCheck > 1000)
    {
        if (_trackedBssidCount >= BEACON_FLOOD_THRESHOLD)
        {
            enqueueThreat(ATTACK_BEACON_FLOOD,
                          (uint8_t *)BROADCAST_MAC, (uint8_t *)BROADCAST_MAC,
                          (uint8_t *)BROADCAST_MAC, _trackedBssidCount, rssi, "SSID_Flood");
        }
        _trackedBssidCount = 0;
        _lastBeaconFloodCheck = now;
    }
    else
    {
        bool found = false;
        for (int i = 0; i < _trackedBssidCount; i++)
        {
            if (memcmp(_trackedBssids[i], bssidMac, 6) == 0)
            {
                found = true;
                break;
            }
        }
        if (!found && _trackedBssidCount < MAX_TRACKED_BSSIDS)
        {
            memcpy(_trackedBssids[_trackedBssidCount], bssidMac, 6);
            _trackedBssidCount++;
        }
    }

    uint8_t tagNumber = payload[36];
    uint8_t tagLen = payload[37];
    if (tagNumber != 0 || tagLen == 0 || tagLen > 32 || (38 + tagLen >= len))
        return;

    char ssidBuf[33];
    memcpy(ssidBuf, &payload[38], tagLen);
    ssidBuf[tagLen] = '\0';

    if (!_ourSSID.equals(ssidBuf) || memcmp(bssidMac, _ourBSSID, 6) == 0)
        return;

    bool sameVendorPrefix = (memcmp(bssidMac, _ourBSSID, 4) == 0);
    bool beaconIsSecured = (payload[34] & 0x10) != 0;
    bool securityMismatch = (_ourNetSecured != beaconIsSecured);

    if (!sameVendorPrefix || securityMismatch)
    {
        const char *note = securityMismatch ? "Auth_Mismatch" : "Rogue_BSSID";
        enqueueThreat(ATTACK_EVIL_TWIN, sourceMac, destMac, bssidMac, 0, rssi, note);
    }
}

static void handleCtsFrame(uint8_t *payload, uint16_t len, int rssi, uint32_t now)
{
    if (len < 10)
        return;

    uint16_t duration = payload[1] | (payload[2] << 8);
    uint8_t *targetMac = &payload[4];

    _ctsTotalCount++;

    if (duration >= CTS_DURATION_SUSPICIOUS)
    {
        _ctsHighDurCount++;

        if (duration > _ctsMaxDuration)
        {
            _ctsMaxDuration = duration;
            memcpy(_ctsWorstSource, targetMac, 6);
            _ctsWorstRssi = rssi;
        }
    }

    if (now - _lastCtsCheck < 1000)
        return;

    bool isJamming = false;
    char noteBuf[24];

    if (_ctsHighDurCount >= CTS_JAM_CLOSE_RANGE_MIN && _ctsWorstRssi > CTS_JAM_CLOSE_RSSI)
    {
        snprintf(noteBuf, sizeof(noteBuf), "NAV_Jam_%luus", (unsigned long)_ctsMaxDuration);
        isJamming = true;
    }
    else if (_ctsHighDurCount >= CTS_JAM_FAR_RANGE_MIN)
    {
        snprintf(noteBuf, sizeof(noteBuf), "NAV_Flood_%luus", (unsigned long)_ctsMaxDuration);
        isJamming = true;
    }

    if (isJamming)
    {
        enqueueThreat(ATTACK_CTS_JAMMING, (uint8_t *)BROADCAST_MAC,
                      _ctsWorstSource, _ctsWorstSource,
                      _ctsMaxDuration, _ctsWorstRssi, noteBuf);
    }

    _ctsHighDurCount = 0;
    _ctsTotalCount = 0;
    _ctsMaxDuration = 0;
    _ctsWorstRssi = -127;
    memset(_ctsWorstSource, 0, 6);
    _lastCtsCheck = now;
}

static void handleDataFrame(uint8_t *payload, uint16_t len, uint8_t subtype, int rssi, uint32_t now)
{
    if (len < 24)
        return;

    uint16_t hdrLen = (subtype == 8) ? 26 : 24;
    if (len < hdrLen + 8)
        return;

    uint8_t *data = &payload[hdrLen];

    if (data[0] != 0xAA || data[1] != 0xAA || data[6] != 0x88 || data[7] != 0x8E)
        return;

    if (_lastDeauthTime > 0 && (now - _lastDeauthTime < HANDSHAKE_WINDOW_MS))
    {
        uint8_t *destMac = &payload[4];
        uint8_t *sourceMac = &payload[10];
        uint8_t *bssidMac = &payload[16];
        enqueueThreat(ATTACK_HANDSHAKE_CAPTURE, sourceMac, destMac, bssidMac,
                      0, rssi, "Forced_WPA_Capture");
    }
}

static void IRAM_ATTR _widsSniffer(void *buf, wifi_promiscuous_pkt_type_t type)
{
    wifi_promiscuous_pkt_t *pkt = (wifi_promiscuous_pkt_t *)buf;
    uint8_t *payload = pkt->payload;
    uint16_t len = pkt->rx_ctrl.sig_len;

    if (len < 24)
        return;

    uint8_t frameType = (payload[0] >> 2) & 0x03;
    uint8_t frameSubtype = (payload[0] >> 4) & 0x0F;

    if (frameType == 0)
    {
        uint8_t *destMac = &payload[4];
        uint8_t *sourceMac = &payload[10];
        uint8_t *bssidMac = &payload[16];

        if (frameSubtype == 12 || frameSubtype == 10)
        {
            uint16_t reason = payload[24] | (payload[25] << 8);
            AttackType aType = (frameSubtype == 12) ? ATTACK_DEAUTH : ATTACK_DISASSOC;

            bool targetingUs = _hasOurBSSID && (memcmp(bssidMac, _ourBSSID, 6) == 0);
            if (targetingUs && reason > 1)
            {
                enqueueThreat(aType, sourceMac, destMac, bssidMac, reason, pkt->rx_ctrl.rssi, "Targeted");
            }
            return;
        }

        if (frameSubtype == 8 && _hasOurBSSID && len > 36)
        {
            uint8_t tagLen = payload[37];
            if (payload[36] == 0 && tagLen > 0 && tagLen <= 32)
            {
                char ssidBuf[33];
                memcpy(ssidBuf, &payload[38], tagLen);
                ssidBuf[tagLen] = '\0';

                if (strcmp(_ourSSID.c_str(), ssidBuf) == 0 && memcmp(bssidMac, _ourBSSID, 6) != 0)
                {
                    enqueueThreat(ATTACK_EVIL_TWIN, sourceMac, destMac, bssidMac, 0, pkt->rx_ctrl.rssi, "Rogue_BSSID");
                }
            }
            return;
        }
    }

    if (frameType == 1 && frameSubtype == 12)
    {
        uint16_t duration = payload[1] | (payload[2] << 8);
        if (duration >= 20000)
        {
            _ctsHighDurCount++;
            if (duration > _ctsMaxDuration)
            {
                _ctsMaxDuration = duration;
                memcpy(_ctsWorstSource, &payload[4], 6);
                _ctsWorstRssi = pkt->rx_ctrl.rssi;
            }
        }
        return;
    }

    if (frameType == 2 && (frameSubtype == 0 || frameSubtype == 8))
    {
        uint16_t hdrLen = (frameSubtype == 8) ? 26 : 24;
        if (len >= hdrLen + 8)
        {
            uint8_t *data = &payload[hdrLen];
            if (data[0] == 0xAA && data[1] == 0xAA && data[6] == 0x88 && data[7] == 0x8E)
            {
                enqueueThreat(ATTACK_HANDSHAKE_CAPTURE, &payload[10], &payload[4], &payload[16], 0, pkt->rx_ctrl.rssi, "WPA_Capture");
            }
        }
    }
}

void widsBegin()
{
    if (widsActive)
        return;

    if (!wifiConnected())
    {
        Serial.println("[WIDS] Cannot start — not connected to WiFi.");
        return;
    }

    _ourSSID = WiFi.SSID();
    uint8_t *bssid = WiFi.BSSID();
    if (bssid)
    {
        memcpy(_ourBSSID, bssid, 6);
        _hasOurBSSID = true;
    }
    else
    {
        _hasOurBSSID = false;
    }

    wifi_ap_record_t ap_info;
    if (esp_wifi_sta_get_ap_info(&ap_info) == ESP_OK)
    {
        _ourNetSecured = (ap_info.authmode != WIFI_AUTH_OPEN);
    }

    _newAlert = false;
    _trackedBssidCount = 0;
    _lastDeauthTime = 0;
    _alertHead = 0;
    _alertTail = 0;
    _ctsHighDurCount = 0;
    _ctsTotalCount = 0;
    _lastCtsCheck = 0;
    _ctsMaxDuration = 0;
    _ctsWorstRssi = -127;
    memset(_ctsWorstSource, 0, 6);

    initSdLogFile();

    wifi_promiscuous_filter_t filter = {
        .filter_mask = WIFI_PROMIS_FILTER_MASK_MGMT |
                       WIFI_PROMIS_FILTER_MASK_CTRL |
                       WIFI_PROMIS_FILTER_MASK_DATA};
    esp_wifi_set_promiscuous_filter(&filter);

    wifi_promiscuous_filter_t ctrlFilter = {.filter_mask = WIFI_PROMIS_CTRL_FILTER_MASK_ALL};
    esp_wifi_set_promiscuous_ctrl_filter(&ctrlFilter);

    esp_wifi_set_promiscuous_rx_cb(_widsSniffer);
    esp_wifi_set_promiscuous(true);

    widsActive = true;
    Serial.printf("[WIDS] CLUNCHI Tactical IDS ACTIVE on Ch:%d | Protecting SSID: %s [%s]\n",
                  WiFi.channel(), _ourSSID.c_str(), _ourNetSecured ? "SECURED" : "OPEN");
}

void widsEnd()
{
    if (!widsActive)
        return;

    esp_wifi_set_promiscuous(false);
    widsActive = false;
    Serial.printf("[WIDS] Detector stopped. Total anomaly frames: %lu\n",
                  (unsigned long)widsTotalCount);
}

bool isWidsActive() { return widsActive; }

static void logAlertToSD(const ThreatEvent e)
{
    if (!sdActive)
        return;

    if (_sessionFilePath == "")
        initSdLogFile();
    if (_sessionFilePath == "")
        return;

    File f = SD.open(_sessionFilePath.c_str(), FILE_APPEND);
    if (!f)
    {
        Serial.printf("[WIDS] SD Write Error on %s\n", _sessionFilePath.c_str());
        return;
    }

    String srcMac = widsMacToString(e.source);
    String dstMac = widsMacToString(e.destination);
    String srcVendor = lookupOUI(srcMac);
    String dstVendor = lookupOUI(dstMac);
    if (srcVendor == "")
        srcVendor = "Unknown";
    if (dstVendor == "")
        dstVendor = "Unknown";

    String flags = e.isBroadcast ? "BROADCAST" : (e.isTargeted ? "TARGETED" : "GENERAL");

    char line[512];
    snprintf(line, sizeof(line), "%s,%.6f,%.6f,%s,%s,%s,%s,%s,%s,%d,%d,%d,%s,%s",
             getWidsTimeString().c_str(),
             e.gpsValid ? e.latitude : 0.0,
             e.gpsValid ? e.longitude : 0.0,
             widsAttackTypeString(e.type),
             srcMac.c_str(), srcVendor.c_str(),
             dstMac.c_str(), dstVendor.c_str(),
             widsMacToString(e.bssid).c_str(),
             e.param, e.rssi, e.burstCount,
             flags.c_str(), e.customNotes);
    f.println(line);
    f.flush();
    f.close();
}

void widsUpdate()
{
    if (!widsActive)
        return;

    if (!wifiConnected())
    {
        widsEnd();
        return;
    }

    drainThreatQueue();

    if (_newAlert)
    {
        _newAlert = false;

        ThreatEvent eventCopy;
        bool haveEvent = false;

        portENTER_CRITICAL(&_widsLogMux);
        if (widsLogCount > 0)
        {
            int lastIdx = (widsLogHead - 1 + WIDS_LOG_SIZE) % WIDS_LOG_SIZE;
            eventCopy = widsLog[lastIdx];
            haveEvent = true;
        }
        portEXIT_CRITICAL(&_widsLogMux);

        if (haveEvent && (eventCopy.burstCount == 1 || eventCopy.burstCount % 25 == 0))
        {
            char srcMac[18], dstMac[18];
            widsMacToBuf(eventCopy.source, srcMac);
            widsMacToBuf(eventCopy.destination, dstMac);

            String srcVendor = lookupOUI(srcMac);
            String dstVendor = lookupOUI(dstMac);
            if (srcVendor.isEmpty())
                srcVendor = "Unknown";
            if (dstVendor.isEmpty())
                dstVendor = "Unknown";

            const char *flags = eventCopy.isBroadcast ? "BROADCAST" : (eventCopy.isTargeted ? "TARGETED" : "GENERAL");

            Serial.printf("[WIDS ALERT] %s (x%d) | Src: %s (%s) -> Dst: %s (%s) [%s] | %d dBm | Notes: %s\n",
                          widsAttackTypeString(eventCopy.type), eventCopy.burstCount,
                          srcMac, srcVendor.c_str(),
                          dstMac, dstVendor.c_str(),
                          flags, eventCopy.rssi, eventCopy.customNotes);

            logAlertToSD(eventCopy);
        }
    }

    static uint32_t lastStatusPrint = 0;
    uint32_t now = millis();
    if (now - lastStatusPrint > 60000)
    {
        lastStatusPrint = now;
        if (widsTotalCount > 0)
        {
            Serial.printf("[WIDS Status] %lu attack frames logged. Last incident %lus ago.\n",
                          (unsigned long)widsTotalCount,
                          (unsigned long)((now - widsLastTime) / 1000));
        }
    }
}

bool widsHasRecentAlert(uint32_t withinMs)
{
    if (widsTotalCount == 0)
        return false;
    return (millis() - widsLastTime) <= withinMs;
}

uint32_t widsRecentCount(uint32_t withinMs)
{
    uint32_t count = 0;
    uint32_t cutoff = millis() - withinMs;

    portENTER_CRITICAL(&_widsLogMux);
    for (int i = 0; i < widsLogCount; i++)
    {
        int idx = getLogIndex(i);
        if (widsLog[idx].timestamp >= cutoff)
            count += widsLog[idx].burstCount;
    }
    portEXIT_CRITICAL(&_widsLogMux);

    return count;
}

uint8_t widsThreatScore()
{
    if (widsTotalCount == 0)
        return 0;

    uint8_t score = 0;
    uint32_t now = millis();
    uint32_t msSinceLast = now - widsLastTime;

    if (msSinceLast < 2000)
        score += 40;
    else if (msSinceLast < 10000)
        score += 20;
    else if (msSinceLast < 60000)
        score += 10;

    uint32_t recent = widsRecentCount(10000);
    if (recent >= 50)
        score += 50;
    else if (recent >= 15)
        score += 30;
    else if (recent >= 5)
        score += 15;

    portENTER_CRITICAL(&_widsLogMux);
    for (int i = 0; i < widsLogCount; i++)
    {
        int idx = getLogIndex(i);
        if (now - widsLog[idx].timestamp < 30000)
        {
            if (widsLog[idx].isTargeted)
                score += 30;
            if (widsLog[idx].type == ATTACK_EVIL_TWIN)
                score += 40;
            if (widsLog[idx].type == ATTACK_HANDSHAKE_CAPTURE)
                score += 35;
            if (widsLog[idx].type == ATTACK_BEACON_FLOOD)
                score += 30;
            if (widsLog[idx].type == ATTACK_CTS_JAMMING)
                score += 30;
            if (widsLog[idx].type == ATTACK_DEAUTH ||
                widsLog[idx].type == ATTACK_DISASSOC)
                score += 25;
        }
    }
    portEXIT_CRITICAL(&_widsLogMux);

    return (score > 100) ? 100 : score;
}

bool widsUnderAttack() { return (widsThreatScore() >= 60); }

uint8_t widsUniqueSourceCount()
{
    uint8_t unique[WIDS_LOG_SIZE][6];
    uint8_t count = 0;

    portENTER_CRITICAL(&_widsLogMux);
    for (int i = 0; i < widsLogCount; i++)
    {
        int idx = getLogIndex(i);
        bool found = false;
        for (int j = 0; j < count; j++)
        {
            if (memcmp(unique[j], widsLog[idx].source, 6) == 0)
            {
                found = true;
                break;
            }
        }
        if (!found && count < WIDS_LOG_SIZE)
        {
            memcpy(unique[count], widsLog[idx].source, 6);
            count++;
        }
    }
    portEXIT_CRITICAL(&_widsLogMux);

    return count;
}

int widsMostActiveSourceIndex()
{
    if (widsLogCount == 0)
        return -1;

    uint8_t macs[WIDS_LOG_SIZE][6];
    uint32_t counts[WIDS_LOG_SIZE] = {0};
    uint8_t macCount = 0;
    int resultIdx = -1;

    portENTER_CRITICAL(&_widsLogMux);

    for (int i = 0; i < widsLogCount; i++)
    {
        int idx = getLogIndex(i);
        bool found = false;
        for (int j = 0; j < macCount; j++)
        {
            if (memcmp(macs[j], widsLog[idx].source, 6) == 0)
            {
                counts[j] += widsLog[idx].burstCount;
                found = true;
                break;
            }
        }
        if (!found && macCount < WIDS_LOG_SIZE)
        {
            memcpy(macs[macCount], widsLog[idx].source, 6);
            counts[macCount] = widsLog[idx].burstCount;
            macCount++;
        }
    }

    uint32_t maxCount = 0;
    int maxMacIdx = 0;
    for (int i = 0; i < macCount; i++)
    {
        if (counts[i] > maxCount)
        {
            maxCount = counts[i];
            maxMacIdx = i;
        }
    }

    for (int i = 0; i < widsLogCount; i++)
    {
        int idx = getLogIndex(i);
        if (memcmp(widsLog[idx].source, macs[maxMacIdx], 6) == 0)
        {
            resultIdx = idx;
            break;
        }
    }
    if (resultIdx == -1)
        resultIdx = getLogIndex(0);

    portEXIT_CRITICAL(&_widsLogMux);

    return resultIdx;
}

void widsPrintInfo()
{
    Serial.println("\n[CLUNCHI Tactical WIDS] ===============================");
    Serial.printf(" Status:         %s\n", widsActive ? "ACTIVE" : "INACTIVE");

    if (widsActive || widsTotalCount > 0)
    {
        Serial.printf(" Monitoring Ch:  %d\n", WiFi.channel());
        Serial.printf(" Protected AP:   %s (%s)\n", _ourSSID.c_str(),
                      _hasOurBSSID ? widsMacToString(_ourBSSID).c_str() : "None");
        Serial.printf(" SD Logging:     %s\n", sdActive ? (_sessionFilePath != "" ? _sessionFilePath.c_str() : "PENDING") : "DISABLED");
        Serial.printf(" Total Incidents:%lu frames\n", (unsigned long)widsTotalCount);

        if (widsTotalCount > 0)
            Serial.printf(" Last Attack:    %lus ago\n",
                          (unsigned long)((millis() - widsLastTime) / 1000));

        Serial.printf(" Threat Score:   %d/100 (%s)\n", widsThreatScore(),
                      widsUnderAttack() ? "ATTACK DETECTED!" : "Normal");
        Serial.printf(" Unique Attackers: %d\n", widsUniqueSourceCount());
    }
    Serial.println("=======================================================\n");
}

void widsPrintLog()
{
    if (widsLogCount == 0)
    {
        Serial.println("[CLUNCHI] Attack log is empty.");
        return;
    }

    ThreatEvent snapshot[WIDS_LOG_SIZE];
    int snapCount = 0;

    portENTER_CRITICAL(&_widsLogMux);
    snapCount = widsLogCount;
    for (int i = 0; i < snapCount; i++)
    {
        int idx = getLogIndex(i);
        snapshot[i] = widsLog[idx];
    }
    portEXIT_CRITICAL(&_widsLogMux);

    Serial.printf("\n--- CLUNCHI Incident Log (Last %d Events) ---\n", snapCount);

    for (int i = 0; i < snapCount; i++)
    {
        ThreatEvent &e = snapshot[i];
        uint32_t ago = (millis() - e.timestamp) / 1000;
        uint32_t duration = (e.timestamp - e.firstTimestamp) / 1000;

        String srcMac = widsMacToString(e.source);
        String dstMac = widsMacToString(e.destination);
        String srcVendor = lookupOUI(srcMac);
        String dstVendor = lookupOUI(dstMac);
        if (srcVendor == "")
            srcVendor = "Unknown";
        if (dstVendor == "")
            dstVendor = "Unknown";

        String flags = "";
        if (e.isBroadcast)
            flags += "[BROADCAST] ";
        if (e.isTargeted)
            flags += "[TARGETED] ";
        if (e.gpsValid)
            flags += String("[GPS: ") + String(e.latitude, 4) + "," + String(e.longitude, 4) + "] ";

        Serial.printf("[%2d] %-17s | x%-4d pkts | %3lus ago | %s (%s) -> %s (%s)\n",
                      i + 1, widsAttackTypeString(e.type), e.burstCount, ago,
                      srcMac.c_str(), srcVendor.c_str(),
                      dstMac.c_str(), dstVendor.c_str());
        Serial.printf("     +- Param: %d (%s) | RSSI: %ddBm | Duration: %lus | Notes: %s %s\n",
                      e.param, widsReasonString(e.param), e.rssi, duration,
                      e.customNotes, flags.c_str());
    }
    Serial.println("-------------------------------------------\n");
}