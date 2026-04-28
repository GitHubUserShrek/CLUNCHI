#pragma once
#include <Arduino.h>

struct GPSData {
    double   latitude;
    double   longitude;
    double   altitude;
    double   speed;
    int      satellites;
    int      hdop;
    bool     valid;
    uint8_t  hour;
    uint8_t  minute;
    uint8_t  second;
    uint16_t year;
    uint8_t  month;
    uint8_t  day;
};

extern GPSData gpsData;
extern bool    gpsActive;

void gpsBegin();
void gpsEnd();
void gpsUpdate();
void gpsPrintStatus();