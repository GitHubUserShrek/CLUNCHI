#include "mesh_wardrive.h"
#include "sd_manager.h"
#include "gps_manager.h"
#include <SD.h>

#define MAX_SEEN_MESH 100
static String   _seenMeshNodes[MAX_SEEN_MESH];
static int      _seenMeshCount   = 0;
static bool     _hasNewMeshNode  = false;
static uint32_t _lastNewNodeTime = 0;

void meshBegin() {
    _seenMeshCount   = 0;
    _hasNewMeshNode  = false;
    _lastNewNodeTime = 0;
    if (sdActive && !SD.exists("/mesh")) {
        SD.mkdir("/mesh");
    }
    Serial.println("[Mesh Wardrive] Tactical Scout engine initialized.");
}

void meshEnd() {
    _seenMeshCount  = 0;
    _hasNewMeshNode = false;
    Serial.printf("[Mesh Wardrive] Scout engine offline. Session nodes tracked: %d\n", _seenMeshCount);
}

int meshGetLoggedCount() {
    return _seenMeshCount;
}

static bool isMeshNodeAlreadySeen(const String& addr) {
    for (int i = 0; i < _seenMeshCount; i++) {
        if (_seenMeshNodes[i] == addr) return true;
    }
    if (_seenMeshCount < MAX_SEEN_MESH) {
        _seenMeshNodes[_seenMeshCount++] = addr;
    }
    return false;
}

bool meshProcessDevice(const NimBLEAdvertisedDevice* device, const String& name, const String& addr, bool& outIsMesh, bool& outIsNew) {
    outIsMesh = false;
    outIsNew  = false;

    int sDataCount = device->getServiceDataCount();
    for (int i = 0; i < sDataCount; i++) {
        String sUuid = device->getServiceDataUUID(i).toString().c_str(); sUuid.toLowerCase();
        if (sUuid.indexOf("1ba9f000") >= 0 || sUuid.indexOf("cb0b9a0b") >= 0 || sUuid.indexOf("ba57") >= 0) {
            outIsMesh = true; break;
        }
    }

    if (!outIsMesh && name.length() >= 6 && name.indexOf('_') > 0) {
        int underscoreIdx = name.lastIndexOf('_'); String suffix = name.substring(underscoreIdx + 1); suffix.toLowerCase();
        if (addr.length() >= 17 && suffix.length() == 4) {
            String macSuffix = addr.substring(12, 14) + addr.substring(15, 17); macSuffix.toLowerCase();
            if (suffix == macSuffix) outIsMesh = true;
        }
    }

    if (outIsMesh) {
        if (!isMeshNodeAlreadySeen(addr)) {
            outIsNew         = true;
            _hasNewMeshNode  = true;
            _lastNewNodeTime = millis();
        }
    }
    return outIsMesh;
}

bool meshHasRecentNewNode() {
    if (!_hasNewMeshNode) return false;
    if (millis() - _lastNewNodeTime > 5000) {
        _hasNewMeshNode = false; 
        return false;
    }
    return true;
}

void meshLogToSD(const BLEResult* results, int count) {
    if (!sdActive) return;

    int newMeshCount = 0;
    for (int i = 0; i < count; i++) if (results[i].isNewMeshtastic) newMeshCount++;
    if (newMeshCount == 0) return;

    if (!SD.exists("/mesh")) SD.mkdir("/mesh");

    char dateBuf[16]; LocalTime lt = gpsGetLocalTime();
    if (lt.valid) snprintf(dateBuf, sizeof(dateBuf), "%04d%02d%02d", lt.year, lt.month, lt.day);
    else if (gpsData.year > 2000) snprintf(dateBuf, sizeof(dateBuf), "%04d%02d%02d", gpsData.year, gpsData.month, gpsData.day);
    else strcpy(dateBuf, "NODATE");

    char pathBuf[48]; snprintf(pathBuf, sizeof(pathBuf), "/mesh/MESH_%s.csv", dateBuf);
    bool isNew = !SD.exists(pathBuf); File f = SD.open(pathBuf, FILE_APPEND);
    if (!f) { Serial.println("[Mesh Wardrive] ERROR: could not open SD log file"); return; }
    if (isNew) f.println("time,lat,lon,alt,sats,address,node_name,rssi,addr_type");

    char timeBuf[16];
    if (lt.valid) snprintf(timeBuf, sizeof(timeBuf), "%02d:%02d:%02d", lt.hour, lt.minute, lt.second);
    else snprintf(timeBuf, sizeof(timeBuf), "%02d:%02d:%02d", gpsData.hour, gpsData.minute, gpsData.second);

    for (int i = 0; i < count; i++) {
        if (!results[i].isNewMeshtastic) continue;
        const BLEResult& r = results[i];

        String name = r.name.isEmpty() ? "Unknown_Node" : r.name;
        name.replace(",", ";");

        char line[256];
        snprintf(line, sizeof(line), "%s,%.6f,%.6f,%.1f,%d,%s,%s,%d,%s",
                 timeBuf, gpsData.latitude, gpsData.longitude, gpsData.altitude, gpsData.satellites,
                 r.address.c_str(), name.c_str(), r.rssi, r.isPublicAddr ? "Public" : "Random");
        f.println(line);
    }
    f.close();
    Serial.printf("[Mesh Wardrive Log] %d new node(s) saved to %s\n", newMeshCount, pathBuf);
}