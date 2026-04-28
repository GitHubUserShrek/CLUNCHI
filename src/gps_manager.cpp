#include "gps_manager.h"
#include "config.h"
#include <TinyGPS++.h>
#include <HardwareSerial.h>

static HardwareSerial GPSSerial(1);
static TinyGPSPlus    gps;

GPSData gpsData     = {};
bool    gpsActive   = false;
static uint32_t _lastPrint = 0;

void gpsBegin() {
    if (gpsActive) return;
    
    GPSSerial.begin(GPS_BAUD, SERIAL_8N1, GPS_RX_PIN, GPS_TX_PIN);
    gpsActive = true;
    
    Serial.println("[GPS] Manager started on UART1 (RX:GPIO3, TX:GPIO4)");
}

void gpsEnd() {
    if (!gpsActive) return;
    GPSSerial.end();
    gpsActive = false;
    Serial.println("[GPS] Manager stopped.");
}

void gpsUpdate() {
    if (!gpsActive) return;
    
    while (GPSSerial.available() > 0) {
        gps.encode(GPSSerial.read());
    }
    
    gpsData.valid      = gps.location.isValid();
    gpsData.latitude   = gps.location.lat();
    gpsData.longitude  = gps.location.lng();
    gpsData.altitude   = gps.altitude.meters();
    gpsData.speed      = gps.speed.kmph();
    gpsData.satellites = gps.satellites.value();
    gpsData.hdop       = gps.hdop.value();
    
    if (gps.time.isValid()) {
        gpsData.hour   = gps.time.hour();
        gpsData.minute = gps.time.minute();
        gpsData.second = gps.time.second();
    }
        if (gps.date.isValid()) {
        gpsData.year  = gps.date.year();
        gpsData.month = gps.date.month();
        gpsData.day   = gps.date.day();
    }
}

void gpsPrintStatus() {
    if (!gpsActive) return;
    
    uint32_t now = millis();
    if (now - _lastPrint < 15000) return;
    _lastPrint = now;
    
    Serial.println();
    Serial.println("[GPS] ==========================================");
    Serial.printf("[GPS]  Status:     %s\n", gpsData.valid ? "LOCK" : "SEARCHING");
    Serial.printf("[GPS]  Satellites: %d\n", gpsData.satellites);
    Serial.printf("[GPS]  HDOP:       %.1f\n", gpsData.hdop / 10.0);
    Serial.printf("[GPS]  Latitude:   %.6f\n", gpsData.latitude);
    Serial.printf("[GPS]  Longitude:  %.6f\n", gpsData.longitude);
    Serial.printf("[GPS]  Altitude:   %.1f m\n", gpsData.altitude);
    Serial.printf("[GPS]  Speed:      %.1f km/h\n", gpsData.speed);
    if (gps.time.isValid()) {
        Serial.printf("[GPS]  Time (UTC): %02d:%02d:%02d\n", 
                      gpsData.hour, gpsData.minute, gpsData.second);
    }
    Serial.println("[GPS] ==========================================");
    Serial.println();
}