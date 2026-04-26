// ═══════════════════════════════════════════════════════
//  main.cpp — System bootstrap and main loop
//
//  V2 responsibilities:
//    • Owns all module instances
//    • Calls modules in the correct order each loop()
//    • Handles WiFi lifecycle (deauth/netHealth/dashboard)
//    • Checks menuWantsRadar() and calls triggerRadar()
//    • Passes TouchEvent from touch → mood
//    • Does NOT set mood directly
//    • Does NOT trigger animations directly
//    • Does NOT play sounds directly
// ═══════════════════════════════════════════════════════

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

// ── Module instances (owned here, extern'd elsewhere) ─
Display   display;
Audio     audio;
Animation animation;

// ── Frame timing ──────────────────────────────────────
static uint32_t       lastFrame = 0;
static const uint32_t frameTime = 1000 / FPS_TARGET;

// ─────────────────────────────────────────────────────
//  WiFi lifecycle
//  The ONLY place deauth/netHealth/dashboard start/stop.
//  Reacts to one-shot flags from wifi_manager.
// ─────────────────────────────────────────────────────
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

// ─────────────────────────────────────────────────────
//  Periodic status log
// ─────────────────────────────────────────────────────
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
        Serial.println("[CLUNCHI] WiFi:-- | IP:0.0.0.0");
    }
}

// ─────────────────────────────────────────────────────
//  Setup
// ─────────────────────────────────────────────────────
void setup() {
    Serial.begin(115200);
    delay(900);
    Serial.println("\n[System] CLUNCHI V2 Booting...");

    // NVS namespace init
    Preferences nvsInit;
    nvsInit.begin("clunchi", false); nvsInit.end();
    nvsInit.begin("wifi",    false); nvsInit.end();
    Serial.println("[System] NVS ready.");

    // Audio first — gives boot feedback
    audio.begin();
    audio.beep(1000, 50);

    // Display
    if (!display.begin()) {
        Serial.println("[System] Display FAILED. Halting.");
        while (true) { audio.beep(200, 100); delay(500); }
    }

    // WiFi radio init — does NOT connect yet
    // Connection fires automatically if credentials exist
    wifiBegin();

    // Splash while touch calibrates
    display.drawSplash();
    audio.chirp();
    delay(800);

    // Input + UI
    calibrateTouch();
    menuBegin();
    animation.begin();

    // Mood machine
    moodBegin();

    Serial.println("[System] Boot complete.");
}

// ─────────────────────────────────────────────────────
//  Main loop
// ─────────────────────────────────────────────────────
void loop() {
    uint32_t now = millis();

    // ── Background systems ────────────────────────────
    // Non-blocking — must run every loop() regardless
    // of frame rate, menu state, or portal mode
    wifiUpdate();
    wifiProcessPortal();
    handleWifiLifecycle();

    if (isRadarActive()) bleUpdate();
    deauthDetectorUpdate();
    netHealthUpdate();
    dashboardUpdate();

    // ── Portal mode ───────────────────────────────────
    // Minimal loop — just keep portal alive and allow
    // menu interaction to exit portal if needed
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

    // ── Block while connecting ────────────────────────
    if (connectState == CONN_TRYING) {
        delay(1);
        return;
    }

    // ── Frame rate gate ───────────────────────────────
    if (now - lastFrame < frameTime) return;
    lastFrame = now;

    // ── Input ─────────────────────────────────────────
    handleTouch();

    // ── Menu mode ─────────────────────────────────────
    if (isMenuActive()) {
        menuUpdate();

        // Radar is requested via flag, not direct call.
        // Checked here so triggerRadar() runs in the
        // correct context outside of menu internals.
        if (menuWantsRadar()) {
            triggerRadar();
        }
        return;
    }

    // ── Face mode ─────────────────────────────────────

    // Radar mode handles its own tap logic inside moodUpdate()
    // using raw touch globals — evaluateTaps() is skipped
    if (!isRadarActive()) {
        evaluateTaps();
    }

    // Consume the event (always — mood decides what to do with it)
    TouchEvent ev = consumeTouchEvent();

    // Mood is the single brain — receives event, owns all reactions
    moodUpdate(ev);

    // Animation math — pure, no side effects
    animation.update(mood);

    // Render
    display.drawFace(mood, animation.getState());
    display.drawStatusBar(isTouched, audio.getVolume());

    // Periodic serial log
    handlePeriodicLog(now);
}