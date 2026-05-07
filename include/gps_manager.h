#pragma once
#include <Arduino.h>

#define MAX_SATS 20

struct SatInfo {
    uint8_t  prn;
    uint8_t  elevation;
    uint16_t azimuth;
    uint8_t  snr;
    bool     tracked;
};

struct TimezoneEntry {
    const char* label;
    int         offset;   
};

extern const TimezoneEntry timezones[];
extern const int           timezoneCount;

struct LocalTime {
    uint8_t  hour;
    uint8_t  minute;
    uint8_t  second;
    uint8_t  day;
    uint8_t  month;
    uint16_t year;
    bool     valid;
};

struct GPSData {
    double   latitude;
    double   longitude;
    double   altitude;
    double   speed;
    double   course;
    int      satellites;
    int      hdop;
    bool     valid;
    uint8_t  hour;
    uint8_t  minute;
    uint8_t  second;
    uint16_t year;
    uint8_t  month;
    uint8_t  day;

    SatInfo  sats[MAX_SATS];
    uint8_t  satCount;
    uint8_t  satsTracked;
};

extern GPSData gpsData;
extern bool    gpsActive;

extern int  deviceTimezone;   
extern bool deviceDST;        

void gpsBegin();
void gpsEnd();
void gpsUpdate();
void gpsPrintStatus();

const char* gpsConstellation(uint8_t prn);

LocalTime gpsGetLocalTime();

void gpsLoadTimeSettings();
void gpsSaveTimeSettings();