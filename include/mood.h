#pragma once
#include <Arduino.h>
#include "touch.h"

// ═══════════════════════════════════════════════════════
//  mood.h — Emotional state machine
//
//  OWNS: mood state, all transitions, all reactions
//  CONSUMES: TouchEvent, net health, deauth, BLE alerts
//  CONTROLS: audio, animation triggers
//  DOES NOT: read pins, draw anything, serve web pages
// ═══════════════════════════════════════════════════════

enum Mood {
    NEUTRAL,
    HAPPY,
    SLEEPY,
    ANNOYED,
    CURIOUS,
    JAZZED,
    VIGILANT,
    ENRAGED,
    DEAD,
    MOOD_COUNT
};

// ── Public state (read-only outside mood.cpp) ────────
extern Mood     mood;
extern uint32_t moodChangeTime;
extern uint32_t lastInteraction;

// ── API ──────────────────────────────────────────────
void moodBegin();                          // call once in setup()
void moodUpdate(TouchEvent event);         // call every loop()

// ── Radar mode ───────────────────────────────────────
void triggerRadar();
void exitRadar();
bool isRadarActive();

// ── Mood info ────────────────────────────────────────
const char* moodName(Mood m);
Mood        baseMood();