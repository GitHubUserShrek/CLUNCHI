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
#include "deauth_detector.h"
#include "net_health.h"
#include "web_dashboard.h"
#include "gps_manager.h"
#include "sd_manager.h"
#include "wardriving.h"

Display   display;
Audio     audio;
Animation animation;

static uint32_t       lastFrame = 0;
static const uint32_t frameTime = 1000 / FPS_TARGET;

static void handleWifiLifecycle() {
    if (wifiJustConnected()) {
        Serial.println("[System] WiFi up — starting background systems.");
        deauthDetectorBegin();
        netHealthBegin();
        dashboardBegin();
    }
    if (wifiJustDisconnected()) {
        Serial.println("[System] WiFi lost — stopping background systems.");
        deauthDetectorEnd();
        netHealthEnd();
        dashboardEnd();
    }
}

static void handlePeriodicLog(uint32_t now) {
    static uint32_t lastPrint = 0;
    if (now - lastPrint < 90000) return;
    lastPrint = now;

    if (wifiConnected()) {
        uint32_t up   = wifiConnUptime();
        uint32_t hrs  = up / 3600;
        uint32_t mins = (up % 3600) / 60;
        uint32_t secs = up % 60;
        Serial.printf(
            "[CLUNCHI] WiFi:OK | IP:%s | RSSI:%lddBm | "
            "Up:%luh%lum%lus | Grade:%s\n",
            wifiIP().c_str(),
            (long)wifiRSSI(),
            (unsigned long)hrs,
            (unsigned long)mins,
            (unsigned long)secs,
            netGradeLabel(nhStats.grade));
    } else {
        Serial.println("[CLUNCHI] Take me to your WiFi");
    }
}

void setup() {
    Serial.begin(115200);
    delay(900);
    Serial.println("\n[System] CLUNCHI BETA V1.0 Booting...");

    Preferences nvsInit;
    nvsInit.begin("clunchi", false); nvsInit.end();
    nvsInit.begin("wifi",    false); nvsInit.end();
    Serial.println("[System] NVS ready.");

    audio.begin();
    audio.beep(1000, 50);

    if (!display.begin()) {
        Serial.println("[System] Display FAILED. Halting.");
        while (true) { audio.beep(200, 100); delay(500); }
    }

    wifiBegin();
    gpsBegin();
    gpsLoadTimeSettings(); 
    sdBegin();

    display.drawSplash();
    audio.chirp();
    delay(800);

    calibrateTouch();
    menuBegin();
    animation.begin();

    moodBegin();

    Serial.println("[System] Boot complete.");
}

void loop() {
    uint32_t now = millis();

    wifiUpdate();
    wifiProcessPortal();
    handleWifiLifecycle();
    gpsUpdate();

    if (isRadarActive())      bleUpdate();
    if (deauthDetectorActive) deauthDetectorUpdate();
    if (nhActive)             netHealthUpdate();
    if (isDashboardActive())  dashboardUpdate();
    if (sdActive)             sdUpdate();
    if (wardrivingActive) {
        wardrivingUpdate();
        gpsPrintStatus();
    }

    if (wifiIsPortalActive()) {
        handleTouch();

        static uint32_t lastPortalDraw = 0;
        if (now - lastPortalDraw > 500) {
            lastPortalDraw = now;
            menuUpdate();
        }

        delay(1);
        return;
    }

    if (connectState == CONN_TRYING) {
        delay(1);
        return;
    }

    if (now - lastFrame < frameTime) return;
    lastFrame = now;

    handleTouch();

    if (isMenuActive()) {
        menuUpdate();
        return;
    }

    if (!isRadarActive()) {
        evaluateTaps();
    }

    TouchEvent ev = consumeTouchEvent();
    moodUpdate(ev);
    animation.update(mood);

    display.drawFace(mood, animation.getState());
    display.drawStatusBar(isTouched, audio.getVolume());

    handlePeriodicLog(now);
}