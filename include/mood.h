#pragma once
#include <Arduino.h>
#include "touch.h"

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
    DRIVING,
    MOOD_COUNT
};

extern Mood     mood;
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

const char* moodName(Mood m);
Mood        baseMood();