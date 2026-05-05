#include "ble_manager.h"
#include "oui_lookup.h"
#include "config.h"
#include <NimBLEDevice.h>
#include "sd_manager.h"
#include "gps_manager.h"
#include <SD.h>

struct BLEAlertRule {
    const char* nameContains;
    const char* ouiPrefix;
    const char* serviceUUID;
    const char* label;
};

static const BLEAlertRule alertRules[] = {
    { "flipper",  nullptr,    nullptr, "Flipper Zero"               },
    { nullptr,    nullptr,    "3081",  "Flipper Zero (Black)"       },
    { nullptr,    nullptr,    "3082",  "Flipper Zero (White)"       },
    { nullptr,    nullptr,    "3083",  "Flipper Zero (Transparent)" },
    { nullptr,    "80:7d:3a", nullptr, "Flipper Zero (OUI)"         },
    { nullptr,    "80:e1:26", nullptr, "Flipper Zero (OUI)"         },
    { nullptr,    "80:e1:27", nullptr, "Flipper Zero (OUI)"         },
    { nullptr,    "0c:fa:22", nullptr, "Flipper Zero (OUI)"         },
};

static const int alertRuleCount = sizeof(alertRules) / sizeof(alertRules[0]);

BLEResult bleResults[40];
int       bleCount      = 0;
bool      bleScanActive = false;

static bool           _bleInitialised = false;
static uint32_t       _scanStartTime  = 0;
static NimBLEScan*    _pScan          = nullptr;
static bool           _radarMode      = false;
static const uint32_t RADAR_SWEEP_MS  = 5000;
uint32_t bleAlertsLoggedTotal = 0;

static String buildDeviceType(const NimBLEAdvertisedDevice* device,
                               const String& addr, bool isPublic,
                               String& outManufacturer) {

    if (device->haveManufacturerData()) {
        std::string rawStd = device->getManufacturerData();
        const uint8_t* data = (const uint8_t*)rawStd.data();
        size_t len = rawStd.size();

        if (len >= 2) {
            uint16_t mfrId = (uint16_t)data[0] | ((uint16_t)data[1] << 8);
            outManufacturer = decodeManufacturerID(mfrId);

            if (mfrId == 0x004C && len >= 3) {
                return "Apple " + decodeAppleType(data[2]);
            }

            if (!outManufacturer.isEmpty()) {
                return outManufacturer + " Device";
            }
        }
    }

    if (device->haveAppearance()) {
        String fromAppearance = decodeAppearance(device->getAppearance());
        if (!fromAppearance.isEmpty()) {
            outManufacturer = isPublic ? lookupOUI(addr) : "Random MAC";
            if (outManufacturer.isEmpty()) outManufacturer = "Unknown";
            return fromAppearance;
        }
    }

    int svcCount = device->getServiceUUIDCount();
    for (int i = 0; i < svcCount; i++) {
        String uuid = device->getServiceUUID(i).toString().c_str();
        String fromService = decodeServiceUUID(uuid);
        if (!fromService.isEmpty()) {
            outManufacturer = isPublic ? lookupOUI(addr) : "Random MAC";
            if (outManufacturer.isEmpty()) outManufacturer = "Unknown";
            return fromService;
        }
    }

    String oui = lookupOUI(addr);
    if (!oui.isEmpty()) {
        outManufacturer = oui;
        return oui + " Device";
    }

    outManufacturer = isPublic ? "Unknown" : "Random MAC";
    return isPublic ? "Unknown Device" : "Unknown Device";
}

static bool checkAlert(const String& addr, const String& name,
                       const NimBLEAdvertisedDevice* device,
                       String& outLabel) {
    String prefix = addr.substring(0, 8);
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

        if (bleCount >= 40) return;

        String  name     = device->haveName() ? device->getName().c_str() : "";
        int     rssi     = device->getRSSI();
        uint8_t addrType = device->getAddress().getType();
        bool    isPublic = (addrType == 0 || addrType == 2);

        String alertLabel = "";
        bool   isAlert    = checkAlert(addr, name, device, alertLabel);

        String manufacturer = "";
        String deviceType   = buildDeviceType(device, addr, isPublic, manufacturer);

        if (!name.isEmpty() && deviceType == "Private Device") {
            deviceType = "Named Device";
        }

        BLEResult& r   = bleResults[bleCount];
        r.address       = addr;
        r.name          = name;
        r.rssi          = rssi;
        r.manufacturer  = manufacturer;
        r.deviceType    = deviceType;
        r.isAlert       = isAlert;
        r.alertLabel    = alertLabel;
        r.isPublicAddr  = isPublic;
        r.isKnown       = false;
        bleCount++;

        if (isAlert) {
            Serial.printf("[BLE] !!! ALERT: %s [%s] %s | %s | %s | %d dBm\n",
                addr.c_str(), alertLabel.c_str(), manufacturer.c_str(),
                name.isEmpty() ? "No Name" : name.c_str(),
                deviceType.c_str(), rssi);
        } else if (!_radarMode) {
            String displayName = !name.isEmpty() ? name : deviceType;
            Serial.printf("[BLE] Found: %-20s | %-25s | %-15s | %d dBm\n",
                addr.c_str(), displayName.c_str(), manufacturer.c_str(), rssi);
        }
    }

    void onScanEnd(const NimBLEScanResults& results, int reason) override {
        Serial.printf("[BLE] Scan ended (reason %d, %d devices)\n",
                      reason, bleCount);
        bleScanActive = false;
    }
};

static ScanCallback _scanCallback;

static void logAlertsToSD() {
    if (!sdActive) return;

    int alertCount = 0;
    for (int i = 0; i < bleCount; i++) {
        if (bleResults[i].isAlert) alertCount++;
    }
    if (alertCount == 0) return;

    if (!SD.exists("/ble_alerts")) {
        SD.mkdir("/ble_alerts");
    }

    char dateBuf[16];
    if (gpsData.year > 2000) {
        snprintf(dateBuf, sizeof(dateBuf), "%04d%02d%02d",
                 gpsData.year, gpsData.month, gpsData.day);
    } else {
        strcpy(dateBuf, "NODATE");
    }

    char pathBuf[48];
    snprintf(pathBuf, sizeof(pathBuf), "/ble_alerts/BLE_%s.csv", dateBuf);

    bool isNew = !SD.exists(pathBuf);
    File f = SD.open(pathBuf, FILE_APPEND);
    if (!f) {
        Serial.println("[BLE Log] ERROR: could not open file");
        return;
    }

    if (isNew) {
        f.println("time,lat,lon,alt,sats,address,name,alert_label,device_type,manufacturer,rssi,addr_type");
    }

    char timeBuf[16];
    snprintf(timeBuf, sizeof(timeBuf), "%02d:%02d:%02d",
             gpsData.hour, gpsData.minute, gpsData.second);

    for (int i = 0; i < bleCount; i++) {
        if (!bleResults[i].isAlert) continue;
        const BLEResult& r = bleResults[i];

        // Sanitise commas for CSV
        String name  = r.name.isEmpty()         ? "Unknown" : r.name;
        String mfr   = r.manufacturer.isEmpty()  ? "Unknown" : r.manufacturer;
        String label = r.alertLabel;
        String dtype = r.deviceType;
        name.replace(",",  ";");
        mfr.replace(",",   ";");
        label.replace(",", ";");
        dtype.replace(",", ";");

        char line[384];
        snprintf(line, sizeof(line),
                 "%s,%.6f,%.6f,%.1f,%d,%s,%s,%s,%s,%s,%d,%s",
                 timeBuf,
                 gpsData.latitude,
                 gpsData.longitude,
                 gpsData.altitude,
                 gpsData.satellites,
                 r.address.c_str(),
                 name.c_str(),
                 label.c_str(),
                 dtype.c_str(),
                 mfr.c_str(),
                 r.rssi,
                 r.isPublicAddr ? "Public" : "Random");
        f.println(line);
        bleAlertsLoggedTotal++; 
    }

    f.close();
    Serial.printf("[BLE Log] %d alert(s) saved to %s\n", alertCount, pathBuf);
}

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
        if (bleScanActive) { _pScan->stop(); delay(100); }
        _pScan = nullptr;
    }

    bleScanActive = false;
    _radarMode    = false;
    NimBLEDevice::deinit(true);
    _bleInitialised = false;
    bleCount = 0;
    Serial.println("[BLE] Radio Offline.");
}

bool isBleInitialised() { return _bleInitialised; }

void bleStartScan() {
    if (!_bleInitialised) { Serial.println("[BLE] Error: Call bleBegin first."); return; }
    if (bleScanActive) return;
    if (!_pScan) { Serial.println("[BLE] Error: _pScan is null!"); return; }

    Serial.println("[BLE] Scanning started...");
    bleCount       = 0;
    _scanStartTime = millis();
    bleScanActive  = true;
    _pScan->clearResults();

    if (!_pScan->start(0, false)) {
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
    if (_pScan && bleScanActive) { _pScan->stop(); delay(50); _pScan->clearResults(); }
    bleScanActive = false;
    bleCount = 0;
}

uint32_t bleScanStartTime() { return _scanStartTime; }

void bleStartRadar() {
    if (!_bleInitialised) bleBegin();
    if (!_pScan) return;

    _radarMode     = true;
    bleCount       = 0;
    bleAlertsLoggedTotal = 0;
    _scanStartTime = millis();
    bleScanActive  = true;
    _pScan->clearResults();
    _pScan->start(0, false);
    Serial.println("[BLE] Radar scanning started.");
}

void bleStopRadar() {
    _radarMode = false;
    if (_pScan && bleScanActive) { _pScan->stop(); delay(50); }
    bleScanActive = false;
    bleCount = 0;
    Serial.println("[BLE] Radar scanning stopped.");
}

void bleForceSweep() {
    if (!_bleInitialised || !_radarMode || !_pScan) return;
    if (bleScanActive) { _pScan->stop(); delay(50); }
    bleScanActive  = false;
    bleCount       = 0;
    _scanStartTime = millis();
    _pScan->clearResults();
    bleScanActive = true;
    _pScan->start(0, false);
    Serial.println("[BLE] Forced sweep started.");
}

void bleUpdate() {
    if (!_bleInitialised || !_pScan) return;

    if (_radarMode) {
        if (bleScanActive && millis() - _scanStartTime > RADAR_SWEEP_MS) {
            _pScan->stop();
            bleScanActive = false;
            Serial.printf("[BLE Radar] Sweep done: %d devices, %d alerts\n",
                          bleCount, bleAlertCount());

            if (bleHasAlerts()) {
                blePrintAlerts();
                logAlertsToSD();
            }
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
        if (bleHasAlerts()) {
            blePrintAlerts();
            logAlertsToSD();
        }
    }
}

int bleAlertCount() {
    int count = 0;
    for (int i = 0; i < bleCount; i++) if (bleResults[i].isAlert) count++;
    return count;
}

bool bleHasAlerts() { return bleAlertCount() > 0; }

void bleGetSortedIndices(int* idx, int count) {
    for (int i = 0; i < count; i++) idx[i] = i;
    for (int i = 0; i < count - 1; i++) {
        for (int j = i + 1; j < count; j++) {
            if (bleResults[idx[j]].rssi > bleResults[idx[i]].rssi) {
                int tmp = idx[i];
                idx[i] = idx[j];
                idx[j] = tmp;
            }
        }
    }
}

void blePrintInfo() {
    if (bleCount == 0) { Serial.println("[BLE] No devices found."); return; }

    Serial.println("\n[BLE] ==========================================");
    Serial.printf("[BLE]  Scan Complete: %d device%s found\n",
                  bleCount, bleCount == 1 ? "" : "s");
    Serial.println("[BLE] ==========================================");

    for (int i = 0; i < bleCount; i++) {
        const BLEResult& r = bleResults[i];
        String displayName = !r.name.isEmpty() ? r.name : r.deviceType;
        Serial.printf("[BLE]  %2d: %-20s | %-25s | %-15s | %d dBm%s%s\n",
            i + 1, r.address.c_str(), displayName.c_str(),
            r.manufacturer.c_str(), r.rssi,
            r.isKnown ? " [KNOWN]" : "",
            r.isAlert ? " [!ALERT]" : "");
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
        Serial.printf("[BLE] ! %-20s | %s | %s | %d dBm\n",
            r.alertLabel.c_str(), r.address.c_str(),
            r.name.isEmpty() ? r.deviceType.c_str() : r.name.c_str(),
            r.rssi);
    }
    Serial.println("[BLE] ------------------------------------------\n");
}