#pragma once
#include <Arduino.h>
#include <U8g2lib.h>
#include "config.h"
#include "animation.h"
#include "mood.h"

class Display {
public:
    bool begin();
    void clear();
    void render();

    void drawFace(Mood currentMood, AnimState anim);
    void drawStatusBar(bool touched, int volume);
    void drawMenu(const char* title, const char** items, uint8_t itemCount, int selectedIdx, int arrowIdx = -1);
    void drawConfirm(const char* line1, const char* line2);
    void drawSplash();
    void drawCentered(const char* text, int y);
    void drawSpeedometer(double speed, const char* unit, bool hasFix, int sats);
    void drawClock(const char* time, const char* date, const char* timezone);

private:
    void drawSleepZzz(int frame);
    void drawHappySparkles();
    void drawHeart(int x, int y);
    void drawCuriousQuestion();
    void drawMusicNotes();
    void drawRadarSweep();
    void drawAngryAura();
    void drawAlertMarks();
    void drawSpiralEye(int cx, int cy, int radius, float angle);
   
    

    U8G2_SH1106_128X64_NONAME_F_HW_I2C u8g2_{U8G2_R0, U8X8_PIN_NONE};
    bool ready_ = false;
};