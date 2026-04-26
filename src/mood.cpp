#include "mood.h"
#include "audio.h"
#include "animation.h"
#include "touch.h"
#include "menu.h"
#include "wifi_manager.h"
#include "ble_manager.h"
#include "deauth_detector.h"
#include "net_health.h"
#include <Preferences.h>

// ── External objects (owned by main.cpp) ─────────────
extern Audio     audio;
extern Animation animation;

// ── Public state ─────────────────────────────────────
Mood     mood            = NEUTRAL;
uint32_t moodChangeTime  = 0;
uint32_t lastInteraction = 0;

// ── Private state ────────────────────────────────────
static bool     _wasConnected   = false;
static bool     _wasNetDown     = false;
static bool     _radarActive    = false;
static bool     _radarTouchLock = true;
static uint32_t _lastAlertTime  = 0;
static uint32_t _sleepTimeoutMs = 30000;
static uint32_t _lastSettingsCheck = 0;

// ── Helpers ──────────────────────────────────────────
bool isRadarActive() { return _radarActive; }

Mood baseMood() {
    return wifiConnected() ? HAPPY : NEUTRAL;
}

const char* moodName(Mood m) {
    switch (m) {
        case NEUTRAL:  return "NEUTRAL";
        case HAPPY:    return "HAPPY";
        case SLEEPY:   return "SLEEPY";
        case ANNOYED:  return "ANNOYED";
        case CURIOUS:  return "CURIOUS";
        case JAZZED:   return "JAZZED";
        case VIGILANT: return "VIGILANT";
        case ENRAGED:  return "ENRAGED";
        case DEAD:     return "DEAD";
        default:       return "???";
    }
}

void moodBegin() {
    mood = NEUTRAL;
    moodChangeTime = millis();
    lastInteraction = millis();
    _wasConnected = wifiConnected();
    Serial.println("[Mood] State sponsored machine initialized.");
}

// ─────────────────────────────────────────────────────
//  Reaction helpers
// ─────────────────────────────────────────────────────

static void _setMood(Mood m) {
    if (mood != m) {
        animation.onMoodChange();
    }
    mood = m;
    moodChangeTime = millis();
}

static void _reactJazzed() {
    _setMood(JAZZED);
    audio.jazzed();
    if (wifiConnected()) wifiPrintInfo();
    lastInteraction = millis();
}

static void _reactNetworkCheck() {
    if (!wifiConnected()) return;
    uint32_t now = millis();

    if (!netHealthIsUp) {
        _setMood(ANNOYED);
        audio.annoyed();
        lastInteraction = now;
        Serial.println("[Mood] Internet is down. Annoyed.");
        return;
    }

    int32_t rssi = wifiRSSI();
    const char* quality;
    if      (rssi >= -50) quality = "SUPER";
    else if (rssi >= -60) quality = "Good";
    else if (rssi >= -70) quality = "Meh";
    else if (rssi >= -80) quality = "Weak";
    else                  quality = "Very Weak";

    Serial.printf("[Mood] Signal: %d dBm (%s) Ch:%d\n",
                  (int)rssi, quality, wifiConnectedChannel());

    if (rssi < -80) {
        _setMood(ANNOYED);
        audio.annoyed();
    } else {
        _reactJazzed();
    }
    lastInteraction = millis();
}

static void _reactCurious() {
    lastInteraction = millis();
    _setMood(CURIOUS);
    audio.curious();
    if (!scanActive) {
        Serial.println("[Mood] Huh?...");
        wifiStartScan();
    }
}

// ─────────────────────────────────────────────────────
//  Touch event reactions
// ─────────────────────────────────────────────────────

static void _handleTouchEvent(TouchEvent event, uint32_t now) {
    if (event == TouchEvent::NONE) return;

    lastInteraction = now;

    switch (event) {

    case TouchEvent::LONG_PRESS:
        if (mood == ENRAGED) {
            animation.triggerSpiralEyes(4000);
            audio.spiralEyes();
            Serial.println("[Mood] Dizzy");
        } else {
            _setMood(HAPPY);
            animation.triggerHeartEyes(4000);
            audio.heartEyes();
            Serial.println("[Mood] UwU");
        }
        break;

    case TouchEvent::TAP_10_PLUS:
        animation.triggerDribble(5000);
        audio.dribble();
        break;

    case TouchEvent::DRIBBLE_HIT: {
        animation.triggerDribbleHit();
        int pitch = 600 + random(400);
        audio.beep(pitch, 30);
        break;
    }

    case TouchEvent::DRIBBLE_END:
        Serial.println("[Mood] That was...fun");
        break;

    case TouchEvent::TAP_6_PLUS:
        _setMood(ANNOYED);
        audio.annoyed();
        Serial.println("[Mood] Grrrrr...");
        break;

    case TouchEvent::TAP_5:
        _reactCurious();
        break;

    case TouchEvent::TAP_4:
        break;

    case TouchEvent::TAP_3:
        if (wifiConnected()) {
            _reactNetworkCheck();
        } else {
            _setMood(HAPPY);
            audio.happy();
            animation.triggerBlink();
        }
        break;

    case TouchEvent::TAP_2:
        enterMenu();
        break;

    case TouchEvent::TAP_1:
        break;
    }
}

// ─────────────────────────────────────────────────────
//  Radar mode
// ─────────────────────────────────────────────────────

void triggerRadar() {
    _radarActive    = true;
    _radarTouchLock = true;
    _lastAlertTime  = 0;

    wifiDeinit();
    delay(200);
    bleStartRadar();

    _setMood(VIGILANT);
    lastInteraction = millis();

    Serial.println("[Mood] === RADAR MODE ACTIVATED ===");
    audio.radarOn();
}

void exitRadar() {
    if (!_radarActive) return;
    _radarActive = false;

    bleStopRadar();
    delay(200);
    bleDeinit();
    wifiBegin();

    _setMood(baseMood());
    lastInteraction = millis();

    Serial.println("[Mood] === RADAR MODE DEACTIVATED ===");
    audio.radarOff();
}

static void _updateRadar(uint32_t now) {
    const uint32_t THREAT_CLEAR_MS = 10000;

    if (!isTouched) _radarTouchLock = false;

    if (longTouchActive && !_radarTouchLock) {
        exitRadar();
        return;
    }

    static int      radarTapCount = 0;
    static uint32_t radarLastTap  = 0;

    if (touchJustReleased && !touchWasLongPress) {
        radarTapCount++;
        radarLastTap = now;
        audio.beep(600, 20);
    }

    if (radarTapCount > 0 && now - radarLastTap > 400) {
        if (radarTapCount >= 5) {
            Serial.println("[Mood] Radar ping! Eyes up.");
            bleForceSweep();
            audio.radarPing();
        }
        else if (radarTapCount >= 3) {
            _setMood(ANNOYED);
            Serial.println("[Mood] Not now!");
            audio.annoyed();
        }
        radarTapCount = 0;
    }

    if (bleHasAlerts()) {
        _lastAlertTime = now;
        if (mood != ENRAGED) {
            _setMood(ENRAGED);
            Serial.println("[Mood] !Radar Warning! Smoke in the air.");
            audio.radarAlert();
        }
    }


    else if (mood == ENRAGED && (now - _lastAlertTime > THREAT_CLEAR_MS)) {
        _setMood(VIGILANT);
        Serial.println("[Mood] Threat cleared. Back on watch.");
        audio.radarOff();
    }
    else if (mood == ANNOYED && now - moodChangeTime > 4000) {
        _setMood(VIGILANT);
        Serial.println("[Mood] Calmed down, back on duty.");
    }
}

// ─────────────────────────────────────────────────────
//  Network state reactions
// ─────────────────────────────────────────────────────

static void _updateNetworkState(uint32_t now, bool connected) {
    if (connected != _wasConnected) {
        if (!connected && deauthHasRecentAlert(15000)) {
            _setMood(DEAD);
            Serial.println("[Mood] !!! KILLED BY DEATH! (DEAUTH)");
            audio.dead();
        } else {
            _setMood(baseMood());
        }
        _wasConnected = connected;
        Serial.printf("[Mood] Network: %s\n", connected ? "Online" : "Offline");
    }

    if (connected && deauthDetectorActive && deauthHasRecentAlert(5000)) {
        if (mood == SLEEPY || mood != ENRAGED) {
            _setMood(ENRAGED);
            lastInteraction = now;
            Serial.println("[Mood] Stop right there criminal scum!");
            audio.enraged();
        }
    }

    if (connected && mood != SLEEPY) {
        if (!netHealthIsUp && netHealthConsecutiveFails >= 3 && mood != DEAD) {
            _wasNetDown = true;
            if (mood != ENRAGED) {
                _setMood(ENRAGED);
                audio.enraged();
            }
        }
        else if (!netHealthIsUp && netHealthConsecutiveFails > 0 &&
                 netHealthConsecutiveFails < 3 &&
                 mood != ENRAGED && mood != ANNOYED) {
            _setMood(ANNOYED);
            audio.annoyed();
        }
    }

    if (connected && netHealthIsUp && _wasNetDown) {
        _wasNetDown = false;
        if (mood != SLEEPY) _reactJazzed();
    }
}

// ─────────────────────────────────────────────────────
//  Mood decay / timeouts
// ─────────────────────────────────────────────────────

static void _updateMoodDecay(uint32_t now, bool connected) {
    uint32_t idleTime = now - lastInteraction;

    if (isDribbleActive()) return;

    if (_sleepTimeoutMs > 0 && idleTime > _sleepTimeoutMs &&
        mood != SLEEPY && mood != DEAD) {
        _setMood(SLEEPY);
        audio.sleepy();
        Serial.println("[Mood] HonkShoo mimimi");
        return;
    }

    if (isTouched && mood == DEAD) {
        _setMood(NEUTRAL);
        audio.chirp();
        Serial.println("[Mood] Revived by boop!");
        return;
    }
    if (isTouched && mood == SLEEPY) {
        _setMood(baseMood());
        audio.chirp();
        return;
    }

    uint32_t age = now - moodChangeTime;

    if      (mood == DEAD    && age > 15000) { _setMood(NEUTRAL); Serial.println("[Mood] Resurrection complete"); return; }
    else if (mood == ANNOYED && age > 4000)  { _setMood(baseMood()); return; }
    else if (mood == HAPPY   && age > 8000)  { _setMood(baseMood()); return; }
    else if (mood == CURIOUS && age > 6000)  { _setMood(baseMood()); return; }
    else if (mood == JAZZED  && age > 6000)  { _setMood(baseMood()); return; }
    else if (mood == ENRAGED) {
        bool deauthNow = connected && deauthDetectorActive && deauthHasRecentAlert(5000);
        bool netDown   = connected && !netHealthIsUp && netHealthConsecutiveFails >= 3;

        if (!deauthNow && !netDown && age > 5000) {
            _setMood(baseMood());
            Serial.println("[Mood] Must've been the wind");
            return;
        }
    }
}

// ─────────────────────────────────────────────────────
//  Idle personality
// ─────────────────────────────────────────────────────

static void _updateIdlePersonality(uint32_t now, bool connected) {
    uint32_t idleTime = now - lastInteraction;

    if (mood != baseMood() || idleTime <= 10000) return;

    static uint32_t lastAutoRoll = 0;
    if (now - lastAutoRoll < 30000) return;
    lastAutoRoll = now;

    int roll = random(100);

    if (!connected && roll < 15) {
        _reactCurious();
    }
    else if (connected && roll < 20) {
        _reactNetworkCheck();
    }
}

// ─────────────────────────────────────────────────────
//  Settings reload
// ─────────────────────────────────────────────────────

static void _reloadSettings(uint32_t now) {
    if (isMenuActive()) return;
    if (now - _lastSettingsCheck < 10000) return;
    _lastSettingsCheck = now;

    Preferences prefs;
    if (prefs.begin("clunchi", true)) {
        uint32_t savedSecs = prefs.getUInt("sleep_time", 60);
        _sleepTimeoutMs = (savedSecs > 0) ? (savedSecs * 1000) : 0;
        prefs.end();
    }
}

// ─────────────────────────────────────────────────────
//  Main update
// ─────────────────────────────────────────────────────

void moodUpdate(TouchEvent event) {
    if (wifiIsPortalActive()) return;

    uint32_t now = millis();
    bool connected = wifiConnected();

    if (touchJustPressed) {
        lastInteraction = now;
        if (!isDribbleActive()) {
            audio.chirp();
            animation.triggerSquish();
            animation.triggerBlink();
        }
    }

    if (_radarActive) {
        _updateRadar(now);
        return;
    }

    if (!isMenuActive()) {
        _handleTouchEvent(event, now);
        now = millis();
    }

    _reloadSettings(now);
    _updateNetworkState(now, connected);
    _updateMoodDecay(now, connected);
    _updateIdlePersonality(now, connected);
}