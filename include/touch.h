#pragma once
#include <Arduino.h>
#include "config.h"

// ═══════════════════════════════════════════════════════
//  touch.h — Touch input + event detection
//
//  This module ONLY reads the touch pin and classifies
//  input into events. It does NOT set mood, trigger
//  animations, play sounds, or call any other module.
//
//  The event is consumed by mood.cpp via consumeTouchEvent()
// ═══════════════════════════════════════════════════════

enum class TouchEvent : uint8_t {
    NONE,
    TAP_1,
    TAP_2,
    TAP_3,
    TAP_4,
    TAP_5,
    TAP_6_PLUS,
    TAP_10_PLUS,
    LONG_PRESS,
    DRIBBLE_HIT,
    DRIBBLE_END,
};

// ── Raw state (read-only outside touch.cpp) ──────────
extern bool          isTouched;
extern bool          touchJustPressed;
extern bool          touchJustReleased;
extern bool          longTouchActive;
extern bool          touchWasLongPress;
extern unsigned long touchDownTime;
extern unsigned long touchUpTime;

// ── API ──────────────────────────────────────────────
void       calibrateTouch();
void       handleTouch();         // call every loop — reads pin
void       evaluateTaps();        // call every loop — classifies taps
TouchEvent consumeTouchEvent();   // returns event and clears it
void       resetTapCount();
bool       isDribbleActive();     // so mood.cpp can check

// ── Diagnostics ─────────────────────────────────────
int  readTouchRaw();
int  getTouchBaseline();
bool readTouch();