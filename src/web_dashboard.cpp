#include "web_dashboard.h"
#include "wifi_manager.h"
#include "deauth_detector.h"
#include "net_health.h"
#include <WebServer.h>
#include <esp_wifi.h>
#include <WiFi.h>

static WebServer* _server = nullptr;
static bool _active = false;

// ── Connection history ───────────────────────────────
#define MAX_CONN_EVENTS 10
struct ConnEvent {
    uint32_t timestamp;
    bool     connected;
    int8_t   rssi;
};
static ConnEvent _connHistory[MAX_CONN_EVENTS];
static int  _connHistoryCount = 0;
static bool _wasConnected     = false;

// ── DNS latency ──────────────────────────────────────
static uint32_t _lastDnsLatency = 0;

// ── Nearby networks cache ────────────────────────────
#define MAX_NEARBY 10
struct NearbyAP {
    String  ssid;
    int8_t  rssi;
    uint8_t channel;
    bool    isOpen;
};
static NearbyAP  _nearbyAPs[MAX_NEARBY];
static int       _nearbyCount    = 0;
static uint32_t  _lastNearbyScan = 0;


// ── DNS latency measurement ──────────────────────────
static void measureDnsLatency() {
    if (!wifiConnected()) { _lastDnsLatency = 0; return; }
    IPAddress result;
    uint32_t start   = millis();
    int      err     = WiFi.hostByName("google.com", result);
    uint32_t elapsed = millis() - start;
    _lastDnsLatency  = (err == 1) ? elapsed : 9999;
}

// ── Background scan for nearby networks ──────────────
static void updateNearbyScan() {
    if (!wifiConnected()) return;
    if (millis() - _lastNearbyScan < 60000 && _lastNearbyScan != 0) return;

    int n = WiFi.scanComplete();
    if (n == WIFI_SCAN_FAILED) {
        WiFi.scanNetworks(true, false, true);
        _lastNearbyScan = millis();
        return;
    }
    if (n < 0) return;

    _nearbyCount = min(n, MAX_NEARBY);
    for (int i = 0; i < _nearbyCount; i++) {
        _nearbyAPs[i].ssid    = WiFi.SSID(i);
        _nearbyAPs[i].rssi    = WiFi.RSSI(i);
        _nearbyAPs[i].channel = WiFi.channel(i);
        _nearbyAPs[i].isOpen  = (WiFi.encryptionType(i) == WIFI_AUTH_OPEN);
    }
    WiFi.scanDelete();
    _lastNearbyScan = millis();
}

// ── Track connection events ──────────────────────────
static void trackConnection() {
    bool now = wifiConnected();
    if (now != _wasConnected) {
        if (_connHistoryCount < MAX_CONN_EVENTS) {
            _connHistory[_connHistoryCount].timestamp = millis() / 1000;
            _connHistory[_connHistoryCount].connected = now;
            _connHistory[_connHistoryCount].rssi      = now ? (int8_t)wifiRSSI() : 0;
            _connHistoryCount++;
        }
        _wasConnected = now;
    }
}

// ── Pause/Resume background tasks ───────────────────
static void pauseBackground() {
    if (deauthDetectorActive) deauthDetectorEnd();
    netHealthEnd();
    esp_wifi_set_promiscuous(false);
    delay(50);
}

static void resumeBackground() {
    if (wifiConnected()) {
        deauthDetectorBegin();
        netHealthBegin();
    }
}

// ── WiFi security helpers ────────────────────────────
static String getSecurityType() {
    wifi_ap_record_t info;
    if (esp_wifi_sta_get_ap_info(&info) != ESP_OK) return "UNKNOWN";
    switch (info.authmode) {
        case WIFI_AUTH_OPEN:            return "OPEN (NONE)";
        case WIFI_AUTH_WEP:             return "WEP";
        case WIFI_AUTH_WPA_PSK:         return "WPA-PSK";
        case WIFI_AUTH_WPA2_PSK:        return "WPA2-PSK";
        case WIFI_AUTH_WPA_WPA2_PSK:    return "WPA/WPA2-PSK";
        case WIFI_AUTH_WPA3_PSK:        return "WPA3-PSK";
        case WIFI_AUTH_WPA2_WPA3_PSK:   return "WPA2/WPA3-PSK";
        case WIFI_AUTH_WPA2_ENTERPRISE: return "WPA2-ENT";
        default:                        return "OTHER";
    }
}

static String getCipherType() {
    wifi_ap_record_t info;
    if (esp_wifi_sta_get_ap_info(&info) != ESP_OK) return "UNKNOWN";
    switch (info.pairwise_cipher) {
        case WIFI_CIPHER_TYPE_NONE:      return "NONE";
        case WIFI_CIPHER_TYPE_WEP40:     return "WEP40";
        case WIFI_CIPHER_TYPE_WEP104:    return "WEP104";
        case WIFI_CIPHER_TYPE_TKIP:      return "TKIP";
        case WIFI_CIPHER_TYPE_CCMP:      return "AES-CCMP";
        case WIFI_CIPHER_TYPE_TKIP_CCMP: return "TKIP+CCMP";
        default:                         return "OTHER";
    }
}

// ── RSSI to classic 5-bar level ──────────────────────
static int rssiBars(int32_t rssi) {
    if (rssi >= -50) return 5;
    if (rssi >= -60) return 4;
    if (rssi >= -67) return 3;
    if (rssi >= -75) return 2;
    if (rssi >= -85) return 1;
    return 0;
}

// ── Build 5-bar HTML ─────────────────────────────────
static String buildSignalBars(int32_t rssi) {
    int bars = rssiBars(rssi);
    String html = "<div class='bars'>";
    for (int i = 1; i <= 5; i++) {
        int h = 4 + (i * 1);  // heights: 7, 10, 13, 16, 19
        String color = (i <= bars) ?
            ((bars >= 4) ? "#0f0" : (bars >= 2) ? "#ff0" : "#f00") : "#222";
        html += "<div class='bar' style='height:" + String(h) +
                "px;background:" + color + ";'></div>";
    }
    html += "</div>";
    return html;
}


// ── HTML header ──────────────────────────────────────
static void sendHtmlHeader() {
    _server->sendContent(F("<!DOCTYPE html><html><head>"));
    _server->sendContent(F("<title>CLUNCHI ANALYZER</title>"));
    _server->sendContent(F("<meta name='viewport' content='width=device-width,initial-scale=1'>"));
    _server->sendContent(F("<style>"));
    _server->sendContent(F("*{margin:0;padding:0;box-sizing:border-box}"));
    _server->sendContent(F("body{font-family:'Courier New',monospace;background:#000;color:#0f0;text-align:center;padding:30px 15px;min-height:100vh;}"));
    _server->sendContent(F(".box{max-width:420px;margin:0 auto;border:2px solid #0f0;padding:20px;background:#000;}"));
    _server->sendContent(F("h2{font-size:26px;letter-spacing:6px;margin-bottom:4px;text-shadow:0 0 10px #0f0;}"));
    _server->sendContent(F(".subtitle{font-size:10px;color:#080;letter-spacing:3px;margin-bottom:20px;}"));
    _server->sendContent(F(".sec{background:#111;border:1px dashed #0f0;padding:14px;margin:12px 0;text-align:left;}"));
    _server->sendContent(F(".sec-h{font-size:9px;color:#080;letter-spacing:2px;margin-bottom:8px;display:block;}"));
    _server->sendContent(F(".r{display:flex;justify-content:space-between;padding:4px 0;border-bottom:1px dotted #040;font-size:12px;}"));
    _server->sendContent(F(".r:last-child{border-bottom:none;}"));
    _server->sendContent(F(".v{color:#0f0;}"));
    _server->sendContent(F("table{width:100%;font-size:10px;border-collapse:collapse;margin-top:8px;}"));
    _server->sendContent(F("th{text-align:left;color:#080;border-bottom:1px solid #0f0;padding-bottom:3px;letter-spacing:1px;}"));
    _server->sendContent(F("td{padding:3px 0;color:#0f0;}"));
    _server->sendContent(F(".warn{color:#f00;} .ok{color:#0f0;} .med{color:#ff0;}"));

    // Classic signal bars
    _server->sendContent(F(".bar{width:4px;border-radius:1px;}"));

    // Signal row with bars inline
    _server->sendContent(F(".sig-row{display:flex;justify-content:space-between;align-items:center;padding:4px 0;border-bottom:1px dotted #040;font-size:12px;}"));
    _server->sendContent(F(".sig-right{display:flex;align-items:center;}"));
    _server->sendContent(F(".sig-right span{display:inline-block;line-height:12px;}"));
    _server->sendContent(F(".bars{display:inline-flex;align-items:flex-end;gap:2px;margin-left:6px;transform:translateY(-1px);}"));

    // Button
    _server->sendContent(F(".btn{display:block;width:100%;padding:16px;margin:10px 0;background:#000;color:#0f0;border:2px solid #0f0;"));
    _server->sendContent(F("font:bold 16px monospace;cursor:pointer;text-transform:uppercase;letter-spacing:2px;text-decoration:none;}"));
    _server->sendContent(F(".btn:hover{background:#0f0;color:#000;}"));
    _server->sendContent(F(".btn:active{background:#080;}"));

    // Footer
    _server->sendContent(F(".footer{font-size:9px;color:#040;margin-top:30px;letter-spacing:1px;opacity:0.7;}"));
    _server->sendContent(F(".empty{font-size:10px;color:#040;padding:5px;}"));
    _server->sendContent(F(".alert-box{font-size:10px;color:#f00;padding:5px;margin-top:5px;border:1px solid #f00;}"));

    _server->sendContent(F("</style></head><body>"));
    _server->sendContent(F("<div class='box'>"));
    _server->sendContent(F("<h2>CLUNCHI</h2>"));
    _server->sendContent(F("<div class='subtitle'>NETWORK ANALYZER v2.0</div>"));
}


// ── Main page handler ────────────────────────────────
static void handleRoot() {
    pauseBackground();
    measureDnsLatency();

    _server->sendHeader("Cache-Control", "no-cache");
    _server->sendHeader("Connection", "close");
    _server->setContentLength(CONTENT_LENGTH_UNKNOWN);
    _server->send(200, "text/html", "");

    sendHtmlHeader();

    auto sendRow = [](const char* label, String value, bool isWarn = false) {
        String css = isWarn ? "v warn" : "v";
        _server->sendContent(
            "<div class='r'><span>" + String(label) +
            "</span><span class='" + css + "'>" + value + "</span></div>");
    };

    // ── WIFI STACK (moved to top, includes signal) ───
    int32_t rssi = wifiRSSI();

    _server->sendContent(F("<div class='sec'>"));
    _server->sendContent(F("<span class='sec-h'>// WIFI STACK</span>"));
    sendRow("SSID",    wifiCurrentSSID());
    sendRow("BSSID",   wifiBSSID());
    sendRow("Channel", String(wifiConnectedChannel()));

    // RSSI row with classic bars
    _server->sendContent(
        "<div class='sig-row'><span>RSSI</span>"
        "<div class='sig-right'><span class='" +
        String(rssi < -75 ? "v warn" : "v") + "'>" +
        String(rssi) + " dBm</span>" +
        buildSignalBars(rssi) + "</div></div>");

    sendRow("Quality", wifiSignalLabel());
    _server->sendContent(F("</div>"));

    // ── L2 SECURITY ──────────────────────────────────
    _server->sendContent(F("<div class='sec'>"));
    _server->sendContent(F("<span class='sec-h'>// L2 SECURITY</span>"));

    uint8_t threat   = deauthThreatScore();
    bool    underAtk = deauthUnderAttack();
    String  threatLbl = (threat >= 70) ? "HIGH" : (threat >= 30) ? "MEDIUM" : "LOW";

    sendRow("Total Alerts",   String(deauthTotalCount),         deauthTotalCount > 0);
    sendRow("Recent (10s)",   String(deauthRecentCount(10000)), deauthRecentCount(10000) > 0);
    sendRow("Unique Sources", String(deauthUniqueSourceCount()));
    sendRow("Threat Score",   String(threat) + "/100 [" + threatLbl + "]", threat >= 30);
    sendRow("Under Attack",   underAtk ? "YES" : "NO", underAtk);

    int topIdx = deauthMostActiveSourceIndex();
    if (topIdx >= 0) {
        sendRow("Top Source", deauthMacToString(deauthLog[topIdx].source), true);
    }

    if (underAtk) {
        _server->sendContent(F("<div class='alert-box'>&#9888; ACTIVE DEAUTH ATTACK DETECTED</div>"));
    }

    if (deauthLogCount > 0) {
        _server->sendContent(F("<table><tr><th>TYPE</th><th>SOURCE</th><th>TARGET</th><th>RSSI</th><th>REASON</th></tr>"));
        for (int i = deauthLogCount - 1; i >= max(0, deauthLogCount - 5); i--) {
            DeauthEvent& e = deauthLog[i];
            String rowCss  = e.isTargeted ? "warn" : "ok";
            _server->sendContent("<tr class='" + rowCss + "'><td>");
            _server->sendContent(e.isDisassoc ? "DIS" : "DEA");
            _server->sendContent("</td><td>");
            _server->sendContent(deauthMacToString(e.source).substring(9));
            _server->sendContent("</td><td>");
            _server->sendContent(e.isTargeted ? "YOU" : "AIR");
            _server->sendContent("</td><td>");
            _server->sendContent(String(e.rssi));
            _server->sendContent("</td><td>");
            _server->sendContent(String(deauthReasonString(e.reasonCode)));
            _server->sendContent("</td></tr>");
        }
        _server->sendContent(F("</table>"));
    } else {
        _server->sendContent(F("<div class='empty'>NO MALICIOUS FRAMES DETECTED.</div>"));
    }
    _server->sendContent(F("</div>"));

    // ── L3 CONNECTIVITY ──────────────────────────────
    _server->sendContent(F("<div class='sec'>"));
    _server->sendContent(F("<span class='sec-h'>// L3 CONNECTIVITY</span>"));
    sendRow("Internet", netHealthIsUp ? "ONLINE" : "OFFLINE", !netHealthIsUp);
    uint32_t lat = netHealthAvgLatency();
    sendRow("Avg Latency", (lat > 0 ? String(lat) + " ms" : "---"), lat > 200);
    sendRow("DNS Lookup",
            (_lastDnsLatency == 9999 ? "FAILED" :
            (_lastDnsLatency > 0     ? String(_lastDnsLatency) + " ms" : "---")),
            _lastDnsLatency > 500 || _lastDnsLatency == 9999);
    sendRow("Fail Count", String(netHealthConsecutiveFails), netHealthConsecutiveFails > 0);
    _server->sendContent(F("</div>"));

    // ── WIFI SECURITY ────────────────────────────────
    _server->sendContent(F("<div class='sec'>"));
    _server->sendContent(F("<span class='sec-h'>// WIFI SECURITY</span>"));
    String secType = getSecurityType();
    bool   weakSec = (secType.indexOf("OPEN") >= 0 || secType.indexOf("WEP") >= 0);
    sendRow("Auth Mode", secType, weakSec);
    sendRow("Cipher",    getCipherType());
    if (weakSec) {
        _server->sendContent(F("<div class='alert-box'>&#9888; WEAK SECURITY — UPGRADE RECOMMENDED</div>"));
    }
    _server->sendContent(F("</div>"));

    // ── NEARBY NETWORKS ──────────────────────────────
    if (_nearbyCount > 0) {
        _server->sendContent(F("<div class='sec'>"));
        _server->sendContent(F("<span class='sec-h'>// NEARBY NETWORKS</span>"));
        _server->sendContent(F("<table><tr><th>SSID</th><th>RSSI</th><th>CH</th><th>SEC</th></tr>"));
        for (int i = 0; i < _nearbyCount; i++) {
            String rssiCss = (_nearbyAPs[i].rssi > -60) ? "ok" :
                             (_nearbyAPs[i].rssi > -75) ? "med" : "warn";
            _server->sendContent("<tr><td>" + _nearbyAPs[i].ssid + "</td>");
            _server->sendContent("<td class='" + rssiCss + "'>" + String(_nearbyAPs[i].rssi) + "</td>");
            _server->sendContent("<td>" + String(_nearbyAPs[i].channel) + "</td>");
            _server->sendContent(String("<td>") +
                (_nearbyAPs[i].isOpen ? "<span class='warn'>OPEN</span>" : "ENC") +
                "</td></tr>");
        }
        _server->sendContent(F("</table>"));
        _server->sendContent(F("</div>"));
    }

    // ── IP CONFIG ────────────────────────────────────
    _server->sendContent(F("<div class='sec'>"));
    _server->sendContent(F("<span class='sec-h'>// IP CONFIG</span>"));
    sendRow("Local IP",  wifiIP());
    sendRow("Gateway",   wifiGatewayIP());
    sendRow("DNS",       wifiDNSIP());
    sendRow("Subnet",    wifiSubnetMask());
    sendRow("MAC",       wifiMACAddress());
    sendRow("Hostname",  wifiHostname());
    _server->sendContent(F("</div>"));

    // ── CONNECTION HISTORY ───────────────────────────
    if (_connHistoryCount > 0) {
        _server->sendContent(F("<div class='sec'>"));
        _server->sendContent(F("<span class='sec-h'>// CONNECTION HISTORY</span>"));
        _server->sendContent(F("<table><tr><th>TIME</th><th>EVENT</th><th>RSSI</th></tr>"));
        for (int i = _connHistoryCount - 1; i >= max(0, _connHistoryCount - 8); i--) {
            uint32_t t = _connHistory[i].timestamp;
            String   timeStr = String(t / 3600) + "h " +
                               String((t % 3600) / 60) + "m " +
                               String(t % 60) + "s";
            String   evCss = _connHistory[i].connected ? "ok" : "warn";
            _server->sendContent("<tr class='" + evCss + "'><td>" + timeStr + "</td>");
            _server->sendContent(String("<td>") +
                (_connHistory[i].connected ? "CONNECT" : "DROP") + "</td>");
            _server->sendContent("<td>" + String(_connHistory[i].rssi) + "</td></tr>");
        }
        _server->sendContent(F("</table>"));
        _server->sendContent(F("</div>"));
    }

    // ── HARDWARE ─────────────────────────────────────
    _server->sendContent(F("<div class='sec'>"));
    _server->sendContent(F("<span class='sec-h'>// HARDWARE</span>"));
    sendRow("Free Heap",   String(ESP.getFreeHeap()    / 1024) + " KB");
    sendRow("Min Heap",    String(ESP.getMinFreeHeap() / 1024) + " KB");
    sendRow("Chip Model",  String(ESP.getChipModel()));
    sendRow("CPU Freq",    String(ESP.getCpuFreqMHz())  + " MHz");
    sendRow("Flash Size",  String(ESP.getFlashChipSize() / 1024) + " KB");
    sendRow("TX Power",    String(wifiTxPower(), 1) + " dBm");
    sendRow("SDK Version", String(ESP.getSdkVersion()));
    sendRow("Uptime",      wifiUptimeFormatted());
    _server->sendContent(F("</div>"));

    // ── Refresh + footer ─────────────────────────────
    _server->sendContent(F("<div class='footer'>CLUNCHI ANALYZER BETA v1.0</div>"));

    _server->sendContent(F("</div></body></html>"));
    _server->sendContent("");

    delay(200);
    resumeBackground();
}


// ── JSON API ─────────────────────────────────────────
static void handleAPI() {
    String json = "{";
    json += "\"rssi\":"          + String(wifiRSSI())             + ",";
    json += "\"signal_pct\":"    + String(wifiSignalPercent())    + ",";
    json += "\"signal_bars\":"   + String(rssiBars(wifiRSSI()))   + ",";
    json += "\"ssid\":\""        + wifiCurrentSSID()              + "\",";
    json += "\"ip\":\""          + wifiIP()                       + "\",";
    json += "\"gateway\":\""     + wifiGatewayIP()                + "\",";
    json += "\"online\":"        + String(netHealthIsUp ? "true" : "false") + ",";
    json += "\"latency\":"       + String(netHealthAvgLatency())  + ",";
    json += "\"dns_latency\":"   + String(_lastDnsLatency)        + ",";
    json += "\"deauth_count\":"  + String(deauthTotalCount)       + ",";
    json += "\"threat_score\":"  + String(deauthThreatScore())    + ",";
    json += "\"under_attack\":"  + String(deauthUnderAttack() ? "true" : "false") + ",";
    json += "\"uptime\":"        + String(wifiConnUptime())       + ",";
    json += "\"heap\":"          + String(ESP.getFreeHeap())      + ",";
    json += "\"security\":\""    + getSecurityType()              + "\"";
    json += "}";

    _server->sendHeader("Access-Control-Allow-Origin", "*");
    _server->send(200, "application/json", json);
}


// ── Lifecycle ────────────────────────────────────────
void dashboardBegin() {
    if (_active) return;
    _server = new WebServer(80);
    _server->on("/",    HTTP_GET, handleRoot);
    _server->on("/api", HTTP_GET, handleAPI);
    _server->begin();
    _active       = true;
    _wasConnected = wifiConnected();
}

void dashboardEnd() {
    if (!_active) return;
    if (_server) { _server->stop(); delete _server; _server = nullptr; }
    _active = false;
}

void dashboardUpdate() {
    if (_active && _server) {
        _server->handleClient();
        trackConnection();
        updateNearbyScan();
    }
}

bool isDashboardActive() { return _active; }