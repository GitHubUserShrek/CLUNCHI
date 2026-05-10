#include "tilt.h"
#include "config.h"
#include <Preferences.h>

static bool     lastState       = HIGH;
static uint32_t changeCount     = 0;
static uint32_t windowStart     = 0;
static bool     shakeFlag       = false;
static bool     singleHitFlag   = false;
static bool     _tiltEnabled    = true;

#define SHAKE_WINDOW_MS    1000
#define SHAKE_THRESHOLD    10

void tiltBegin() {
    pinMode(TILT_PIN, INPUT_PULLUP);
    lastState   = digitalRead(TILT_PIN);
    changeCount = 0;
    windowStart = millis();
    shakeFlag   = false;
    singleHitFlag = false;
}

void tiltUpdate() {
    shakeFlag     = false;
    singleHitFlag = false;
    if (!_tiltEnabled) return;

    bool current = digitalRead(TILT_PIN);

    if (current != lastState) {
        lastState = current;
        changeCount++;
        singleHitFlag = true;  
    }

    if (millis() - windowStart > SHAKE_WINDOW_MS) {
        if (changeCount >= SHAKE_THRESHOLD) {
            shakeFlag = true;
        }
        changeCount = 0;
        windowStart = millis();
    }
}

bool tiltShakeDetected() {
    return shakeFlag;
}

bool tiltSingleHit() {
    return singleHitFlag;
}

bool tiltEnabled() {
    return _tiltEnabled;
}

void tiltSetEnabled(bool enabled) {
    _tiltEnabled = enabled;
}

void tiltLoadSettings() {
    Preferences prefs;
    prefs.begin("clunchi", true);
    _tiltEnabled = prefs.getBool("tilt_on", true);
    prefs.end();
    Serial.printf("[Tilt] Loaded setting: %s\n", _tiltEnabled ? "ON" : "OFF");
}

void tiltSaveSettings() {
    Preferences prefs;
    prefs.begin("clunchi", false);
    prefs.putBool("tilt_on", _tiltEnabled);
    prefs.end();
    Serial.printf("[Tilt] Saved setting: %s\n", _tiltEnabled ? "ON" : "OFF");
}