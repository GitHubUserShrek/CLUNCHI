#pragma once
#include <Arduino.h>
#include <U8g2lib.h>
#include "config.h"
#include "animation.h"
#include "mood.h"
#include "dice_roller.h"
#include "blackjack.h"
#include "card_bitmaps.h"
#include "magic_8ball.h"

class Display
{
public:
    struct FacePos {
        int leftX, rightX;
        int eyeYLeft, eyeYRight;
        int mouthY, mouthCX;
        int eyeHeight;
    };

    bool begin();
    void clear();
    void render();

    void drawFace(Mood currentMood, AnimState anim);
    void drawStatusBar(bool touched, int volume);
    void drawMenu(const char *title, const char **items, uint8_t itemCount, int selectedIdx, int arrowIdx = -1);
    void drawConfirm(const char *line1, const char *line2);
    void drawSplash();
    void drawCentered(const char *text, int y);
    void drawSpeedometer(double speed, const char *unit, bool hasFix, int sats);
    void drawClock(const char *time, const char *date, const char *timezone);
    void diceNext() { diceRoller_.nextDie(); }
    void diceRoll() { diceRoller_.startRoll(); }
    bool diceIsRolling() const { return diceRoller_.isRolling(); }
    void diceReset() { diceRoller_.reset(); }
    void drawDiceScreen(bool tiltMode);

    void m8bAsk() { magic8Ball_.ask(); }
    bool m8bIsShaking() const { return magic8Ball_.isShaking(); }
    void m8bReset() { magic8Ball_.reset(); }
    void drawMagic8BallScreen(bool tiltMode);

    void bjHit() { blackjack_.hit(); }
    void bjStand() { blackjack_.stand(); }
    void bjReset() { blackjack_.reset(); }
    void drawBlackjackScreen(bool tiltMode);

    void drawRssiBars(int x, int y, int bars);

    void drawBatteryFace(int percentage, float voltage, bool isLow);


    #if defined(BOARD_XIAO_C5)
    void drawHuntingReticle();
    void drawAlprAlertMarks();
#endif

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
    void drawAttackFace(AnimState anim);

    
    void drawEyesSleepy(const FacePos &p);
    void drawEyesJazzed(const FacePos &p);
    void drawEyesSpiral(const AnimState &anim, const FacePos &p);
    void drawEyesHeart(const FacePos &p);
    void drawEyesCurious(const AnimState &anim, const FacePos &p);
    void drawEyesVigilant(const AnimState &anim, const FacePos &p);
    bool drawEyesTactical(const AnimState &anim, const FacePos &p);
    void drawEyesDriving(const FacePos &p);
    void drawEyesEnraged(const AnimState &anim, const FacePos &p);
    void drawEyesDead(const FacePos &p);
    void drawEyesDefault(const AnimState &anim, const FacePos &p, Mood currentMood);
    
    void drawMouthForMood(Mood currentMood, const AnimState &anim, const FacePos &p);
    
    void drawMoodDecorations(Mood currentMood, const AnimState &anim);
    void drawVigilantFooter();
    void drawTacticalFooter();
    void drawDrivingFooter();
    
    void drawBeaconFloodFace(const AnimState &anim, const FacePos &p, int eyeR);
    
    #if defined(BOARD_XIAO_C5)
    void drawEyesHunting(const AnimState &anim, const FacePos &p);
    void drawEyesAlertCamera(const FacePos &p);
    void drawAlprHunterFooter(Mood currentMood);
    #endif

    FacePos computeFacePos(const AnimState &anim);

    U8G2_SH1106_128X64_NONAME_F_HW_I2C u8g2_{U8G2_R0, U8X8_PIN_NONE};
    bool ready_ = false;

    DiceRoller diceRoller_{u8g2_};
    Magic8Ball magic8Ball_{u8g2_};
    Blackjack blackjack_{u8g2_};
};