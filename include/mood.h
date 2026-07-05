#pragma once
#include <Arduino.h>
#include "touch.h"

enum Mood
{
    NEUTRAL,
    HAPPY,
    SLEEPY,
    ANNOYED,
    CURIOUS,
    JAZZED,
    VIGILANT,
    ENRAGED,
    DEAD,
    DRIVING,
    TACTICAL,
#if defined(BOARD_XIAO_C5)
    HUNTING,      
    ALERT_CAMERA,   
#endif
    MOOD_COUNT
};

extern Mood mood;
extern uint32_t moodChangeTime;
extern uint32_t lastInteraction;

void moodBegin();
void moodUpdate(TouchEvent event);

void triggerRadar();
void exitRadar();
bool isRadarActive();

void triggerWardriving();
void exitWardriving();
bool isWardrivingMoodActive();
void resumeWardrivingView();

#if defined(BOARD_XIAO_C5)
    void triggerAlprHunter();
    void exitAlprHunter();
    bool isAlprHunterMoodActive();
    void resumeAlprHunterView();
#else
    inline void triggerAlprHunter() {}
    inline void exitAlprHunter() {}
    inline bool isAlprHunterMoodActive() { return false; }
    inline void resumeAlprHunterView() {}
#endif

const char *moodName(Mood m);
Mood baseMood();