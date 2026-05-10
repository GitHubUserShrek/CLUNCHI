#include "touch.h"
#include "config.h"
#include "animation.h"     
#include "menu.h"  
#include "tilt.h"

extern Animation animation;  

bool          isTouched         = false;
bool          touchJustPressed  = false;
bool          touchJustReleased = false;
bool          longTouchActive   = false;
bool          touchWasLongPress = false;
unsigned long touchDownTime     = 0;
unsigned long touchUpTime       = 0;

static int           touchBaseline_  = 0; 
static int           tapCount_       = 0;
static uint32_t      firstTapTime_   = 0;
static bool          dribbleActive_  = false;
static uint32_t      lastDribbleTap_ = 0;
static TouchEvent    pendingEvent_   = TouchEvent::NONE;

int  readTouchRaw()     { return digitalRead(PIN_TOUCH); }
int  getTouchBaseline() { return touchBaseline_; }
bool isDribbleActive()  { return dribbleActive_ || animation.isDribbling(); }

bool readTouch() {
    return (digitalRead(PIN_TOUCH) == LOW);
}

void calibrateTouch() {
    pinMode(PIN_TOUCH, INPUT_PULLUP);
    
    delay(50); 
    
    touchBaseline_ = 1; 
    Serial.println("[Touch] Digital Input Ready (Button or TTP223)");
}

void resetTapCount() {
    tapCount_ = 0;
    dribbleActive_ = false;
}

TouchEvent consumeTouchEvent() {
    TouchEvent ev = pendingEvent_;
    pendingEvent_ = TouchEvent::NONE;
    return ev;
}

void handleTouch() {
    bool touched = readTouch();
    uint32_t now = millis();

    touchJustPressed  = false;
    touchJustReleased = false;
    touchWasLongPress = false;

    if (touched && !isTouched) {
        touchJustPressed = true;
        touchDownTime    = now;
    }
    else if (!touched && isTouched) {
        touchJustReleased = true;
        touchUpTime       = now;
        touchWasLongPress = longTouchActive;
        longTouchActive   = false;
    }

    if (touched && !longTouchActive && (now - touchDownTime >= LONG_TOUCH_TIME)) {
        longTouchActive = true;
    }

    isTouched = touched;
}

void triggerDribbleFromShake() {
    if (isMenuActive()) return;

    if (dribbleActive_ || animation.isDribbling()) {
        lastDribbleTap_ = millis();
        pendingEvent_   = TouchEvent::DRIBBLE_HIT;
        Serial.println("[Tilt] SHAKE -> BOING!");
        return;
    }

    dribbleActive_  = true;
    lastDribbleTap_ = millis();
    tapCount_       = 0;
    pendingEvent_   = TouchEvent::TAP_10_PLUS;
    Serial.println("[Tilt] SHAKE -> DRIBBLE MODE!");
}

void evaluateTaps() {
    uint32_t now = millis();

    if (dribbleActive_) {
        if (touchJustPressed) {
            lastDribbleTap_ = now;
            pendingEvent_ = TouchEvent::DRIBBLE_HIT;
            Serial.println("[Touch] BOING!");
        }

        if (now - lastDribbleTap_ > 3000) {
            dribbleActive_ = false;
            tapCount_ = 0;
            pendingEvent_ = TouchEvent::DRIBBLE_END;
        }
        return;
    }

    if (touchJustPressed) {
        if (tapCount_ == 0) firstTapTime_ = now;
        tapCount_++;
        if (now - firstTapTime_ > TAP_SEQUENCE_TIME) {
            tapCount_ = 1;
            firstTapTime_ = now;
        }

        if (tapCount_ >= 10 && !dribbleActive_) {
            dribbleActive_ = true;
            lastDribbleTap_ = now;
            tapCount_ = 0;
            pendingEvent_ = TouchEvent::TAP_10_PLUS;
            Serial.println("[Touch] WOOOO!!!");
            return;
        }
    }

    if (longTouchActive && tapCount_ > 0) {
        tapCount_ = 0;
        pendingEvent_ = TouchEvent::LONG_PRESS;
    }

    if (!isTouched && tapCount_ > 0 && (now - touchUpTime > 500)) {

        if (tapCount_ >= 6) {
            pendingEvent_ = TouchEvent::TAP_6_PLUS;
        }
        else if (tapCount_ == 5) {
            pendingEvent_ = TouchEvent::TAP_5;
        }
        else if (tapCount_ == 4) {
            pendingEvent_ = TouchEvent::TAP_4;
        }
        else if (tapCount_ == 3) {
            pendingEvent_ = TouchEvent::TAP_3;
        }
        else if (tapCount_ == 2) {
            pendingEvent_ = TouchEvent::TAP_2;
        }
        else if (tapCount_ == 1) {
            pendingEvent_ = TouchEvent::TAP_1;
        }

        tapCount_ = 0;
    }
}