#include "ble_manager.h"
#include "config.h"
#include <NimBLEDevice.h>

struct OUIEntry {
    const char* prefix;
    const char* manufacturer;
};

static const OUIEntry ouiTable[] = {
    { "80:7d:3a", "Flipper Devices" },
    { "80:e1:26", "Flipper Devices" },
};
static const int ouiCount = sizeof(ouiTable) / sizeof(ouiTable[0]);

struct BLEAlertRule {
    const char* nameContains;
    const char* ouiPrefix;
    const char* serviceUUID;
    const char* label;
};

static const BLEAlertRule alertRules[] = {
    { "flipper",  nullptr,    nullptr, "Flipper Zero"        },
    { nullptr,    "80:7d:3a", nullptr, "Flipper Zero (OUI)"  },
    { nullptr,    "80:e1:26", nullptr, "Flipper Zero (OUI)"  },
    { nullptr,    nullptr,    "3082",  "Flipper Zero (UUID)" },
};
static const int alertRuleCount = sizeof(alertRules) / sizeof(alertRules[0]);

BLEResult bleResults[20];
int       bleCount      = 0;
bool      bleScanActive = false;

static bool           _bleInitialised = false;
static uint32_t       _scanStartTime  = 0;
static NimBLEScan*    _pScan          = nullptr;
static bool           _radarMode      = false;
static const uint32_t RADAR_SWEEP_MS  = 5000;

static String lookupOUI(const String& addr) {
    String prefix = addr.substring(0, 8);
    prefix.toLowerCase();
    for (int i = 0; i < ouiCount; i++) {
        if (prefix == ouiTable[i].prefix) return ouiTable[i].manufacturer;
    }
    return "Unknown";
}

static bool checkAlert(const String& addr, const String& name,
                       const NimBLEAdvertisedDevice* device,
                       String& outLabel) {
    String prefix   = addr.substring(0, 8);
    prefix.toLowerCase();
    String nameLower = name;
    nameLower.toLowerCase();

    static String uuids[5];
    int uuidCount = 0;

    int svcCount = device->getServiceUUIDCount();
    for (int u = 0; u < svcCount && uuidCount < 5; u++) {
        String ustr = device->getServiceUUID(u).toString().c_str();
        ustr.toLowerCase();
        bool dup = false;
        for (int d = 0; d < uuidCount; d++) {
            if (uuids[d] == ustr) { dup = true; break; }
        }
        if (!dup) uuids[uuidCount++] = ustr;
    }

    for (int i = 0; i < alertRuleCount; i++) {
        bool nameMatch = alertRules[i].nameContains &&
                         nameLower.indexOf(alertRules[i].nameContains) >= 0;
        bool ouiMatch  = alertRules[i].ouiPrefix &&
                         prefix == alertRules[i].ouiPrefix;
        bool uuidMatch = false;
        if (alertRules[i].serviceUUID) {
            String t = alertRules[i].serviceUUID;
            t.toLowerCase();
            for (int u = 0; u < uuidCount; u++) {
                if (uuids[u].indexOf(t) >= 0) { uuidMatch = true; break; }
            }
        }
        if (nameMatch || ouiMatch || uuidMatch) {
            outLabel = alertRules[i].label;
            return true;
        }
    }
    return false;
}

class ScanCallback : public NimBLEScanCallbacks {
public:
    void onResult(const NimBLEAdvertisedDevice* device) override {
        if (!device) return; 

        String addr = device->getAddress().toString().c_str();

        for (int i = 0; i < bleCount; i++) {
            if (bleResults[i].address == addr) {
                bleResults[i].rssi = device->getRSSI();
                return;
            }
        }

        if (bleCount >= 20) return;

        String  name     = device->haveName() ? device->getName().c_str() : "";
        int     rssi     = device->getRSSI();
        uint8_t addrType = device->getAddress().getType();
        bool    isPublic = (addrType == 0 || addrType == 2);

        String manufacturer = isPublic ? lookupOUI(addr) : "Random MAC";

        String alertLabel = "";
        bool   isAlert    = checkAlert(addr, name, device, alertLabel);

        BLEResult& r  = bleResults[bleCount];
        r.address      = addr;
        r.name         = name;
        r.rssi         = rssi;
        r.manufacturer = manufacturer;
        r.deviceType   = isPublic ? "Public" : "Random/Private";
        r.isAlert      = isAlert;
        r.alertLabel   = alertLabel;
        r.isPublicAddr = isPublic;
        r.isKnown      = false;
        bleCount++;

        if (isAlert) {
            Serial.printf("[BLE] !!! ALERT: %s [%s] %s | %d dBm\n",
                addr.c_str(), alertLabel.c_str(), manufacturer.c_str(), rssi);
        } else if (!_radarMode) {
            Serial.printf("[BLE] Found: %s | %s | %d dBm\n",
                addr.c_str(), manufacturer.c_str(), rssi);
        }
    }

    void onScanEnd(const NimBLEScanResults& results, int reason) override {
        Serial.printf("[BLE] Scan ended (reason %d, %d devices)\n",
                      reason, bleCount);
        bleScanActive = false;
    }
};

static ScanCallback _scanCallback;

void bleBegin() {
    if (_bleInitialised) return;

    Serial.println("[BLE] Radio Initializing...");

    NimBLEDevice::init("CLUNCHI");

    _pScan = NimBLEDevice::getScan();
    if (!_pScan) {
        Serial.println("[BLE] ERROR: getScan() returned null!");
        return;
    }

    _pScan->setScanCallbacks(&_scanCallback, false);
    _pScan->setActiveScan(true);
    _pScan->setInterval(100);
    _pScan->setWindow(99);

    _bleInitialised = true;
    bleScanActive   = false;
    bleCount        = 0;

    Serial.println("[BLE] Radio Ready.");
}

void bleDeinit() {
    if (!_bleInitialised) return;

    Serial.println("[BLE] Deinitializing Radio...");

    if (_pScan) {
        if (bleScanActive) {
            _pScan->stop();
            delay(100);          
        }
        _pScan = nullptr;       
    }

    bleScanActive   = false;
    _radarMode      = false;

    NimBLEDevice::deinit(true);

    _bleInitialised = false;
    bleCount        = 0;

    Serial.println("[BLE] Radio Offline.");
}

bool isBleInitialised() { return _bleInitialised; }

void bleStartScan() {
    if (!_bleInitialised) {
        Serial.println("[BLE] Error: Call bleBegin first.");
        return;
    }
    if (bleScanActive) return;
    if (!_pScan) {
        Serial.println("[BLE] Error: _pScan is null!");
        return;
    }

    Serial.println("[BLE] Scanning started...");

    bleCount       = 0;
    _scanStartTime = millis();
    bleScanActive  = true;

    _pScan->clearResults();

    bool ok = _pScan->start(0, false); 
    if (!ok) {
        Serial.println("[BLE] ERROR: scan start failed!");
        bleScanActive = false;
    }
}

void bleStopScan() {
    if (!_pScan || !bleScanActive) return;
    _pScan->stop();
    bleScanActive = false;
    Serial.println("[BLE] Scanning stopped.");
}

void bleCancelScan() {
    if (!_bleInitialised) return;
    if (_pScan && bleScanActive) {
        _pScan->stop();
        delay(50);
        _pScan->clearResults();
    }
    bleScanActive = false;
    bleCount      = 0;
}

uint32_t bleScanStartTime() { return _scanStartTime; }

void bleStartRadar() {
    if (!_bleInitialised) bleBegin();
    if (!_pScan) return;

    _radarMode     = true;
    bleCount       = 0;
    _scanStartTime = millis();
    bleScanActive  = true;

    _pScan->clearResults();
    _pScan->start(0, false);

    Serial.println("[BLE] Radar scanning started.");
}

void bleStopRadar() {
    _radarMode = false;
    if (_pScan && bleScanActive) {
        _pScan->stop();
        delay(50);
    }
    bleScanActive = false;
    bleCount      = 0;
    Serial.println("[BLE] Radar scanning stopped.");
}

void bleForceSweep() {
    if (!_bleInitialised || !_radarMode || !_pScan) return;
    if (bleScanActive) {
        _pScan->stop();
        delay(50);
    }
    bleScanActive  = false;
    bleCount       = 0;
    _scanStartTime = millis();

    _pScan->clearResults();
    bleScanActive = true;
    _pScan->start(0, false);

    Serial.println("[BLE] Forced sweep started.");
}

bool isBleRadarActive() { return _radarMode; }

void bleUpdate() {
    if (!_bleInitialised || !_pScan) return;

    if (_radarMode) {
        if (bleScanActive && millis() - _scanStartTime > RADAR_SWEEP_MS) {
            _pScan->stop();
            // bleScanActive set false by onScanEnd callback
            // but set here too as safety net
            bleScanActive = false;
            Serial.printf("[BLE Radar] Sweep done: %d devices, %d alerts\n",
                          bleCount, bleAlertCount());
        }
        if (!bleScanActive) {
            bleCount       = 0;
            _scanStartTime = millis();
            bleScanActive  = true;
            _pScan->clearResults();
            _pScan->start(0, false);
        }
        return;
    }

    if (!bleScanActive) return;

    if (millis() - _scanStartTime > BLE_SCAN_DURATION) {
        _pScan->stop();
        bleScanActive = false;
        blePrintInfo();
        if (bleHasAlerts()) blePrintAlerts();
    }
}

int bleAlertCount() {
    int count = 0;
    for (int i = 0; i < bleCount; i++) if (bleResults[i].isAlert) count++;
    return count;
}

bool bleHasAlerts() { return bleAlertCount() > 0; }

void blePrintInfo() {
    if (bleCount == 0) { Serial.println("[BLE] No devices found."); return; }

    Serial.println("\n[BLE] ==========================================");
    Serial.printf("[BLE]  Scan Complete: %d device%s found\n",
                  bleCount, bleCount == 1 ? "" : "s");
    Serial.println("[BLE] ==========================================");

    for (int i = 0; i < bleCount; i++) {
        const BLEResult& r = bleResults[i];
        Serial.printf("[BLE]  %2d: %s\n", i + 1, r.address.c_str());
        Serial.printf("[BLE]      Name: %s\n",  r.name.isEmpty() ? "?" : r.name.c_str());
        Serial.printf("[BLE]      Mfr:  %s\n",  r.manufacturer.c_str());
        Serial.printf("[BLE]      Type: %s\n",  r.deviceType.c_str());
        Serial.printf("[BLE]      RSSI: %d dBm\n", r.rssi);
        Serial.printf("[BLE]      Addr: %s%s%s\n",
            r.isPublicAddr ? "Public" : "Random",
            r.isKnown      ? " [KNOWN]"  : "",
            r.isAlert      ? " [!ALERT]" : "");
    }
    Serial.println("[BLE] ==========================================\n");
}

void blePrintAlerts() {
    int alerts = bleAlertCount();
    if (alerts == 0) return;

    Serial.printf("[BLE] !!! %d ALERT%s DETECTED !!!\n",
                  alerts, alerts == 1 ? "" : "S");
    Serial.println("[BLE] ------------------------------------------");

    for (int i = 0; i < bleCount; i++) {
        if (!bleResults[i].isAlert) continue;
        const BLEResult& r = bleResults[i];
        Serial.printf("[BLE] ! %s\n",        r.alertLabel.c_str());
        Serial.printf("[BLE] !   Addr: %s\n", r.address.c_str());
        Serial.printf("[BLE]  !   Name: %s\n", r.name.isEmpty() ? "?" : r.name.c_str());
        Serial.printf("[BLE] !   Mfr:  %s\n", r.manufacturer.c_str());
        Serial.printf("[BLE] !   RSSI: %d dBm\n", r.rssi);
        Serial.println("[BLE] !");
    }
    Serial.println("[BLE] ------------------------------------------\n");
}