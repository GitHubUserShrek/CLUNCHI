#include "gps_manager.h"
#include "config.h"
#include <TinyGPS++.h>
#include <HardwareSerial.h>
#include <Preferences.h>

static HardwareSerial GPSSerial(1);
static TinyGPSPlus    gps;

GPSData gpsData     = {};
bool    gpsActive   = false;
static uint32_t _lastPrint = 0;

int  deviceTimezone = 0;     
bool deviceDST      = false;

static char    nmeaBuf[120];
static uint8_t nmeaIdx = 0;
static SatInfo  tempSats[MAX_SATS];
static uint8_t  tempSatCount = 0;

static void feedChar(char c);
static void parseGSV(const char* sentence);
static int  splitFields(char* buf, char** fields, int maxFields);

const TimezoneEntry timezones[] = {
    { "UTC",     0    },
    { "EST",    -300  },   // UTC-5
    { "CST",    -360  },   // UTC-6
    { "MST",    -420  },   // UTC-7
    { "PST",    -480  },   // UTC-8
    { "AKST",   -540  },   // UTC-9
    { "HST",    -600  },   // UTC-10
    { "GMT",     0    },
    { "CET",     60   },   // UTC+1
    { "EET",     120  },   // UTC+2
    { "MSK",     180  },   // UTC+3
    { "GST",     240  },   // UTC+4
    { "IST",     330  },   // UTC+5:30
    { "CST-CN",  480  },   // UTC+8
    { "JST",     540  },   // UTC+9
    { "AEST",    600  },   // UTC+10
    { "NZST",    720  },   // UTC+12
};

const int timezoneCount = sizeof(timezones) / sizeof(timezones[0]);

void gpsLoadTimeSettings() {
    Preferences prefs;
    prefs.begin("clunchi", true);
    deviceTimezone = prefs.getInt("timezone", 0);
    deviceDST      = prefs.getBool("dst", false);
    prefs.end();

    if (deviceTimezone < 0 || deviceTimezone >= timezoneCount) {
        deviceTimezone = 0;
    }

    Serial.printf("[GPS] Loaded timezone: %s (index %d) DST: %s\n",
                  timezones[deviceTimezone].label,
                  deviceTimezone,
                  deviceDST ? "ON" : "OFF");
}

void gpsSaveTimeSettings() {
    Preferences prefs;
    prefs.begin("clunchi", false);
    prefs.putInt("timezone", deviceTimezone);
    prefs.putBool("dst", deviceDST);
    prefs.end();

    Serial.printf("[GPS] Saved timezone: %s DST: %s\n",
                  timezones[deviceTimezone].label,
                  deviceDST ? "ON" : "OFF");
}

LocalTime gpsGetLocalTime() {
    LocalTime lt = {};
    lt.valid = false;

    if (!gpsActive) return lt;
    if (gpsData.hour == 0 && gpsData.minute == 0 &&
        gpsData.second == 0 && gpsData.year == 0) return lt;

    const TimezoneEntry& tz = timezones[deviceTimezone];
    int dstOffset = deviceDST ? 60 : 0;
    int totalMinutes = (gpsData.hour * 60) + gpsData.minute + tz.offset + dstOffset;

    int dayShift = 0;
    while (totalMinutes < 0)     { totalMinutes += 1440; dayShift--; }
    while (totalMinutes >= 1440) { totalMinutes -= 1440; dayShift++; }

    lt.hour   = totalMinutes / 60;
    lt.minute = totalMinutes % 60;
    lt.second = gpsData.second;

    static const uint8_t daysInMonth[] = {
        0, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31
    };

    int d = gpsData.day + dayShift;
    int m = gpsData.month;
    int y = gpsData.year;

    int febDays = ((y % 4 == 0 && y % 100 != 0) || (y % 400 == 0)) ? 29 : 28;

    if (d < 1) {
        m--;
        if (m < 1) { m = 12; y--; }
        d = (m == 2) ? febDays : daysInMonth[m];
    } else {
        int maxDay = (m == 2) ? febDays : daysInMonth[m];
        if (m >= 1 && m <= 12 && d > maxDay) {
            d = 1;
            m++;
            if (m > 12) { m = 1; y++; }
        }
    }

    lt.day   = d;
    lt.month = m;
    lt.year  = y;
    lt.valid = true;

    return lt;
}

void gpsBegin() {
    if (gpsActive) return;

    GPSSerial.begin(GPS_BAUD, SERIAL_8N1, GPS_RX_PIN, GPS_TX_PIN);
    gpsActive = true;

    gpsData.satCount    = 0;
    gpsData.satsTracked = 0;
    memset(gpsData.sats, 0, sizeof(gpsData.sats));

    Serial.println("[GPS] Manager started on UART1 (RX:GPIO3, TX:GPIO4)");
}

void gpsEnd() {
    if (!gpsActive) return;
    GPSSerial.end();
    gpsActive = false;
    gpsData.satCount    = 0;
    gpsData.satsTracked = 0;
    Serial.println("[GPS] Manager stopped.");
}

void gpsUpdate() {
    if (!gpsActive) return;

    while (GPSSerial.available() > 0) {
        char c = GPSSerial.read();
        gps.encode(c);
        feedChar(c);
    }

    gpsData.valid      = gps.location.isValid();
    gpsData.latitude   = gps.location.lat();
    gpsData.longitude  = gps.location.lng();
    gpsData.altitude   = gps.altitude.meters();
    gpsData.speed      = gps.speed.kmph();
    gpsData.course     = gps.course.isValid() ? gps.course.deg() : 0;
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

    LocalTime lt = gpsGetLocalTime();

    Serial.println();
    Serial.println("[GPS] ==========================================");
    Serial.printf("[GPS]  Status:     %s\n", gpsData.valid ? "LOCK" : "SEARCHING");
    Serial.printf("[GPS]  Satellites: %d (tracking %d)\n",
                  gpsData.satellites, gpsData.satsTracked);
    Serial.printf("[GPS]  HDOP:       %.1f\n", gpsData.hdop / 10.0);
    Serial.printf("[GPS]  Latitude:   %.6f\n", gpsData.latitude);
    Serial.printf("[GPS]  Longitude:  %.6f\n", gpsData.longitude);
    Serial.printf("[GPS]  Altitude:   %.1f m\n", gpsData.altitude);
    Serial.printf("[GPS]  Speed:      %.1f km/h\n", gpsData.speed);
    Serial.printf("[GPS]  Heading:    %.1f°\n", gpsData.course);
    if (gps.time.isValid()) {
        Serial.printf("[GPS]  UTC:        %02d:%02d:%02d\n",
                      gpsData.hour, gpsData.minute, gpsData.second);
    }
    if (lt.valid) {
        Serial.printf("[GPS]  Local:      %02d:%02d:%02d %s%s\n",
                      lt.hour, lt.minute, lt.second,
                      timezones[deviceTimezone].label,
                      deviceDST ? "+DST" : "");
    }

    if (gpsData.satCount > 0) {
        Serial.println("[GPS]  ---- Satellites ----");
        Serial.println("[GPS]  PRN  EL   AZ  SNR  System");
        for (int i = 0; i < gpsData.satCount; i++) {
            SatInfo& s = gpsData.sats[i];
            Serial.printf("[GPS]  %3d  %2d  %3d  %2d   %s %s\n",
                          s.prn, s.elevation, s.azimuth, s.snr,
                          gpsConstellation(s.prn),
                          s.tracked ? "" : "(no signal)");
        }
    }

    Serial.println("[GPS] ==========================================");
    Serial.println();
}

const char* gpsConstellation(uint8_t prn) {
    if (prn >= 1   && prn <= 32)  return "GPS";
    if (prn >= 33  && prn <= 64)  return "SBAS";
    if (prn >= 65  && prn <= 96)  return "GLO";
    if (prn >= 193 && prn <= 202) return "QZSS";
    if (prn >= 201 && prn <= 237) return "BDS";
    if (prn >= 301 && prn <= 336) return "GAL";
    return "???";
}

static void feedChar(char c) {
    if (c == '$') nmeaIdx = 0;

    if (nmeaIdx < sizeof(nmeaBuf) - 1) {
        nmeaBuf[nmeaIdx++] = c;
    }

    if (c == '\n' || c == '\r') {
        nmeaBuf[nmeaIdx] = '\0';
        if (nmeaIdx > 10 && strstr(nmeaBuf, "GSV")) {
            parseGSV(nmeaBuf);
        }
        nmeaIdx = 0;
    }
}

static int splitFields(char* buf, char** fields, int maxFields) {
    int count = 0;
    char* p = buf;
    while (*p && count < maxFields) {
        fields[count++] = p;
        while (*p && *p != ',' && *p != '*') p++;
        if (*p) { *p = '\0'; p++; }
    }
    return count;
}

static void parseGSV(const char* sentence) {
    char buf[120];
    strncpy(buf, sentence, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';

    char* fields[25];
    int fieldCount = splitFields(buf, fields, 25);

    if (fieldCount < 4) return;

    int totalMsgs = atoi(fields[1]);
    int msgNum    = atoi(fields[2]);

    if (msgNum == 1) tempSatCount = 0;

    for (int i = 0; i < 4; i++) {
        int base = 4 + (i * 4);
        if (base >= fieldCount) break;
        if (tempSatCount >= MAX_SATS) break;

        int prn = atoi(fields[base]);
        if (prn == 0) continue;

        SatInfo& s = tempSats[tempSatCount];
        s.prn       = prn;
        s.elevation = (base + 1 < fieldCount) ? atoi(fields[base + 1]) : 0;
        s.azimuth   = (base + 2 < fieldCount) ? atoi(fields[base + 2]) : 0;
        s.snr       = (base + 3 < fieldCount) ? atoi(fields[base + 3]) : 0;
        s.tracked   = (s.snr > 0);
        tempSatCount++;
    }

    if (msgNum >= totalMsgs) {
        gpsData.satCount = tempSatCount;
        gpsData.satsTracked = 0;
        for (int i = 0; i < tempSatCount; i++) {
            gpsData.sats[i] = tempSats[i];
            if (tempSats[i].tracked) gpsData.satsTracked++;
        }
        for (int i = tempSatCount; i < MAX_SATS; i++) {
            memset(&gpsData.sats[i], 0, sizeof(SatInfo));
        }
    }
}