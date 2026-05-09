#pragma once
#include <Arduino.h>
#include <U8g2lib.h>

struct Particle {
    int16_t x, y;
    int8_t  vx, vy;
    uint8_t life;
};

class DiceRoller {
public:
    DiceRoller(U8G2& u8g2) : u8g2_(u8g2) {}

    void draw(const char* dieLabel, int result, bool rolling, int frame, bool tiltMode);
    void reset();

private:
    U8G2& u8g2_;

    uint32_t rollStartTime_    = 0;
    float    spinSpeed_        = 0;
    float    currentRotation_  = 0;
    int      shakeX_           = 0;
    int      shakeY_           = 0;
    bool     hasCriticalBurst_ = false;

    static constexpr int MAX_PARTICLES = 12;
    Particle particles_[MAX_PARTICLES] = {};

    int  parseDieType(const char* label);
    void drawDieShape(int cx, int cy, int dieType);
    void drawDieWithPips(int cx, int cy, int dieType, int value, bool rolling);
    void drawD6Pips(int cx, int cy, int value);
    void drawNumber(int cx, int cy, int dieType, int value);
    void drawParticleBurst(int cx, int cy, float intensity);
};