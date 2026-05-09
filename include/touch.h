#pragma once
#include <Arduino.h>
#include "config.h"

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

extern bool          isTouched;
extern bool          touchJustPressed;
extern bool          touchJustReleased;
extern bool          longTouchActive;
extern bool          touchWasLongPress;
extern unsigned long touchDownTime;
extern unsigned long touchUpTime;

void       calibrateTouch();
void       handleTouch();       
void       evaluateTaps();       
TouchEvent consumeTouchEvent();  
void       resetTapCount();
bool       isDribbleActive();   

void triggerDribbleFromShake();

int  readTouchRaw();
int  getTouchBaseline();
bool readTouch();