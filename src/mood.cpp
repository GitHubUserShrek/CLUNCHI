#include "mood.h"
#include "audio.h"
#include "animation.h"
#include "touch.h"
#include "menu.h"
#include "wifi_manager.h"
#include "ble_manager.h"
#include "oui_lookup.h"
#include "wids.h"
#include "mesh_wardrive.h"
#include "net_health.h"
#include "wardriving.h"
#include "alpr_detector.h"
#include "gps_manager.h"
#include <Preferences.h>

extern Audio audio;
extern Animation animation;

Mood mood = NEUTRAL;
uint32_t moodChangeTime = 0;
uint32_t lastInteraction = 0;

static bool _wasConnected = false;
static bool _wasNetDown = false;
static bool _radarActive = false;
static bool _wardrivingActive = false;

static bool _radarTouchLock = true;
static bool _wardrivingTouchLock = true;

static uint32_t _lastAlertTime = 0;

static uint32_t _sleepTimeoutMs = 30000;
static uint32_t _lastSettingsCheck = 0;

#if defined(BOARD_XIAO_C5)
static bool _alprActive = false;
static bool _alprTouchLock = true;
static uint32_t _lastAlprAlertTime = 0;
#endif
#if defined(BOARD_XIAO_C5)
static bool _batteryTouchLock = true; 
#endif

bool isRadarActive() { return _radarActive; }
bool isWardrivingMoodActive() { return _wardrivingActive; }
#if defined(BOARD_XIAO_C5)
bool isAlprHunterMoodActive() { return _alprActive; }
#endif
Mood baseMood() { return wifiConnected() ? HAPPY : NEUTRAL; }

const char *moodName(Mood m)
{
    switch (m)
    {
    case NEUTRAL:
        return "NEUTRAL";
    case HAPPY:
        return "HAPPY";
    case SLEEPY:
        return "SLEEPY";
    case ANNOYED:
        return "ANNOYED";
    case CURIOUS:
        return "CURIOUS";
    case JAZZED:
        return "JAZZED";
    case VIGILANT:
        return "VIGILANT";
    case ENRAGED:
        return "ENRAGED";
    case DEAD:
        return "DEAD";
    case DRIVING:
        return "DRIVING";
    case TACTICAL:
        return "TACTICAL";
#if defined(BOARD_XIAO_C5)
    case HUNTING:
        return "HUNTING";
    case ALERT_CAMERA:
        return "ALERT_CAMERA";
#endif
    default:
        return "???";
    }
}

void moodBegin()
{
    mood = NEUTRAL;
    moodChangeTime = millis();
    lastInteraction = millis();
    _wasConnected = wifiConnected();
    Serial.println("[Mood] State sponsored machine initialized.");
}

static void _setMood(Mood m)
{
    if (mood != m)
        animation.onMoodChange();
    mood = m;
    moodChangeTime = millis();
}

void forceSetMood(Mood m) {
    _setMood(m);
    lastInteraction = millis();
    
#if defined(BOARD_XIAO_C5)
    if (m == BATTERY_STATUS) {
        _batteryTouchLock = true; 
    }
#endif
}

static void _reactJazzed()
{
    _setMood(JAZZED);
    audio.jazzed();
    Serial.println("[Mood] MMM BOP");
    if (wifiConnected())
        wifiPrintInfo();
    lastInteraction = millis();
}

static void _reactNetworkCheck()
{
    if (!wifiConnected())
        return;
    uint32_t now = millis();
    if (!netHealthIsUp)
    {
        _setMood(ANNOYED);
        audio.annoyed();
        lastInteraction = now;
        Serial.println("[Mood] Internet is down. Annoyed.");
        return;
    }
    int32_t rssi = wifiRSSI();
    const char *quality;
    if (rssi >= -50)
        quality = "SUPER";
    else if (rssi >= -60)
        quality = "Good";
    else if (rssi >= -70)
        quality = "Meh";
    else if (rssi >= -80)
        quality = "Weak";
    else
        quality = "Very Weak";

    Serial.printf("[Mood] Signal: %d dBm (%s) Ch:%d\n", (int)rssi, quality, wifiConnectedChannel());
    if (rssi < -80)
    {
        _setMood(ANNOYED);
        audio.annoyed();
    }
    else
    {
        _reactJazzed();
    }
    lastInteraction = millis();
}

static void _reactCurious()
{
    lastInteraction = millis();
    _setMood(CURIOUS);
    audio.curious();
    if (!scanActive)
    {
        Serial.println("[Mood] Huh?...");
        wifiStartScan();
    }
}

static void _handleTouchEvent(TouchEvent event, uint32_t now)
{
    if (event == TouchEvent::NONE)
        return;
    lastInteraction = now;
    switch (event)
    {
    case TouchEvent::LONG_PRESS:
        if (mood == ENRAGED)
        {
            animation.triggerSpiralEyes(4000);
            audio.spiralEyes();
            Serial.println("[Mood] Dizzy");
        }
        else
        {
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
    case TouchEvent::DRIBBLE_HIT:
    {
        animation.triggerDribbleHit();
        audio.beep(600 + random(400), 30);
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
        if (wifiConnected())
            _reactNetworkCheck();
        else
        {
            _setMood(HAPPY);
            audio.happy();
            Serial.println("[Mood] *Thunderous applause*");
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

void triggerRadar()
{
    _radarActive = true;
    _radarTouchLock = true;
    _lastAlertTime = 0;
    wifiDeinit();
    delay(200);
    bleStartRadar();
    _setMood(VIGILANT);
    lastInteraction = millis();
    Serial.println("[Mood] === RADAR MODE ACTIVATED ===");
    audio.radarOn();
}

void exitRadar()
{
    if (!_radarActive)
        return;
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

static void _updateRadar(uint32_t now)
{
    const uint32_t THREAT_CLEAR_MS = 5000;

    if (!isTouched)
        _radarTouchLock = false;
    if (longTouchActive && !_radarTouchLock)
    {
        exitRadar();
        return;
    }

    static int radarTapCount = 0;
    static uint32_t radarLastTap = 0;
    if (touchJustReleased && !touchWasLongPress)
    {
        radarTapCount++;
        radarLastTap = now;
        audio.beep(600, 20);
    }
    if (radarTapCount > 0 && now - radarLastTap > 400)
    {
        if (radarTapCount >= 5)
        {
            Serial.println("[Mood] Radar ping! Eyes up.");
            bleForceSweep();
            audio.radarPing();
        }
        else if (radarTapCount >= 3)
        {
            _setMood(ANNOYED);
            Serial.println("[Mood] Not now!");
            audio.annoyed();
        }
        radarTapCount = 0;
    }

    if (bleHasAlerts())
    {
        _lastAlertTime = now;
        if (mood != ENRAGED)
        {
            _setMood(ENRAGED);
            Serial.println("[Mood] !Radar Warning! Cyber threat in airspace.");
            audio.radarAlert();
        }
    }
    else if (meshHasRecentNewNode())
    {
        _lastAlertTime = now;
        if (mood != TACTICAL && mood != ENRAGED)
        {
            _setMood(TACTICAL);
            Serial.println("[Mood] # Tactical Scout: New Meshtastic node acquired.");
            audio.beep(800, 40);
        }
    }
    else if ((mood == ENRAGED || mood == TACTICAL) && (now - _lastAlertTime > THREAT_CLEAR_MS))
    {
        _setMood(VIGILANT);
        Serial.println("[Mood] Airspace clear / Nodes logged. Back on watch.");
        audio.radarOff();
    }
    else if (mood == ANNOYED && now - moodChangeTime > 4000)
    {
        _setMood(VIGILANT);
        Serial.println("[Mood] Calmed down, back on duty.");
    }
}

void triggerWardriving()
{
    _wardrivingActive = true;
    _wardrivingTouchLock = true;
    wardrivingBegin();
    _setMood(DRIVING);
    lastInteraction = millis();
    Serial.println("[Mood] === WARDRIVING MODE ACTIVATED ===");
    audio.beep(1000, 50);
    delay(80);
    audio.beep(1200, 50);
}

void exitWardriving()
{
    if (!_wardrivingActive)
        return;
    _wardrivingActive = false;
    wardrivingEnd();
    _setMood(baseMood());
    lastInteraction = millis();
    Serial.println("[Mood] === WARDRIVING MODE DEACTIVATED ===");
    audio.beep(900, 50);
}

static void _updateWardriving(uint32_t now)
{
    if (!isTouched)
        _wardrivingTouchLock = false;

    if (longTouchActive && !_wardrivingTouchLock)
    {
        exitWardriving();
        return;
    }

    static uint32_t lastWardrivingTap = 0;
    static bool waitingForSecondTap = false;
    const uint32_t DOUBLE_TAP_WINDOW_MS = 400;

    if (touchJustReleased && !touchWasLongPress)
    {
        if (waitingForSecondTap && (now - lastWardrivingTap) < DOUBLE_TAP_WINDOW_MS)
        {
            waitingForSecondTap = false;

            Serial.println("[Mood] Wardriving double-tap → Speedometer");
            audio.beep(800, 30);
            delay(50);
            audio.beep(1000, 30);

            openSpeedometerFromWardriving();
        }
        else
        {
            waitingForSecondTap = true;
            lastWardrivingTap = now;
        }
    }

    if (waitingForSecondTap && (now - lastWardrivingTap) > DOUBLE_TAP_WINDOW_MS)
    {
        waitingForSecondTap = false;
    }
}

void resumeWardrivingView()
{
    _wardrivingTouchLock = true;
    lastInteraction = millis();
}

#if defined(BOARD_XIAO_C5)
void triggerAlprHunter()
{
    _alprActive = true;
    _alprTouchLock = true;
    _lastAlprAlertTime = 0;

    if (wardrivingActive)
    {
        wardrivingEnd();
    }

    alprDetectorBegin();

    _setMood(HUNTING);
    lastInteraction = millis();
    Serial.println("[Mood] === ALPR HUNTER MODE ACTIVATED ===");
    audio.beep(1500, 50);
    delay(30);
    audio.beep(2000, 50);
    delay(30);
    audio.beep(2500, 50);
}

void exitAlprHunter()
{
    if (!_alprActive)
        return;

    _alprActive = false;
    alprDetectorEnd();
    bleForceResync();
    bleReset();

    _setMood(baseMood());
    lastInteraction = millis();
    Serial.println("[Mood] === ALPR HUNTER MODE DEACTIVATED ===");
    audio.beep(1000, 50);
    delay(30);
    audio.beep(700, 50);
}

void resumeAlprHunterView()
{
    _alprTouchLock = true;
    lastInteraction = millis();
}

static void _updateAlprHunter(uint32_t now)
{
    const uint32_t ALPR_ALERT_CLEAR_MS = 4000;

    if (gpsActive)
    {
        gpsUpdate();
    }

    if (!isTouched)
        _alprTouchLock = false;

    if (longTouchActive && !_alprTouchLock)
    {
        exitAlprHunter();
        return;
    }

    if (alprJustDetected)
    {
        alprJustDetected = false;
        _lastAlprAlertTime = now;

        if (mood != ALERT_CAMERA)
        {
            _setMood(ALERT_CAMERA);
            Serial.printf("[Mood] !!! %s %s DETECTED: %s @ RSSI %d\n",
                          alprVendorName(alprLastVendor),
                          alprDeviceTypeName(alprLastType),
                          alprLastMAC, (int)alprLastRSSI);
            audio.beep(2000, 40);
            delay(20);
            audio.beep(2800, 40);
            delay(20);
            audio.beep(2000, 40);
        }
    }

    if (mood == ALERT_CAMERA && (now - _lastAlprAlertTime > ALPR_ALERT_CLEAR_MS))
    {
        _setMood(HUNTING);
        Serial.println("[Mood] Back to hunting...");
    }
}
#endif

static void _updateNetworkState(uint32_t now, bool connected)
{
    if (connected != _wasConnected)
    {
        if (!connected && widsHasRecentAlert(15000))
        {
            _setMood(DEAD);
            Serial.println("[Mood] !!! KILLED BY DEATH! (WIRELESS ATTACK)");
            audio.dead();
        }
        else
        {
            _setMood(baseMood());
        }
        _wasConnected = connected;
        Serial.printf("[Mood] Network: %s\n", connected ? "Online" : "Offline");
    }

    if (connected && widsActive && widsHasRecentAlert(5000))
    {
        if (mood == SLEEPY || mood != ENRAGED)
        {
            _setMood(ENRAGED);
            lastInteraction = now;
            Serial.println("[Mood] WIDS Alert! Baring teeth...");
            audio.enraged();
        }
    }

    if (connected && mood != SLEEPY)
    {
        if (!netHealthIsUp && netHealthConsecutiveFails >= 3 && mood != DEAD)
        {
            _wasNetDown = true;
            if (mood != ENRAGED)
            {
                _setMood(ENRAGED);
                audio.enraged();
            }
        }
        else if (!netHealthIsUp && netHealthConsecutiveFails > 0 && netHealthConsecutiveFails < 3 && mood != ENRAGED && mood != ANNOYED)
        {
            _setMood(ANNOYED);
            audio.annoyed();
        }
    }
    if (connected && netHealthIsUp && _wasNetDown)
    {
        _wasNetDown = false;
        if (mood != SLEEPY)
            _reactJazzed();
    }
}

static void _updateMoodDecay(uint32_t now, bool connected)
{
    uint32_t idleTime = now - lastInteraction;
    if (isDribbleActive())
        return;

    bool inSpecialMode = (mood == SLEEPY || mood == DEAD || mood == DRIVING);
#if defined(BOARD_XIAO_C5)
    inSpecialMode = inSpecialMode || (mood == HUNTING || mood == ALERT_CAMERA);
#endif

    if (_sleepTimeoutMs > 0 && idleTime > _sleepTimeoutMs && !inSpecialMode)
    {
        _setMood(SLEEPY);
        audio.sleepy();
        Serial.println("[Mood] HonkShoo mimimi");
        return;
    }

    if (isTouched && mood == DEAD)
    {
        _setMood(NEUTRAL);
        audio.chirp();
        Serial.println("[Mood] Revived by boop!");
        return;
    }
    if (isTouched && mood == SLEEPY)
    {
        _setMood(baseMood());
        audio.chirp();
        return;
    }

    uint32_t age = now - moodChangeTime;
    if (mood == DEAD && age > 15000)
    {
        _setMood(NEUTRAL);
        Serial.println("[Mood] Resurrection complete");
        return;
    }
    else if (mood == ANNOYED && age > 4000)
    {
        _setMood(baseMood());
        return;
    }
    else if (mood == HAPPY && age > 8000)
    {
        _setMood(baseMood());
        return;
    }
    else if (mood == CURIOUS && age > 6000)
    {
        _setMood(baseMood());
        return;
    }
    else if (mood == JAZZED && age > 6000)
    {
        _setMood(baseMood());
        return;
    }
    else if (mood == ENRAGED)
    {
        bool threatNow = connected && widsActive && widsHasRecentAlert(5000);
        bool netDown = connected && !netHealthIsUp && netHealthConsecutiveFails >= 3;
        if (!threatNow && !netDown && age > 5000)
        {
            _setMood(baseMood());
            Serial.println("[Mood] Threat cleared. Calming down.");
            return;
        }
    }
}

static void _updateIdlePersonality(uint32_t now, bool connected)
{
    uint32_t idleTime = now - lastInteraction;
    if (mood != baseMood() || idleTime <= 10000)
        return;
    static uint32_t lastAutoRoll = 0;
    if (now - lastAutoRoll < 30000)
        return;
    lastAutoRoll = now;
    int roll = random(100);
    if (!connected && roll < 15)
        _reactCurious();
    else if (connected && roll < 20)
        _reactNetworkCheck();
}

static void _reloadSettings(uint32_t now)
{
    if (isMenuActive())
        return;
    if (now - _lastSettingsCheck < 10000)
        return;
    _lastSettingsCheck = now;
    Preferences prefs;
    if (prefs.begin("clunchi", true))
    {
        uint32_t savedSecs = prefs.getUInt("sleep_time", 60);
        _sleepTimeoutMs = (savedSecs > 0) ? (savedSecs * 1000) : 0;
        prefs.end();
    }
}

void moodUpdate(TouchEvent event)
{
    if (wifiIsPortalActive())
        return;

    uint32_t now = millis();
    bool connected = wifiConnected();

#if defined(BOARD_XIAO_C5)
    if (mood == BATTERY_STATUS) {
        if (!isTouched) {
            _batteryTouchLock = false;
        }
        
        if (longTouchActive && !_batteryTouchLock) {
            _setMood(baseMood());
            lastInteraction = now;
        }
        return; 
    }
#endif

    if (touchJustPressed)
    {
        lastInteraction = now;
        if (!isDribbleActive())
        {
            audio.chirp();
            animation.triggerSquish();
            animation.triggerBlink();
        }
    }

    if (_radarActive)
    {
        _updateRadar(now);
        return;
    }
    if (_wardrivingActive)
    {
        _updateWardriving(now);
        return;
    }
#if defined(BOARD_XIAO_C5)
    if (_alprActive)
    {
        _updateAlprHunter(now);
        return;
    }
#endif

    if (!isMenuActive())
    {
        _handleTouchEvent(event, now);
        now = millis();
    }
    _reloadSettings(now);
    _updateNetworkState(now, connected);
    _updateMoodDecay(now, connected);
    _updateIdlePersonality(now, connected);
}