#include <Arduino.h>
#include <Preferences.h>
#include "config.h"
#include "display.h"
#include "audio.h"
#include "animation.h"
#include "touch.h"
#include "menu.h"
#include "mood.h"
#include "wifi_manager.h"
#include "ble_manager.h"
#include "wids.h"
#include "net_health.h"
#include "web_dashboard.h"
#include "gps_manager.h"
#include "sd_manager.h"
#include "wardriving.h"
#include "alpr_detector.h"
#include "tilt.h"
#include "battery_manager.h"

Display display;
Audio audio;
Animation animation;

static uint32_t lastFrame = 0;
static const uint32_t frameTime = 1000 / FPS_TARGET;

static void handleWifiLifecycle()
{
    if (wifiJustConnected())
    {
        Serial.println("[System] WiFi up — starting background systems.");
        widsBegin();
        netHealthBegin();
        dashboardBegin();
    }
    if (wifiJustDisconnected())
    {
        Serial.println("[System] WiFi lost — stopping background systems.");
        widsEnd();
        netHealthEnd();
        dashboardEnd();
    }
}

static void handlePeriodicLog(uint32_t now)
{
    static uint32_t lastPrint = 0;
    if (now - lastPrint < 90000)
        return;
    lastPrint = now;

    if (wifiConnected())
    {
        uint32_t up = wifiConnUptime();
        uint32_t hrs = up / 3600;
        uint32_t mins = (up % 3600) / 60;
        uint32_t secs = up % 60;

        Serial.printf(
            "[CLUNCHI Status] WiFi:OK | IP:%s | RSSI:%lddBm | Up:%luh%lum%lus | Grade:%s | WIDS Threat:%d/100 (Incidents:%lu)\n",
            wifiIP().c_str(),
            (long)wifiRSSI(),
            (unsigned long)hrs,
            (unsigned long)mins,
            (unsigned long)secs,
            netGradeLabel(nhStats.grade),
            widsThreatScore(),
            (unsigned long)widsTotalCount);
    }
    else
    {
        Serial.println("[CLUNCHI Status] Disconnected. Take me to your WiFi.");
    }
}

void setup()
{
    Serial.begin(115200);
    delay(900);
    Serial.println("\n[System] CLUNCHI v1.0 Booting...");

    Preferences nvsInit;
    nvsInit.begin("clunchi", false);
    nvsInit.end();
    nvsInit.begin("wifi", false);
    nvsInit.end();
    Serial.println("[System] NVS ready.");

    audio.begin();
    audio.beep(1000, 50);

    if (!display.begin())
    {
        Serial.println("[System] Display FAILED. Halting.");
        while (true)
        {
            audio.beep(200, 100);
            delay(500);
        }
    }

    wifiBegin();
    gpsBegin();
    gpsLoadTimeSettings();
    batteryBegin();
    sdBegin();

#if BLE_INIT_AT_BOOT
    bleBegin();
    Serial.println("[System] BLE initialized at boot.");
#endif

    display.drawSplash();
    audio.chirp();
    delay(800);

    calibrateTouch();
    tiltBegin();
    tiltLoadSettings();
    menuBegin();
    animation.begin();

    moodBegin();

    Serial.println("[System] Boot complete.");
}

void loop()
{
    uint32_t now = millis();

    wifiUpdate();
    wifiProcessPortal();
    handleWifiLifecycle();
    gpsUpdate();
    batteryUpdate();

    if (isRadarActive())
        bleUpdate();
    if (widsActive)
        widsUpdate();
    if (nhActive && !widsHasRecentAlert(5000))
        netHealthUpdate();
    if (isDashboardActive())
        dashboardUpdate();
    if (sdActive)
        sdUpdate();
    if (wardrivingActive)
    {
        wardrivingUpdate();
        gpsPrintStatus();
    }

#if defined(BOARD_XIAO_C5)
    if (alprDetectorActive)
    {
        alprDetectorUpdate();
    }

#endif

    if (wifiIsPortalActive())
    {
        handleTouch();
        static uint32_t lastPortalDraw = 0;
        if (now - lastPortalDraw > 500)
        {
            lastPortalDraw = now;
            menuUpdate();
        }
        delay(1);
        return;
    }

    if (connectState == CONN_TRYING)
    {
        delay(1);
        return;
    }

    if (now - lastFrame < frameTime)
        return;
    lastFrame = now;

    handleTouch();
    tiltUpdate();

    if (tiltEnabled() && !isMenuActive() && !wardrivingActive &&
        !isRadarActive() && mood != SLEEPY
#if defined(BOARD_XIAO_C5)
        && !alprDetectorActive
#endif
    )
    {
        if (isDribbleActive())
        {
            if (tiltSingleHit())
                triggerDribbleFromShake();
        }
        else
        {
            if (tiltShakeDetected())
                triggerDribbleFromShake();
        }
    }

    if (isMenuActive())
    {
        menuUpdate();
        lastInteraction = now;
        return;
    }

    if (!isRadarActive() && !isWardrivingActive()
#if defined(BOARD_XIAO_C5)
        && !isAlprDetectorActive()
#endif
    )
    {
        evaluateTaps();
    }



    handlePeriodicLog(now);
    TouchEvent ev = consumeTouchEvent();
    moodUpdate(ev);
    animation.update(mood);

#if defined(BOARD_XIAO_C5)
    if (mood == BATTERY_STATUS || mood == LOW_BATTERY)
    {
        display.drawBatteryFace(getBatteryPercentage(), getBatteryVoltage(), mood == LOW_BATTERY);
    }
    else
#endif
    {
        display.drawFace(mood, animation.getState());
    }

    display.drawStatusBar(isTouched, audio.getVolume());

    handlePeriodicLog(now);
}