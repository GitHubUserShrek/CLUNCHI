#include "wifi_manager.h"
#include <WiFi.h>
#include <WiFiClient.h>
#include <DNSServer.h>
#include <ESPAsyncWebServer.h>
#include <Preferences.h>
#include <ArduinoJson.h>
#include "esp_wifi.h"
#include "portal.h"

APResult     scanResults[20];
int          scanCount    = 0;
bool         scanActive   = false;
ConnectState connectState = CONN_IDLE;
String       connectedSSID = "";

static bool     _wifiInitialised      = false;
static bool     _connected            = false;
static uint32_t _scanStartTime        = 0;
static uint32_t _connStartTime        = 0;
static uint32_t _connEstablishedTime  = 0;

static bool _justConnectedFlag    = false;
static bool _justDisconnectedFlag = false;

static DNSServer        dnsServer;
static AsyncWebServer*  portalServer        = nullptr;
static Preferences      portalPrefs;
static bool             portalActive        = false;
static bool             portalSaveAndReboot = false;
static bool             portalShouldClear   = false;
static String           portalPendingSSID   = "";
static String           portalPendingPass   = "";


bool wifiJustConnected() {
    if (_justConnectedFlag) { _justConnectedFlag = false; return true; }
    return false;
}

bool wifiJustDisconnected() {
    if (_justDisconnectedFlag) { _justDisconnectedFlag = false; return true; }
    return false;
}

static void wifiEventHandler(WiFiEvent_t event) {
    switch (event) {
    case ARDUINO_EVENT_WIFI_STA_GOT_IP:
        _connected           = true;
        _connEstablishedTime = millis();
        connectState         = CONN_SUCCESS;
        connectedSSID        = WiFi.SSID();
        _justConnectedFlag   = true;
        wifiPrintInfo();
        break;

    case ARDUINO_EVENT_WIFI_STA_DISCONNECTED:
        _connected            = false;
        _connEstablishedTime  = 0;
        connectedSSID         = "";
        _justDisconnectedFlag = true;
        if (connectState == CONN_TRYING) connectState = CONN_FAILED;
        else                             connectState = CONN_IDLE;
        Serial.println("[WiFi] Disconnected.");
        break;
    }
}

void wifiBegin() {
    if (_wifiInitialised || !WIFI_ENABLED) return;

    WiFi.onEvent(wifiEventHandler);
    WiFi.mode(WIFI_OFF);
    delay(100);
    WiFi.mode(WIFI_STA);
    WiFi.setTxPower(WIFI_POWER_8_5dBm);
    WiFi.setSleep(false);
    esp_wifi_set_protocol(WIFI_IF_STA,
        WIFI_PROTOCOL_11B | WIFI_PROTOCOL_11G | WIFI_PROTOCOL_11N);

    _wifiInitialised = true;
    Serial.println("[WiFi] Radio initialized.");
}

void wifiDeinit() {
    if (!_wifiInitialised) return;
    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);
    _wifiInitialised     = false;
    _connected           = false;
    _connEstablishedTime = 0;
    connectState         = CONN_IDLE;
    Serial.println("[WiFi] Radio deinitialized.");
}

bool isWifiInitialised() { return _wifiInitialised; }

void wifiConnect(const char* ssid) {
    if (!_wifiInitialised) wifiBegin();

    String savedSSID = wifiGetPortalSSID();
    if (savedSSID == "" || String(ssid) != savedSSID) {
        connectState = CONN_NO_CREDS;
        return;
    }

    portalPrefs.begin("wifi", true);
    String savedPass = portalPrefs.getString("pass", "");
    portalPrefs.end();

    WiFi.disconnect(true);
    delay(100);
    WiFi.mode(WIFI_STA);
    WiFi.setSleep(false);
    esp_wifi_set_ps(WIFI_PS_NONE);

    connectState   = CONN_TRYING;
    _connStartTime = millis();

    WiFi.begin(savedSSID.c_str(), savedPass.c_str());
    Serial.printf("[WiFi] Connecting to: %s\n", ssid);
}

void wifiDisconnect() {
    WiFi.disconnect();
    connectState         = CONN_IDLE;
    _connected           = false;
    _connEstablishedTime = 0;
}

void wifiStartScan() {
    if (!_wifiInitialised) wifiBegin();
    if (scanActive) return;

    WiFi.scanDelete();
    WiFi.mode(WIFI_STA);
    delay(50);

    scanActive     = true;
    _scanStartTime = millis();
    WiFi.scanNetworks(true, false, true);
    Serial.println("[WiFi] Scan started.");
}

void wifiCancelScan() {
    if (!_wifiInitialised) return;
    WiFi.scanDelete();
    scanActive = false;
}

uint32_t wifiScanStartTime() { return _scanStartTime; }

void wifiUpdate() {
    if (!_wifiInitialised && !scanActive) return;

    if (scanActive) {
        int n = WiFi.scanComplete();
        if (n >= 0) {
            scanCount = min(n, 20);
            String saved = wifiGetPortalSSID();

            Serial.printf("[WiFi] Scan complete — %d networks:\n", scanCount);
            for (int i = 0; i < scanCount; i++) {
                scanResults[i].ssid    = WiFi.SSID(i);
                scanResults[i].rssi    = WiFi.RSSI(i);
                scanResults[i].channel = WiFi.channel(i);
                scanResults[i].isOpen  = (WiFi.encryptionType(i) == WIFI_AUTH_OPEN);
                scanResults[i].isSaved = (saved.length() > 0 &&
                                          scanResults[i].ssid == saved);

                Serial.printf("  %2d: %-20s %d dBm ch:%d %s%s\n",
                    i + 1,
                    scanResults[i].ssid.c_str(),
                    (int)scanResults[i].rssi,
                    scanResults[i].channel,
                    scanResults[i].isOpen  ? "[OPEN]"  : "",
                    scanResults[i].isSaved ? "[SAVED]" : "");
            }
            WiFi.scanDelete();
            scanActive = false;
        }
        else if (n == WIFI_SCAN_FAILED) {
            scanActive = false;
            Serial.println("[WiFi] Scan failed.");
        }
    }

    if (connectState == CONN_TRYING) {
        if (WiFi.status() == WL_CONNECTED) {
            _connected   = true;
            connectState = CONN_SUCCESS;
            return;
        }
        if (millis() - _connStartTime > WIFI_CONNECT_TIMEOUT) {
            connectState = CONN_FAILED;
            WiFi.disconnect();
            Serial.println("[WiFi] Connection timeout.");
        }
    }
}

class CaptiveRequestHandler : public AsyncWebHandler {
public:
    bool canHandle(AsyncWebServerRequest *request) const override {
        if (request->url() == "/save"   ||
            request->url() == "/status" ||
            request->url() == "/clear") return false;
        return true;
    }
    void handleRequest(AsyncWebServerRequest *request) {
        request->send(200, "text/html", PORTAL_HTML);
    }
};

void wifiStartPortal() {
    if (portalActive) return;
    portalActive = true;

    WiFi.softAPdisconnect(true);
    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);
    delay(200);
    WiFi.mode(WIFI_AP);
    WiFi.setSleep(false);
    esp_wifi_set_ps(WIFI_PS_NONE);

    if (WiFi.softAP(WIFI_AP_NAME, WIFI_AP_PASS, 6)) {
        dnsServer.start(53, "*", WiFi.softAPIP());
        portalServer = new AsyncWebServer(80);

        String savedS = wifiGetPortalSSID();
        static String cachedStatusJSON;
        cachedStatusJSON = "{\"saved\":"
            + String(savedS.length() > 0 ? "true" : "false")
            + ",\"ssid\":\"" + savedS + "\"}";

        portalServer->on("/status", HTTP_GET,
            [](AsyncWebServerRequest *request) {
                request->send(200, "application/json", cachedStatusJSON);
            });

        portalServer->on("/clear", HTTP_POST,
            [](AsyncWebServerRequest *request) {
                request->send(200, "application/json", "{\"status\":\"ok\"}");
                portalShouldClear = true;
            });

        portalServer->on("/save", HTTP_POST,
            [](AsyncWebServerRequest *request) {}, NULL,
            [](AsyncWebServerRequest *request, uint8_t *data,
               size_t len, size_t index, size_t total) {
                JsonDocument doc;
                DeserializationError error = deserializeJson(doc, (const char*)data, len);
                if (!error) {
                    portalPendingSSID   = doc["ssid"].as<String>();
                    portalPendingPass   = doc["password"].as<String>();
                    portalSaveAndReboot = true;
                    request->send(200, "application/json",
                                  "{\"status\":\"ok\"}");
                }
            });

        portalServer->addHandler(new CaptiveRequestHandler())
                    .setFilter(ON_AP_FILTER);
        portalServer->begin();
        Serial.println("[Portal] Web server started.");
    }
}

void wifiStopPortal() {
    if (!portalActive) return;
    dnsServer.stop();
    if (portalServer) {
        portalServer->end();
        delete portalServer;
        portalServer = nullptr;
    }
    WiFi.softAPdisconnect(true);
    WiFi.mode(WIFI_OFF);
    portalActive = false;
}

void wifiProcessPortal() {
    if (!portalActive) return;
    dnsServer.processNextRequest();

    if (portalSaveAndReboot || portalShouldClear) {
        delay(1000);
        portalPrefs.begin("wifi", false);
        if (portalShouldClear) portalPrefs.clear();
        else {
            portalPrefs.putString("ssid", portalPendingSSID);
            portalPrefs.putString("pass", portalPendingPass);
        }
        portalPrefs.end();
        ESP.restart();
    }
}

bool     wifiIsPortalActive()   { return portalActive; }
bool     wifiConnected()        { return _connected; }
String   wifiIP()               { return _connected ? WiFi.localIP().toString()    : "0.0.0.0"; }
String   wifiCurrentSSID()      { return connectedSSID; }
String   wifiSubnetMask()       { return _connected ? WiFi.subnetMask().toString() : "0.0.0.0"; }
String   wifiGatewayIP()        { return _connected ? WiFi.gatewayIP().toString()  : "0.0.0.0"; }
String   wifiDNSIP()            { return _connected ? WiFi.dnsIP().toString()      : "0.0.0.0"; }
String   wifiMACAddress()       { return WiFi.macAddress(); }
String   wifiBSSID()            { return _connected ? WiFi.BSSIDstr() : "00:00:00:00:00:00"; }
int32_t  wifiRSSI()             { return _connected ? WiFi.RSSI() : 0; }
int      wifiConnectedChannel() { return _connected ? WiFi.channel() : 0; }
String   wifiHostname()         { return WiFi.getHostname(); }

uint32_t wifiConnUptime() {
    if (!_connected || _connEstablishedTime == 0) return 0;
    return (millis() - _connEstablishedTime) / 1000;
}

bool wifiHasPortalCredentials() { return wifiGetPortalSSID().length() > 0; }

String wifiGetPortalSSID() {
    portalPrefs.begin("wifi", true);
    String s = portalPrefs.getString("ssid", "");
    portalPrefs.end();
    return s;
}

void wifiClearPortalCredentials() {
    portalPrefs.begin("wifi", false);
    portalPrefs.clear();
    portalPrefs.end();
}

float wifiTxPower() {
    int8_t pwr;
    esp_wifi_get_max_tx_power(&pwr);
    return pwr / 4.0f;
}

String wifiUptimeFormatted() {
    uint32_t s = wifiConnUptime();
    return String(s / 3600) + "h " +
           String((s % 3600) / 60) + "m " +
           String(s % 60) + "s";
}

int wifiSignalPercent() {
    int32_t r = wifiRSSI();
    if (r >= -50)  return 100;
    if (r <= -100) return 0;
    return 2 * (r + 100);
}

String wifiSignalLabel() {
    int32_t r = wifiRSSI();
    if (r >= -50) return "EXCELLENT";
    if (r >= -60) return "GOOD";
    if (r >= -70) return "FAIR";
    if (r >= -80) return "WEAK";
    return "CRITICAL";
}

void wifiPrintInfo() {
    if (!wifiConnected()) {
        Serial.println("[WiFi] Not connected.");
        return;
    }

    Serial.println();
    Serial.println("[WiFi] ═══════════════════════════════════════════");
    Serial.printf("[WiFi]  SSID:    %s\n",    wifiCurrentSSID().c_str());
    Serial.printf("[WiFi]  IP:      %s\n",    wifiIP().c_str());
    Serial.printf("[WiFi]  Subnet:  %s\n",    wifiSubnetMask().c_str());
    Serial.printf("[WiFi]  Gateway: %s\n",    wifiGatewayIP().c_str());
    Serial.printf("[WiFi]  DNS:     %s\n",    wifiDNSIP().c_str());
    Serial.printf("[WiFi]  MAC:     %s\n",    wifiMACAddress().c_str());
    Serial.printf("[WiFi]  BSSID:   %s\n",    wifiBSSID().c_str());
    Serial.printf("[WiFi]  RSSI:    %d dBm (%s)\n",
                  (int)wifiRSSI(), wifiSignalLabel().c_str());
    Serial.printf("[WiFi]  Signal:  %d%%\n",  wifiSignalPercent());
    Serial.printf("[WiFi]  Channel: %d\n",    wifiConnectedChannel());
    Serial.printf("[WiFi]  TX Pwr:  %.1f dBm\n", wifiTxPower());
    Serial.printf("[WiFi]  Host:    %s\n",    wifiHostname().c_str());
    Serial.printf("[WiFi]  Uptime:  %s\n",    wifiUptimeFormatted().c_str());
    Serial.println("[WiFi] ═══════════════════════════════════════════");
    Serial.println();
}