#pragma once
#include <Arduino.h>
#include <U8g2lib.h>

class Magic8Ball {
public:
    Magic8Ball(U8G2& u8g2) : u8g2_(u8g2) {}

    void ask();
    bool isShaking() const { return shaking_; }
    void updateAndDraw(bool tiltMode);
    void reset();
    
    // --- ADDED MISSING GETTER DECLARATION ---
    static const char* getRandomQuote();

private:
    U8G2& u8g2_;

    bool     shaking_          = false;
    uint32_t shakeStartTime_   = 0;
    int      frame_            = 0;
    const char* quote_         = nullptr;
    float    shakeIntensity_   = 0;
    int      shakeX_           = 0;
    int      shakeY_           = 0;

    static const char* EIGHT_BALL_QUOTES_[16];
    static const uint32_t SHAKE_DURATION = 1500;

    void drawBallShape(int cx, int cy);
    void drawAnswerWindow(int cx, int cy, const char* quote);
};