#include "display.h"
#include "wardriving.h"
#include "gps_manager.h"
#include "ble_manager.h"
#include "dice_roller.h"
#include "logo_bitmaps.h"
#include "mesh_wardrive.h"
#include "magic_8ball.h"
#include "wids.h"
#include "alpr_detector.h"
#include <Wire.h>

static const int BASE_EYE_Y = 26;
static const int BASE_LEFT_X = 38;
static const int BASE_RIGHT_X = 90;
static const int EYE_R = 7;
static const int BASE_MOUTH_Y = 46;
static const int BASE_MOUTH_CX = 64;

Display::FacePos Display::computeFacePos(const AnimState &anim)
{
    Display::FacePos p;
    int offsetY = (int)anim.bounceY + (int)anim.breathY + (int)anim.squishY;
    
    p.leftX = BASE_LEFT_X + (int)anim.leftEyeOffX;
    p.rightX = BASE_RIGHT_X + (int)anim.rightEyeOffX;
    p.eyeYLeft = BASE_EYE_Y + offsetY + (int)anim.leftEyeOffY;
    p.eyeYRight = BASE_EYE_Y + offsetY + (int)anim.rightEyeOffY;
    p.mouthY = BASE_MOUTH_Y + offsetY + (int)anim.mouthOffY;
    p.mouthCX = BASE_MOUTH_CX + (int)anim.mouthOffX;
    
    p.eyeHeight = EYE_R * 2;
    if (anim.blink > 0)
        p.eyeHeight = std::max(2, (int)(p.eyeHeight * (1.0f - anim.blink)));
    
    return p;
}

bool Display::begin()
{
    Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL, I2C_FREQ);
    delay(10);

    u8g2_.setI2CAddress(OLED_ADDR << 1);
    if (!u8g2_.begin())
        return false;

    u8g2_.setBusClock(I2C_FREQ);
    u8g2_.clearDisplay();
    u8g2_.setFontMode(1);
    u8g2_.setDrawColor(1);
    ready_ = true;
    return true;
}

void Display::clear()
{
    if (ready_) u8g2_.clearBuffer();
}

void Display::render()
{
    if (ready_) u8g2_.sendBuffer();
}

void Display::drawCentered(const char *text, int y)
{
    int w = u8g2_.getStrWidth(text);
    u8g2_.drawStr((OLED_WIDTH - w) / 2, y, text);
}

void Display::drawSplash()
{
    clear();
    u8g2_.setFont(u8g2_font_logisoso16_tr);
    drawCentered("CLUNCHI", 26);
    u8g2_.setFont(u8g2_font_5x7_tr);
    drawCentered("BETA 1.0", 44);
    u8g2_.drawRFrame(0, 0, OLED_WIDTH, OLED_HEIGHT, 8);
    render();
}

void Display::drawSpiralEye(int cx, int cy, int radius, float angle)
{
    const int arms = 2;
    const float armSpacing = 3.5f;
    const float thetaStep = 0.15f;
    const float maxTheta = radius * (2.0f * PI) / armSpacing;

    for (int arm = 0; arm < arms; arm++)
    {
        float armOffset = arm * (2.0f * PI / arms);
        for (float theta = 0.5f; theta < maxTheta; theta += thetaStep)
        {
            float r = (armSpacing / (2.0f * PI)) * theta;
            if (r > radius) break;
            float drawAngle = theta + angle + armOffset;
            int px = cx + (int)(cos(drawAngle) * r);
            int py = cy + (int)(sin(drawAngle) * r);
            if (px >= 0 && px < OLED_WIDTH && py >= 0 && py < OLED_HEIGHT)
                u8g2_.drawPixel(px, py);
        }
    }
    u8g2_.drawDisc(cx, cy, 1);
    u8g2_.drawCircle(cx, cy, radius);
}

static void drawEvilTwinFace(U8G2 &u8g2, const AnimState &anim, const Display::FacePos &p, int eyeR)
{
    uint32_t now = millis();

    bool seamFlicker = (now / 90) % 4 != 0;
    if (seamFlicker)
    {
        for (int y = 14; y < 52; y += 3)
        {
            int jitter = ((now / 50 + y) % 3) - 1;
            u8g2.drawVLine(p.mouthCX + jitter, y, 2);
        }
        u8g2.drawPixel(p.mouthCX + 4, 18 + (int)((now / 70) % 6));
        u8g2.drawPixel(p.mouthCX - 4, 40 - (int)((now / 60) % 6));
    }

    u8g2.drawDisc(p.leftX, p.eyeYLeft, eyeR);
    u8g2.setDrawColor(0);
    u8g2.drawDisc(p.leftX + 2 + anim.pupilX, p.eyeYLeft - 1 + anim.pupilY, 2);
    u8g2.setDrawColor(1);

    if ((now / 400) % 2)
    {
        int spkX = p.leftX - 16, spkY = p.eyeYLeft - 11;
        u8g2.drawPixel(spkX, spkY);
        u8g2.drawPixel(spkX - 1, spkY + 1);
        u8g2.drawPixel(spkX + 1, spkY + 1);
        u8g2.drawPixel(spkX, spkY + 2);
    }

    u8g2.drawCircle(p.mouthCX, p.mouthY - 8, 12, U8G2_DRAW_LOWER_LEFT);

    u8g2.drawTriangle(p.rightX - 7, p.eyeYRight, p.rightX + 7, p.eyeYRight, p.rightX, p.eyeYRight - 7);
    u8g2.drawTriangle(p.rightX - 7, p.eyeYRight, p.rightX + 7, p.eyeYRight, p.rightX, p.eyeYRight + 7);
    u8g2.setDrawColor(0);
    int impPupil = ((now / 200) % 2) ? 2 : 1;
    u8g2.drawDisc(p.rightX + anim.pupilX, p.eyeYRight + anim.pupilY, impPupil);
    u8g2.setDrawColor(1);
    if ((now / 120) % 3 == 0)
        u8g2.drawHLine(p.rightX - 6, p.eyeYRight - 1, 12);
    u8g2.drawLine(p.rightX + 8, p.eyeYRight - 13, p.rightX - 4, p.eyeYRight - 8);
    u8g2.drawLine(p.rightX + 8, p.eyeYRight - 12, p.rightX - 4, p.eyeYRight - 7);

    float grinCycle = fmod(now / 2200.0f, 1.0f);
    int grinLift = 2 + (int)(sin(grinCycle * PI) * 3.0f);
    int rMouthY = p.mouthY + 2;

    int gStartX = p.mouthCX;
    int gEndX = p.mouthCX + 16;
    int span = gEndX - gStartX;

    int prevX = gStartX;
    int prevY = rMouthY + 2;
    for (int i = 1; i <= 8; i++)
    {
        float t = (float)i / 8.0f;
        int x = gStartX + (int)(span * t);
        int y = (rMouthY + 2) - (int)((grinLift + 4) * (t * t));
        u8g2.drawLine(prevX, prevY, x, y);
        prevX = x;
        prevY = y;
    }

    u8g2.drawTriangle(prevX - 4, prevY + 2, prevX, prevY + 2, prevX - 2, prevY + 6);
}

static void drawCtsJammingFace(U8G2 &u8g2, const AnimState &anim, const Display::FacePos &p, int eyeR)
{
    u8g2.drawDisc(p.leftX, p.eyeYLeft, eyeR + 1);
    u8g2.drawDisc(p.rightX, p.eyeYRight, eyeR + 1);
    u8g2.setDrawColor(0);
    u8g2.drawDisc(p.leftX + anim.pupilX, p.eyeYLeft + anim.pupilY, 2);
    u8g2.drawDisc(p.rightX + anim.pupilX, p.eyeYRight + anim.pupilY, 2);
    u8g2.setDrawColor(1);
    
    u8g2.drawBox(p.mouthCX - 12, p.mouthY - 4, 24, 8);
    u8g2.setDrawColor(0);
    for (int x = p.mouthCX - 10; x < p.mouthCX + 10; x += 5)
        u8g2.drawLine(x, p.mouthY - 4, x + 3, p.mouthY + 3);
    u8g2.setDrawColor(1);
}

static void drawHandshakeCaptureFace(U8G2 &u8g2, const AnimState &anim, const Display::FacePos &p, int eyeR)
{
    u8g2.drawDisc(p.leftX, p.eyeYLeft, eyeR);
    u8g2.drawDisc(p.rightX, p.eyeYRight, eyeR);
    u8g2.setDrawColor(0);
    u8g2.drawDisc(p.leftX + 2 + anim.pupilX, p.eyeYLeft - 1 + anim.pupilY, 2);
    u8g2.drawDisc(p.rightX + 2 + anim.pupilX, p.eyeYRight - 1 + anim.pupilY, 2);
    u8g2.setDrawColor(1);
    
    int wave = (int)(sin(millis() / 150.0f) * 2.0f);
    u8g2.drawLine(p.mouthCX - 8, p.mouthY + wave, p.mouthCX - 4, p.mouthY - wave);
    u8g2.drawLine(p.mouthCX - 4, p.mouthY - wave, p.mouthCX, p.mouthY + wave);
    u8g2.drawLine(p.mouthCX, p.mouthY + wave, p.mouthCX + 4, p.mouthY - wave);
    u8g2.drawLine(p.mouthCX + 4, p.mouthY - wave, p.mouthCX + 8, p.mouthY + wave);
    
    int lx = 112, ly = 48;
    u8g2.drawFrame(lx, ly + 4, 8, 7);
    u8g2.drawCircle(lx + 4, ly + 4, 3, U8G2_DRAW_UPPER_LEFT);
    u8g2.drawVLine(lx + 7, ly + 1, 3);
}

void Display::drawBeaconFloodFace(const AnimState &anim, const Display::FacePos &p, int eyeR)
{
    uint32_t now = millis();

    drawSpiralEye(p.leftX, p.eyeYLeft, eyeR, anim.spiralAngle);
    drawSpiralEye(p.rightX, p.eyeYRight, eyeR, -anim.spiralAngle);

    int mhalf = 8;
    int mbot = p.mouthY - 1;
    u8g2_.drawCircle(p.mouthCX, mbot, mhalf, U8G2_DRAW_UPPER_LEFT | U8G2_DRAW_UPPER_RIGHT);
    u8g2_.drawHLine(p.mouthCX - mhalf, mbot, mhalf * 2 + 1);

    int tongueSway = (int)(sin(now / 350.0f) * 2.0f);
    u8g2_.drawDisc(p.mouthCX + 2 + tongueSway, mbot + 2, 2);

    int streakL_X = p.mouthCX - mhalf + 2;
    int streakR_X = p.mouthCX + mhalf - 2;
    int sL = 3 + (int)((now / 90) % 6);
    int sR = 2 + (int)((now / 70) % 5);
    u8g2_.drawVLine(streakL_X, mbot + 1, sL);
    u8g2_.drawVLine(streakR_X, mbot + 1, sR);
    u8g2_.drawPixel(streakL_X, mbot + 1 + sL);

    for (int i = 0; i < 4; i++)
    {
        float a = (now / 600.0f) + (i * (PI / 2.0f));
        int ox = p.mouthCX + (int)(cos(a) * 30);
        int oy = 14 + (int)(sin(a) * 4);
        if (oy > 4 && oy < 14)
        {
            u8g2_.drawPixel(ox, oy);
            if ((now / 200 + i) % 2)
            {
                u8g2_.drawPixel(ox - 1, oy);
                u8g2_.drawPixel(ox + 1, oy);
                u8g2_.drawPixel(ox, oy - 1);
                u8g2_.drawPixel(ox, oy + 1);
            }
        }
    }
}

static void drawDeauthFace(U8G2 &u8g2, const AnimState &anim, const Display::FacePos &p, int eyeR)
{
    u8g2.drawDisc(p.leftX, p.eyeYLeft, eyeR + 1);
    u8g2.drawDisc(p.rightX, p.eyeYRight, eyeR + 1);
    u8g2.setDrawColor(0);
    u8g2.drawDisc(p.leftX - 2, p.eyeYLeft - 2, 2);
    u8g2.drawDisc(p.rightX - 2, p.eyeYRight - 2, 2);
    u8g2.setDrawColor(1);

    u8g2.drawLine(p.leftX - 8, p.eyeYLeft - 11, p.leftX + 6, p.eyeYLeft - 14);
    u8g2.drawLine(p.rightX - 6, p.eyeYRight - 14, p.rightX + 8, p.eyeYRight - 11);

    uint32_t tearTime = millis() / 60;
    int tearY1 = (tearTime) % 16;
    int tearY2 = (tearTime + 8) % 16;
    u8g2.drawVLine(p.leftX - 5, p.eyeYLeft + 6 + tearY1, 4);
    u8g2.drawVLine(p.leftX - 2, p.eyeYLeft + 8 + tearY2, 3);
    u8g2.drawVLine(p.rightX + 5, p.eyeYRight + 6 + tearY1, 4);
    u8g2.drawVLine(p.rightX + 2, p.eyeYRight + 8 + tearY2, 3);

    u8g2.drawHLine(p.mouthCX - 6, p.mouthY, 12);
    u8g2.drawLine(p.mouthCX - 10, p.mouthY + 3, p.mouthCX - 6, p.mouthY);
    u8g2.drawLine(p.mouthCX + 6, p.mouthY, p.mouthCX + 10, p.mouthY + 3);
}

void Display::drawAttackFace(AnimState anim)
{
    clear();
    u8g2_.setDrawColor(1);
    u8g2_.setFont(u8g2_font_5x7_tr);

    FacePos p = computeFacePos(anim);

    switch (anim.currentAttack)
    {
    case ATTACK_EVIL_TWIN:
        drawEvilTwinFace(u8g2_, anim, p, EYE_R);
        drawCentered("EVIL TWIN DETECTED", 60);
        break;
    case ATTACK_CTS_JAMMING:
        drawCtsJammingFace(u8g2_, anim, p, EYE_R);
        drawCentered("CTS NAV JAMMING", 60);
        break;
    case ATTACK_HANDSHAKE_CAPTURE:
        drawHandshakeCaptureFace(u8g2_, anim, p, EYE_R);
        drawCentered("WPA KEY CAPTURE", 60);
        break;
    case ATTACK_BEACON_FLOOD:
        drawBeaconFloodFace(anim, p, EYE_R);
        drawCentered("BEACON FLOOD", 60);
        break;
    case ATTACK_DEAUTH:
    case ATTACK_DISASSOC:
        drawDeauthFace(u8g2_, anim, p, EYE_R);
        drawCentered("DEAUTH", 60);
        break;
    default:
        break;
    }

    u8g2_.drawFrame(0, 0, OLED_WIDTH, OLED_HEIGHT);
    drawAngryAura();
    drawAlertMarks();
    render();
}

void Display::drawEyesSleepy(const FacePos &p)
{
    u8g2_.drawHLine(p.leftX - 7, p.eyeYLeft, 14);
    u8g2_.drawHLine(p.rightX - 7, p.eyeYRight, 14);
}

void Display::drawEyesJazzed(const FacePos &p)
{
    u8g2_.drawHLine(p.leftX - 5, p.eyeYLeft, 10);
    u8g2_.drawHLine(p.rightX - 5, p.eyeYRight, 10);
}

void Display::drawEyesSpiral(const AnimState &anim, const FacePos &p)
{
    if (anim.blink > 0.5f)
    {
        u8g2_.drawHLine(p.leftX - EYE_R, p.eyeYLeft, EYE_R * 2);
        u8g2_.drawHLine(p.rightX - EYE_R, p.eyeYRight, EYE_R * 2);
    }
    else
    {
        drawSpiralEye(p.leftX, p.eyeYLeft, EYE_R, anim.spiralAngle);
        drawSpiralEye(p.rightX, p.eyeYRight, EYE_R, -anim.spiralAngle);
    }
}

void Display::drawEyesHeart(const FacePos &p)
{
    drawHeart(p.leftX, p.eyeYLeft);
    drawHeart(p.rightX, p.eyeYRight);
    
    int cheekYL = p.eyeYLeft + 9;
    int cheekYR = p.eyeYRight + 9;
    u8g2_.drawVLine(p.leftX - 3, cheekYL, 4);
    u8g2_.drawVLine(p.leftX - 1, cheekYL, 6);
    u8g2_.drawVLine(p.leftX + 1, cheekYL, 6);
    u8g2_.drawVLine(p.leftX + 3, cheekYL, 4);
    u8g2_.drawVLine(p.rightX - 3, cheekYR, 4);
    u8g2_.drawVLine(p.rightX - 1, cheekYR, 6);
    u8g2_.drawVLine(p.rightX + 1, cheekYR, 6);
    u8g2_.drawVLine(p.rightX + 3, cheekYR, 4);
}

void Display::drawEyesCurious(const AnimState &anim, const FacePos &p)
{
    int tilt = (int)anim.headTilt;
    int lEY = p.eyeYLeft + tilt;
    int rEY = p.eyeYRight - tilt;
    
    u8g2_.drawDisc(p.leftX, lEY, EYE_R);
    u8g2_.drawDisc(p.rightX, rEY, EYE_R + 3);
    u8g2_.setDrawColor(0);
    u8g2_.drawDisc(p.leftX + 3, lEY - 1, 2);
    u8g2_.drawDisc(p.rightX + 3, rEY - 2, 3);
    u8g2_.setDrawColor(1);
    u8g2_.drawLine(p.rightX - 6, rEY - 14, p.rightX + 6, rEY - 12);
}

void Display::drawEyesVigilant(const AnimState &anim, const FacePos &p)
{
    const int slitWidth = 14;
    const int slitHeight = 3;
    const int pupilSize = 2;
    int leftSlitX = p.leftX - (slitWidth / 2);
    int rightSlitX = p.rightX - (slitWidth / 2);
    
    u8g2_.drawBox(leftSlitX, p.eyeYLeft - (slitHeight / 2), slitWidth, slitHeight);
    u8g2_.drawBox(rightSlitX, p.eyeYRight - (slitHeight / 2), slitWidth, slitHeight);
    u8g2_.setDrawColor(0);
    u8g2_.drawDisc(p.leftX + anim.pupilX, p.eyeYLeft + anim.pupilY, pupilSize);
    u8g2_.drawDisc(p.rightX + anim.pupilX, p.eyeYRight + anim.pupilY, pupilSize);
    u8g2_.setDrawColor(1);
    u8g2_.drawHLine(p.leftX - 8, p.eyeYLeft - 5, 16);
    u8g2_.drawHLine(p.rightX - 8, p.eyeYRight - 5, 16);
}

bool Display::drawEyesTactical(const AnimState &anim, const FacePos &p)
{
    uint32_t tacticalAge = millis() - moodChangeTime;

    if (tacticalAge < 1500)
    {
        u8g2_.drawXBMP(40, 6, 48, 48, meshtastic_logo_48x48);
        u8g2_.setFont(u8g2_font_5x7_tr);
        int tw = u8g2_.getStrWidth("[ ACQUIRING NODE ]");
        u8g2_.drawStr((OLED_WIDTH - tw) / 2, 61, "[ ACQUIRING NODE ]");
        u8g2_.drawRFrame(0, 0, OLED_WIDTH, OLED_HEIGHT, 6);
        render();
        return true;  
    }
    
    int visorW = (p.rightX - p.leftX) + 28;
    int visorX = p.leftX - 14;
    int visorY = p.eyeYLeft - 8;

    u8g2_.drawRFrame(visorX, visorY, visorW, 16, 4);

    if (anim.blink > 0.5f)
    {
        u8g2_.drawHLine(visorX + 4, p.eyeYLeft, visorW - 8);
    }
    else
    {
        uint32_t sweepTime = millis() % 1400;
        int startX = visorX + 4;
        int endX = visorX + visorW - 7;
        int range = endX - startX;
        int laserX = (sweepTime < 700) 
                     ? (startX + (sweepTime * range) / 700) 
                     : (endX - ((sweepTime - 700) * range) / 700);

        u8g2_.drawBox(laserX, visorY + 3, 3, 10);
        if (sweepTime < 700)
            u8g2_.drawVLine(laserX - 1, visorY + 4, 8);
        else
            u8g2_.drawVLine(laserX + 3, visorY + 4, 8);
    }
    
    u8g2_.drawHLine(p.mouthCX - 6, p.mouthY, 12);
    u8g2_.drawPixel(p.mouthCX - 7, p.mouthY - 1);
    u8g2_.drawPixel(p.mouthCX + 6, p.mouthY - 1);
    
    return false;
}

void Display::drawEyesDriving(const FacePos &p)
{
    uint32_t now = millis();
    float cycle = fmod(now / 4000.0f, 1.0f);
    
    if (cycle > 0.4f && cycle < 0.6f)
    {
        float winkPhase = (cycle - 0.4f) / 0.2f;
        float winkAmount = sin(winkPhase * PI);
        
        if (winkAmount > 0.8f)
        {
            u8g2_.drawLine(p.leftX - 5, p.eyeYLeft - 4, p.leftX + 2, p.eyeYLeft);
            u8g2_.drawLine(p.leftX + 2, p.eyeYLeft, p.leftX - 5, p.eyeYLeft + 4);
            u8g2_.drawLine(p.leftX - 3, p.eyeYLeft - 3, p.leftX + 4, p.eyeYLeft);
            u8g2_.drawLine(p.leftX + 4, p.eyeYLeft, p.leftX - 3, p.eyeYLeft + 3);
            u8g2_.drawLine(p.leftX - 1, p.eyeYLeft - 2, p.leftX + 5, p.eyeYLeft);
            u8g2_.drawLine(p.leftX + 5, p.eyeYLeft, p.leftX - 1, p.eyeYLeft + 2);
        }
        else if (winkAmount > 0.4f)
        {
            u8g2_.drawHLine(p.leftX - 6, p.eyeYLeft, 12);
            u8g2_.drawHLine(p.leftX - 5, p.eyeYLeft - 1, 10);
            u8g2_.drawHLine(p.leftX - 5, p.eyeYLeft + 1, 10);
        }
        else
        {
            int squintR = (int)(EYE_R * (1.0f - winkAmount));
            u8g2_.drawDisc(p.leftX, p.eyeYLeft, squintR);
            u8g2_.setDrawColor(0);
            u8g2_.drawDisc(p.leftX + 2, p.eyeYLeft - 1, 1);
            u8g2_.setDrawColor(1);
        }
    }
    else
    {
        u8g2_.drawDisc(p.leftX, p.eyeYLeft, EYE_R);
        u8g2_.setDrawColor(0);
        u8g2_.drawDisc(p.leftX + 2, p.eyeYLeft - 1, 2);
        u8g2_.setDrawColor(1);
    }
    
    u8g2_.drawLine(p.leftX - 8, p.eyeYLeft - 10, p.leftX + 6, p.eyeYLeft - 9);
    u8g2_.drawLine(p.leftX - 8, p.eyeYLeft - 11, p.leftX + 6, p.eyeYLeft - 10);
    
    u8g2_.drawDisc(p.rightX, p.eyeYRight - 2, EYE_R + 1);
    u8g2_.setDrawColor(0);
    u8g2_.drawDisc(p.rightX + 2, p.eyeYRight - 3, 2);
    u8g2_.setDrawColor(1);
    
    float browCycle = fmod(millis() / 3000.0f, 1.0f);
    float browRaise = sin(browCycle * PI * 2) * 3.0f;
    int browY = p.eyeYRight - 14 - (int)browRaise;
    u8g2_.drawLine(p.rightX - 7, browY + 2, p.rightX, browY);
    u8g2_.drawLine(p.rightX, browY, p.rightX + 8, browY + 1);
    u8g2_.drawLine(p.rightX - 7, browY + 1, p.rightX, browY - 1);
    u8g2_.drawLine(p.rightX, browY - 1, p.rightX + 8, browY);
}

#if defined(BOARD_XIAO_C5)
void Display::drawEyesHunting(const AnimState &anim, const FacePos &p)
{
    uint32_t now = millis();

    u8g2_.drawDisc(p.leftX, p.eyeYLeft, EYE_R);
    u8g2_.setDrawColor(0);
    u8g2_.drawDisc(p.leftX + anim.pupilX, p.eyeYLeft + anim.pupilY, 3);
    u8g2_.setDrawColor(1);
    u8g2_.drawPixel(p.leftX + anim.pupilX, p.eyeYLeft + anim.pupilY - 2);
    u8g2_.drawPixel(p.leftX + anim.pupilX, p.eyeYLeft + anim.pupilY + 2);
    u8g2_.drawPixel(p.leftX + anim.pupilX - 2, p.eyeYLeft + anim.pupilY);
    u8g2_.drawPixel(p.leftX + anim.pupilX + 2, p.eyeYLeft + anim.pupilY);

    u8g2_.drawDisc(p.rightX, p.eyeYRight, EYE_R);
    u8g2_.setDrawColor(0);
    u8g2_.drawDisc(p.rightX + anim.pupilX, p.eyeYRight + anim.pupilY, 3);
    u8g2_.setDrawColor(1);
    u8g2_.drawPixel(p.rightX + anim.pupilX, p.eyeYRight + anim.pupilY - 2);
    u8g2_.drawPixel(p.rightX + anim.pupilX, p.eyeYRight + anim.pupilY + 2);
    u8g2_.drawPixel(p.rightX + anim.pupilX - 2, p.eyeYRight + anim.pupilY);
    u8g2_.drawPixel(p.rightX + anim.pupilX + 2, p.eyeYRight + anim.pupilY);

    u8g2_.drawLine(p.leftX - 9, p.eyeYLeft - 11, p.leftX + 6, p.eyeYLeft - 8);
    u8g2_.drawLine(p.leftX - 9, p.eyeYLeft - 12, p.leftX + 6, p.eyeYLeft - 9);
    u8g2_.drawLine(p.rightX + 9, p.eyeYRight - 11, p.rightX - 6, p.eyeYRight - 8);
    u8g2_.drawLine(p.rightX + 9, p.eyeYRight - 12, p.rightX - 6, p.eyeYRight - 9);

    if ((now / 400) % 2)
    {
        u8g2_.drawHLine(2, 8, 4);
        u8g2_.drawVLine(2, 8, 4);
        u8g2_.drawHLine(OLED_WIDTH - 6, 8, 4);
        u8g2_.drawVLine(OLED_WIDTH - 3, 8, 4);
        u8g2_.drawHLine(2, 45, 4);
        u8g2_.drawVLine(2, 42, 4);
        u8g2_.drawHLine(OLED_WIDTH - 6, 45, 4);
        u8g2_.drawVLine(OLED_WIDTH - 3, 42, 4);
    }
}

void Display::drawEyesAlertCamera(const FacePos &p)
{
    uint32_t now = millis();
    int wideR = EYE_R + 2;

    u8g2_.drawDisc(p.leftX, p.eyeYLeft, wideR);
    u8g2_.setDrawColor(0);
    u8g2_.drawDisc(p.leftX, p.eyeYLeft, wideR - 3);
    u8g2_.setDrawColor(1);
    u8g2_.drawDisc(p.leftX, p.eyeYLeft, 2);

    u8g2_.drawDisc(p.rightX, p.eyeYRight, wideR);
    u8g2_.setDrawColor(0);
    u8g2_.drawDisc(p.rightX, p.eyeYRight, wideR - 3);
    u8g2_.setDrawColor(1);
    u8g2_.drawDisc(p.rightX, p.eyeYRight, 2);

    u8g2_.drawLine(p.leftX - 10, p.eyeYLeft - 12, p.leftX + 6, p.eyeYLeft - 9);
    u8g2_.drawLine(p.leftX - 10, p.eyeYLeft - 13, p.leftX + 6, p.eyeYLeft - 10);
    u8g2_.drawLine(p.rightX + 10, p.eyeYRight - 12, p.rightX - 6, p.eyeYRight - 9);
    u8g2_.drawLine(p.rightX + 10, p.eyeYRight - 13, p.rightX - 6, p.eyeYRight - 10);

    if ((now / 150) % 2)
    {
        u8g2_.drawLine(p.leftX - wideR - 3, p.eyeYLeft - wideR - 3, p.leftX - wideR - 3, p.eyeYLeft - wideR + 1);
        u8g2_.drawLine(p.leftX - wideR - 3, p.eyeYLeft - wideR - 3, p.leftX - wideR + 1, p.eyeYLeft - wideR - 3);
        u8g2_.drawLine(p.leftX + wideR + 3, p.eyeYLeft - wideR - 3, p.leftX + wideR + 3, p.eyeYLeft - wideR + 1);
        u8g2_.drawLine(p.leftX + wideR + 3, p.eyeYLeft - wideR - 3, p.leftX + wideR - 1, p.eyeYLeft - wideR - 3);
        u8g2_.drawLine(p.leftX - wideR - 3, p.eyeYLeft + wideR + 3, p.leftX - wideR - 3, p.eyeYLeft + wideR - 1);
        u8g2_.drawLine(p.leftX - wideR - 3, p.eyeYLeft + wideR + 3, p.leftX - wideR + 1, p.eyeYLeft + wideR + 3);
        u8g2_.drawLine(p.leftX + wideR + 3, p.eyeYLeft + wideR + 3, p.leftX + wideR + 3, p.eyeYLeft + wideR - 1);
        u8g2_.drawLine(p.leftX + wideR + 3, p.eyeYLeft + wideR + 3, p.leftX + wideR - 1, p.eyeYLeft + wideR + 3);

        u8g2_.drawLine(p.rightX - wideR - 3, p.eyeYRight - wideR - 3, p.rightX - wideR - 3, p.eyeYRight - wideR + 1);
        u8g2_.drawLine(p.rightX - wideR - 3, p.eyeYRight - wideR - 3, p.rightX - wideR + 1, p.eyeYRight - wideR - 3);
        u8g2_.drawLine(p.rightX + wideR + 3, p.eyeYRight - wideR - 3, p.rightX + wideR + 3, p.eyeYRight - wideR + 1);
        u8g2_.drawLine(p.rightX + wideR + 3, p.eyeYRight - wideR - 3, p.rightX + wideR - 1, p.eyeYRight - wideR - 3);
        u8g2_.drawLine(p.rightX - wideR - 3, p.eyeYRight + wideR + 3, p.rightX - wideR - 3, p.eyeYRight + wideR - 1);
        u8g2_.drawLine(p.rightX - wideR - 3, p.eyeYRight + wideR + 3, p.rightX - wideR + 1, p.eyeYRight + wideR + 3);
        u8g2_.drawLine(p.rightX + wideR + 3, p.eyeYRight + wideR + 3, p.rightX + wideR + 3, p.eyeYRight + wideR - 1);
        u8g2_.drawLine(p.rightX + wideR + 3, p.eyeYRight + wideR + 3, p.rightX + wideR - 1, p.eyeYRight + wideR + 3);
    }
}
#endif

void Display::drawEyesEnraged(const AnimState &anim, const FacePos &p)
{
    int wideR = EYE_R + 1;
    if (p.eyeHeight < 4)
    {
        u8g2_.drawHLine(p.leftX - 7, p.eyeYLeft, 14);
        u8g2_.drawHLine(p.rightX - 7, p.eyeYRight, 14);
    }
    else
    {
        u8g2_.drawDisc(p.leftX, p.eyeYLeft, wideR);
        u8g2_.drawDisc(p.rightX, p.eyeYRight, wideR);
        u8g2_.setDrawColor(0);
        u8g2_.drawDisc(p.leftX + anim.pupilX, p.eyeYLeft + anim.pupilY, 1);
        u8g2_.drawDisc(p.rightX + anim.pupilX, p.eyeYRight + anim.pupilY, 1);
        u8g2_.setDrawColor(1);
    }
    
    u8g2_.drawLine(p.leftX - 8, p.eyeYLeft - 13, p.leftX + 5, p.eyeYLeft - 7);
    u8g2_.drawLine(p.rightX + 8, p.eyeYRight - 13, p.rightX - 5, p.eyeYRight - 7);
    
    u8g2_.drawLine(p.leftX - 14, p.eyeYLeft - 8, p.leftX - 10, p.eyeYLeft - 4);
    u8g2_.drawLine(p.leftX - 10, p.eyeYLeft - 8, p.leftX - 14, p.eyeYLeft - 4);
    u8g2_.drawLine(p.rightX + 14, p.eyeYRight - 8, p.rightX + 10, p.eyeYRight - 4);
    u8g2_.drawLine(p.rightX + 10, p.eyeYRight - 8, p.rightX + 14, p.eyeYRight - 4);
}

void Display::drawEyesDead(const FacePos &p)
{
    int xSize = 6;
    u8g2_.drawLine(p.leftX - xSize, p.eyeYLeft - xSize, p.leftX + xSize, p.eyeYLeft + xSize);
    u8g2_.drawLine(p.leftX + xSize, p.eyeYLeft - xSize, p.leftX - xSize, p.eyeYLeft + xSize);
    u8g2_.drawLine(p.rightX - xSize, p.eyeYRight - xSize, p.rightX + xSize, p.eyeYRight + xSize);
    u8g2_.drawLine(p.rightX + xSize, p.eyeYRight - xSize, p.rightX - xSize, p.eyeYRight + xSize);
    u8g2_.drawHLine(p.mouthCX - 10, p.mouthY, 20);
    u8g2_.drawPixel(p.mouthCX - 11, p.mouthY + 1);
    u8g2_.drawPixel(p.mouthCX + 8, p.mouthY + 1);
    u8g2_.drawPixel(p.mouthCX + 11, p.mouthY + 1);
}

void Display::drawEyesDefault(const AnimState &anim, const FacePos &p, Mood currentMood)
{
    if (p.eyeHeight < 4)
    {
        u8g2_.drawHLine(p.leftX - 6, p.eyeYLeft, 12);
        u8g2_.drawHLine(p.rightX - 6, p.eyeYRight, 12);
    }
    else
    {
        u8g2_.drawDisc(p.leftX, p.eyeYLeft, EYE_R);
        u8g2_.drawDisc(p.rightX, p.eyeYRight, EYE_R);
        if (p.eyeHeight > 8)
        {
            u8g2_.setDrawColor(0);
            u8g2_.drawDisc(p.leftX + 2 + anim.pupilX, p.eyeYLeft - 1 + anim.pupilY, 2);
            u8g2_.drawDisc(p.rightX + 2 + anim.pupilX, p.eyeYRight - 1 + anim.pupilY, 2);
            u8g2_.setDrawColor(1);
        }
    }
    
    if (currentMood == ANNOYED)
    {
        u8g2_.drawLine(p.leftX - 5, p.eyeYLeft - 10, p.leftX + 4, p.eyeYLeft - 6);
        u8g2_.drawLine(p.rightX + 5, p.eyeYRight - 10, p.rightX - 4, p.eyeYRight - 6);
    }
}

void Display::drawMouthForMood(Mood currentMood, const AnimState &anim, const FacePos &p)
{
    if (anim.spiralEyes && anim.dribbling)
    {
        int wave = (int)(sin(millis() / 200.0f) * 3.0f);
        u8g2_.drawLine(p.mouthCX - 12, p.mouthY + wave, p.mouthCX - 6, p.mouthY - wave);
        u8g2_.drawLine(p.mouthCX - 6, p.mouthY - wave, p.mouthCX, p.mouthY + wave);
        u8g2_.drawLine(p.mouthCX, p.mouthY + wave, p.mouthCX + 6, p.mouthY - wave);
        u8g2_.drawLine(p.mouthCX + 6, p.mouthY - wave, p.mouthCX + 12, p.mouthY + wave);
        return;
    }
    if (anim.spiralEyes)
    {
        int wave = (int)(sin(millis() / 200.0f) * 3.0f);
        u8g2_.drawLine(52, p.mouthY + wave, 58, p.mouthY - wave);
        u8g2_.drawLine(58, p.mouthY - wave, 64, p.mouthY + wave);
        u8g2_.drawLine(64, p.mouthY + wave, 70, p.mouthY - wave);
        u8g2_.drawLine(70, p.mouthY - wave, 76, p.mouthY + wave);
        return;
    }

    switch (currentMood)
    {
    case HAPPY:
        u8g2_.drawCircle(p.mouthCX, p.mouthY - 4, 12, U8G2_DRAW_LOWER_LEFT | U8G2_DRAW_LOWER_RIGHT);
        break;
    case JAZZED:
        if (sin(millis() / 150.0f) > 0.3f)
            u8g2_.drawCircle(p.mouthCX, p.mouthY - 2, 5);
        else
            u8g2_.drawCircle(p.mouthCX, p.mouthY - 4, 12, U8G2_DRAW_LOWER_LEFT | U8G2_DRAW_LOWER_RIGHT);
        break;
    case SLEEPY:
        u8g2_.drawHLine(p.mouthCX - 12, p.mouthY, 24);
        break;
    case CURIOUS:
        u8g2_.drawCircle(p.mouthCX, p.mouthY, 4);
        break;
    case ANNOYED:
        u8g2_.drawLine(p.mouthCX - 10, p.mouthY, p.mouthCX - 6, p.mouthY - 2);
        u8g2_.drawLine(p.mouthCX - 6, p.mouthY - 2, p.mouthCX, p.mouthY);
        u8g2_.drawLine(p.mouthCX, p.mouthY, p.mouthCX + 6, p.mouthY - 2);
        u8g2_.drawLine(p.mouthCX + 6, p.mouthY - 2, p.mouthCX + 10, p.mouthY);
        break;
    case VIGILANT:
        u8g2_.drawHLine(p.mouthCX - 8, p.mouthY, 16);
        u8g2_.drawPixel(p.mouthCX - 9, p.mouthY + 1);
        u8g2_.drawPixel(p.mouthCX + 8, p.mouthY + 1);
        u8g2_.drawPixel(p.mouthCX + 9, p.mouthY + 1);
        break;
    case TACTICAL:
        u8g2_.drawHLine(p.mouthCX - 6, p.mouthY, 12);
        u8g2_.drawPixel(p.mouthCX - 7, p.mouthY - 1);
        u8g2_.drawPixel(p.mouthCX + 6, p.mouthY - 1);
        break;
    case DRIVING:
    {
        uint32_t now = millis();
        float smirkCycle = fmod(now / 5000.0f, 1.0f);
        float smirkGrow = sin(smirkCycle * PI);
        int smirkHeight = 3 + (int)(smirkGrow * 3.0f);
        int endX = p.mouthCX + 9;
        int endY = p.mouthY - smirkHeight;
        u8g2_.drawHLine(p.mouthCX - 14, p.mouthY, 10);
        u8g2_.drawLine(p.mouthCX - 4, p.mouthY, p.mouthCX + 2, p.mouthY - 1);
        u8g2_.drawLine(p.mouthCX + 2, p.mouthY - 1, p.mouthCX + 6, p.mouthY - 3);
        u8g2_.drawLine(p.mouthCX + 6, p.mouthY - 3, endX, endY);
        u8g2_.drawLine(endX, endY - 4, endX + 2, endY - 2);
        u8g2_.drawLine(endX + 2, endY - 2, endX + 3, endY);
        u8g2_.drawLine(endX + 3, endY, endX + 2, endY + 2);
        u8g2_.drawLine(endX + 2, endY + 2, endX, endY + 4);
        break;
    }
#if defined(BOARD_XIAO_C5)
    case HUNTING:
        u8g2_.drawHLine(p.mouthCX - 8, p.mouthY, 16);
        u8g2_.drawPixel(p.mouthCX - 9, p.mouthY - 1);
        u8g2_.drawPixel(p.mouthCX + 8, p.mouthY - 1);
        u8g2_.drawPixel(p.mouthCX - 10, p.mouthY);
        u8g2_.drawPixel(p.mouthCX + 10, p.mouthY);
        break;
    case ALERT_CAMERA:
        u8g2_.drawLine(p.mouthCX - 12, p.mouthY - 2, p.mouthCX - 6, p.mouthY);
        u8g2_.drawLine(p.mouthCX - 6, p.mouthY, p.mouthCX, p.mouthY - 3);
        u8g2_.drawLine(p.mouthCX, p.mouthY - 3, p.mouthCX + 6, p.mouthY);
        u8g2_.drawLine(p.mouthCX + 6, p.mouthY, p.mouthCX + 12, p.mouthY - 2);
        u8g2_.drawLine(p.mouthCX - 4, p.mouthY, p.mouthCX - 4, p.mouthY + 3);
        u8g2_.drawLine(p.mouthCX + 4, p.mouthY, p.mouthCX + 4, p.mouthY + 3);
        break;
#endif
    case ENRAGED:
        u8g2_.drawLine(p.mouthCX - 14, p.mouthY, p.mouthCX - 9, p.mouthY - 4);
        u8g2_.drawLine(p.mouthCX - 9, p.mouthY - 4, p.mouthCX - 4, p.mouthY);
        u8g2_.drawLine(p.mouthCX - 4, p.mouthY, p.mouthCX + 1, p.mouthY - 4);
        u8g2_.drawLine(p.mouthCX + 1, p.mouthY - 4, p.mouthCX + 6, p.mouthY);
        u8g2_.drawLine(p.mouthCX + 6, p.mouthY, p.mouthCX + 11, p.mouthY - 4);
        u8g2_.drawLine(p.mouthCX + 11, p.mouthY - 4, p.mouthCX + 14, p.mouthY);
        break;
    case DEAD:
        u8g2_.drawHLine(p.mouthCX - 10, p.mouthY, 20);
        u8g2_.drawPixel(p.mouthCX - 11, p.mouthY + 1);
        u8g2_.drawPixel(p.mouthCX + 8, p.mouthY + 1);
        u8g2_.drawPixel(p.mouthCX + 11, p.mouthY + 1);
        break;
    default:
        u8g2_.drawHLine(p.mouthCX - 10, p.mouthY, 20);
        break;
    }
}

void Display::drawMoodDecorations(Mood currentMood, const AnimState &anim)
{
    if (anim.dribbling) return;
    
    if (currentMood == HAPPY)
        drawHappySparkles();
    if (currentMood == SLEEPY && (millis() / 500) % 2)
        drawSleepZzz(millis() / 300);
    if (currentMood == CURIOUS)
        drawCuriousQuestion();
    if (currentMood == JAZZED)
        drawMusicNotes();
    if (currentMood == VIGILANT || currentMood == TACTICAL)
        drawRadarSweep();
    if (currentMood == ENRAGED)
    {
        drawAngryAura();
        drawAlertMarks();
    }
#if defined(BOARD_XIAO_C5)
    if (currentMood == HUNTING)
        drawHuntingReticle();
    if (currentMood == ALERT_CAMERA)
    {
        drawAlprAlertMarks();
        drawAngryAura();
    }
#endif
}

void Display::drawVigilantFooter()
{
    u8g2_.setFont(u8g2_font_5x7_tr);

    char alertBuf[12];
    if (bleAlertsLoggedTotal > 0)
        snprintf(alertBuf, sizeof(alertBuf), "%lu", (unsigned long)bleAlertsLoggedTotal);
    else
        strcpy(alertBuf, "0");
    u8g2_.drawStr(5, 60, alertBuf);

    if (bleAlertsLoggedTotal > 0)
    {
        int sx = 5 + (strlen(alertBuf) * 5) + 6;
        int sy = 59;
        bool isLit = (millis() / 300) % 2;
        u8g2_.drawHLine(sx - 1, sy, 9);
        
        if (isLit)
        {
            u8g2_.drawBox(sx, sy - 4, 7, 4);
            u8g2_.drawHLine(sx + 1, sy - 5, 5);
            u8g2_.drawPixel(sx + 3, sy - 7);
            u8g2_.drawPixel(sx - 2, sy - 2);
            u8g2_.drawPixel(sx + 8, sy - 2);
        }
        else
        {
            u8g2_.drawVLine(sx, sy - 4, 4);
            u8g2_.drawVLine(sx + 6, sy - 4, 4);
            u8g2_.drawHLine(sx + 1, sy - 5, 5);
            u8g2_.drawPixel(sx + 1, sy - 4);
            u8g2_.drawPixel(sx + 5, sy - 4);
        }
    }

    char devBuf[8];
    snprintf(devBuf, sizeof(devBuf), "%d", bleCount);
    int devW = strlen(devBuf) * 5;
    u8g2_.drawStr(OLED_WIDTH - devW - 14, 60, devBuf);

    int bx = OLED_WIDTH - 10;
    int by = 54;
    u8g2_.drawVLine(bx, by - 1, 9);
    u8g2_.drawLine(bx, by, bx + 5, by + 2);
    u8g2_.drawLine(bx + 5, by + 2, bx, by + 3);
    u8g2_.drawLine(bx, by + 3, bx + 5, by + 4);
    u8g2_.drawLine(bx + 5, by + 4, bx, by + 6);
    u8g2_.drawLine(bx - 2, by, bx, by + 2);
    u8g2_.drawLine(bx - 2, by + 6, bx, by + 4);
}

void Display::drawTacticalFooter()
{
    u8g2_.setFont(u8g2_font_5x7_tr);
    char meshBuf[32];
    snprintf(meshBuf, sizeof(meshBuf), "[ %d NODES LOGGED ]", meshGetLoggedCount());
    int mx = (OLED_WIDTH - u8g2_.getStrWidth(meshBuf)) / 2;
    u8g2_.drawStr(mx, 60, meshBuf);
}

static void drawWifiSignalIcon(U8G2 &u8g2, int wifiX, int wifiY)
{
    u8g2.drawPixel(wifiX + 4, wifiY - 1);
    
    u8g2.drawPixel(wifiX + 3, wifiY - 3);
    u8g2.drawPixel(wifiX + 4, wifiY - 4);
    u8g2.drawPixel(wifiX + 5, wifiY - 3);
    
    u8g2.drawPixel(wifiX + 1, wifiY - 4);
    u8g2.drawPixel(wifiX + 2, wifiY - 5);
    u8g2.drawPixel(wifiX + 3, wifiY - 6);
    u8g2.drawPixel(wifiX + 4, wifiY - 6);
    u8g2.drawPixel(wifiX + 5, wifiY - 6);
    u8g2.drawPixel(wifiX + 6, wifiY - 5);
    u8g2.drawPixel(wifiX + 7, wifiY - 4);
    
    u8g2.drawPixel(wifiX - 1, wifiY - 6);
    u8g2.drawPixel(wifiX,     wifiY - 7);
    u8g2.drawPixel(wifiX + 1, wifiY - 8);
    u8g2.drawPixel(wifiX + 2, wifiY - 8);
    u8g2.drawPixel(wifiX + 3, wifiY - 9);
    u8g2.drawPixel(wifiX + 4, wifiY - 9);
    u8g2.drawPixel(wifiX + 5, wifiY - 9);
    u8g2.drawPixel(wifiX + 6, wifiY - 8);
    u8g2.drawPixel(wifiX + 7, wifiY - 8);
    u8g2.drawPixel(wifiX + 8, wifiY - 7);
    u8g2.drawPixel(wifiX + 9, wifiY - 6);
}

static void drawGpsIcon(U8G2 &u8g2, int gx, int gy, bool valid, int sats)
{
    u8g2.drawBox(gx, gy, 5, 3);
    u8g2.drawBox(gx - 4, gy, 3, 3);
    u8g2.drawBox(gx + 6, gy, 3, 3);
    u8g2.drawVLine(gx + 2, gy - 2, 2);
    u8g2.drawPixel(gx + 1, gy - 3);
    u8g2.drawPixel(gx + 3, gy - 3);

    if (valid)
    {
        char satBuf[8];
        snprintf(satBuf, sizeof(satBuf), "%d", sats);
        u8g2.drawStr(gx + 12, 60, satBuf);
        u8g2.drawCircle(gx + 2, gy - 3, 3, U8G2_DRAW_UPPER_LEFT | U8G2_DRAW_UPPER_RIGHT);
    }
    else if ((millis() / 500) % 2)  
    {
        u8g2.drawStr(gx + 12, 60, "0");
        u8g2.drawCircle(gx + 2, gy - 3, 3, U8G2_DRAW_UPPER_LEFT | U8G2_DRAW_UPPER_RIGHT);
    }
}

void Display::drawDrivingFooter()
{
    u8g2_.setFont(u8g2_font_5x7_tr);
    
    u8g2_.drawStr(3, 20, "W");
    u8g2_.drawStr(3, 30, "A");
    u8g2_.drawStr(3, 40, "R");
    u8g2_.drawStr(120, 20, "D");
    u8g2_.drawStr(120, 30, "R");
    u8g2_.drawStr(120, 40, "V");

    char netBuf[16];
    snprintf(netBuf, sizeof(netBuf), "%lu", (unsigned long)wardrivingNetworksLogged);
    u8g2_.drawStr(8, 60, netBuf);
    int wifiX = 10 + (strlen(netBuf) * 5) + 2;
    drawWifiSignalIcon(u8g2_, wifiX, 61);

    drawGpsIcon(u8g2_, 100, 57, gpsData.valid, gpsData.satellites);
}

#if defined(BOARD_XIAO_C5)
void Display::drawAlprHunterFooter(Mood currentMood)
{
    u8g2_.setFont(u8g2_font_5x7_tr);

    u8g2_.drawStr(3, 20, "F");
    u8g2_.drawStr(3, 30, "L");
    u8g2_.drawStr(3, 40, "K");
    u8g2_.drawStr(120, 20, "H");
    u8g2_.drawStr(120, 30, "N");
    u8g2_.drawStr(120, 40, "T");

    char chBuf[8];
    snprintf(chBuf, sizeof(chBuf), "CH%d", alprCurrentChannel);
    u8g2_.drawStr(7, 60, chBuf);

    char countBuf[20];
    snprintf(countBuf, sizeof(countBuf), "D:%lu L:%lu",
             (unsigned long)alprGetDefiniteCount(),
             (unsigned long)alprGetLikelyCount());
    u8g2_.drawStr(76, 60, countBuf);

    drawGpsIcon(u8g2_, 34, 57, gpsData.valid, gpsData.satellites);

    int camX = OLED_WIDTH - 12;
    int camY = 55;
    u8g2_.drawFrame(camX, camY, 8, 6);
    u8g2_.drawBox(camX + 2, camY - 1, 3, 1);
    u8g2_.drawDisc(camX + 4, camY + 3, 1);

    if (currentMood == ALERT_CAMERA && (millis() / 200) % 2)
    {
        u8g2_.drawPixel(camX + 4, camY - 3);
        u8g2_.drawPixel(camX + 3, camY - 2);
        u8g2_.drawPixel(camX + 5, camY - 2);
    }
}
#endif

void Display::drawFace(Mood currentMood, AnimState anim)
{
    if (!ready_) return;

    if (anim.showAttackFace)
    {
        drawAttackFace(anim);
        return;
    }

    clear();

    FacePos p = computeFacePos(anim);

    if (currentMood == SLEEPY)
        drawEyesSleepy(p);
    else if (currentMood == JAZZED)
        drawEyesJazzed(p);
    else if (anim.spiralEyes)
        drawEyesSpiral(anim, p);
    else if (anim.heartEyes)
        drawEyesHeart(p);
    else if (currentMood == CURIOUS)
        drawEyesCurious(anim, p);
    else if (currentMood == VIGILANT)
        drawEyesVigilant(anim, p);
    else if (currentMood == TACTICAL)
    {
        if (drawEyesTactical(anim, p)) return;  
    }
    else if (currentMood == DRIVING)
        drawEyesDriving(p);
#if defined(BOARD_XIAO_C5)
    else if (currentMood == HUNTING)
        drawEyesHunting(anim, p);
    else if (currentMood == ALERT_CAMERA)
        drawEyesAlertCamera(p);
#endif
    else if (currentMood == ENRAGED)
        drawEyesEnraged(anim, p);
    else if (currentMood == DEAD)
    {
        drawEyesDead(p);
        drawMoodDecorations(currentMood, anim);
        u8g2_.drawRFrame(0, 0, OLED_WIDTH, OLED_HEIGHT, 6);
        render();
        return;
    }
    else
        drawEyesDefault(anim, p, currentMood);

    drawMouthForMood(currentMood, anim, p);

    drawMoodDecorations(currentMood, anim);

    bool useAngryFrame = (currentMood == ENRAGED);
#if defined(BOARD_XIAO_C5)
    useAngryFrame = useAngryFrame || (currentMood == ALERT_CAMERA);
#endif
    if (useAngryFrame)
        u8g2_.drawFrame(0, 0, OLED_WIDTH, OLED_HEIGHT);
    else
        u8g2_.drawRFrame(0, 0, OLED_WIDTH, OLED_HEIGHT, 6);

    if (currentMood == VIGILANT)
        drawVigilantFooter();
    else if (currentMood == TACTICAL)
        drawTacticalFooter();
    else if (currentMood == DRIVING)
        drawDrivingFooter();
#if defined(BOARD_XIAO_C5)
    else if (currentMood == HUNTING || currentMood == ALERT_CAMERA)
        drawAlprHunterFooter(currentMood);
#endif

    render();
}

void Display::drawSleepZzz(int frame)
{
    u8g2_.setFont(u8g2_font_5x7_tr);
    int x = 100 + (frame % 3) * 2;
    int y = 18 - (frame % 4) * 3;
    u8g2_.drawStr(x, y, "z");
    u8g2_.drawStr(x + 4, y - 3, "z");
    u8g2_.drawStr(x + 8, y - 6, "Z");
}

void Display::drawMusicNotes()
{
    int offset = (millis() / 500) % 3;
    for (int i = 0; i < 3; i++)
    {
        int x = 100 + (i * 14) - (offset * 4);
        int y = 20 - (i * 5);
        if (x < 100) x += 42;
        u8g2_.drawDisc(x, y + 2, 2);
        u8g2_.drawVLine(x + 3, y - 5, 7);
        u8g2_.drawDisc(x + 7, y + 2, 2);
        u8g2_.drawVLine(x + 10, y - 5, 7);
        u8g2_.drawHLine(x + 3, y - 5, 7);
    }
}

void Display::drawCuriousQuestion()
{
    int bobY = 12 + (int)(sin(millis() / 300.0) * 3);
    u8g2_.setFont(u8g2_font_6x12_tr);
    u8g2_.drawStr(106, bobY, "?");
}

void Display::drawHeart(int x, int y)
{
    u8g2_.drawDisc(x - 3, y - 2, 3);
    u8g2_.drawDisc(x + 3, y - 2, 3);
    u8g2_.drawBox(x - 2, y, 5, 3);
    u8g2_.drawHLine(x - 3, y + 2, 7);
    u8g2_.drawHLine(x - 2, y + 3, 5);
    u8g2_.drawHLine(x - 1, y + 4, 3);
    u8g2_.drawHLine(x, y + 5, 1);
}

void Display::drawHappySparkles()
{
    u8g2_.drawPixel(20, 15);
    u8g2_.drawPixel(19, 16);
    u8g2_.drawPixel(21, 16);
    u8g2_.drawPixel(20, 17);
    u8g2_.drawPixel(108, 16);
    u8g2_.drawPixel(107, 17);
    u8g2_.drawPixel(109, 17);
    u8g2_.drawPixel(108, 18);
}

void Display::drawRadarSweep()
{
    int cx = 114, cy = 12, r = 8;
    u8g2_.drawCircle(cx, cy, r);
    float angle = fmod(millis() / 600.0f, 2.0f) * PI;
    int ex = cx + (int)(sin(angle) * r);
    int ey = cy - (int)(cos(angle) * r);
    u8g2_.drawLine(cx, cy, ex, ey);
    
    for (int i = 1; i <= 3; i++)
    {
        float trailAngle = angle - (i * 0.3f);
        int tx = cx + (int)(sin(trailAngle) * (r - i));
        int ty = cy - (int)(cos(trailAngle) * (r - i));
        u8g2_.drawPixel(tx, ty);
    }
    u8g2_.drawDisc(cx, cy, 1);
}

void Display::drawAngryAura()
{
    if ((millis() / 120) % 3 == 0) return;
    u8g2_.drawLine(25, 2, 28, 0);
    u8g2_.drawLine(50, 2, 47, 0);
    u8g2_.drawLine(78, 2, 81, 0);
    u8g2_.drawLine(103, 2, 100, 0);
    u8g2_.drawLine(25, 61, 28, 63);
    u8g2_.drawLine(50, 61, 47, 63);
    u8g2_.drawLine(78, 61, 81, 63);
    u8g2_.drawLine(103, 61, 100, 63);
    u8g2_.drawLine(2, 20, 0, 17);
    u8g2_.drawLine(2, 44, 0, 47);
    u8g2_.drawLine(125, 20, 127, 17);
    u8g2_.drawLine(125, 44, 127, 47);
}

void Display::drawAlertMarks()
{
    u8g2_.setFont(u8g2_font_6x12_tr);
    if ((millis() / 200) % 2)
    {
        u8g2_.drawStr(6, 14, "!");
        u8g2_.drawStr(116, 58, "!");
    }
    else
    {
        u8g2_.drawStr(116, 14, "!");
        u8g2_.drawStr(6, 58, "!");
    }
}

void Display::drawRssiBars(int x, int y, int bars)
{
    for (int i = 0; i < 5; i++)
    {
        int barHeight = 3 + (i * 2);
        int barX = x + (i * 8);
        int barTop = y + (11 - barHeight);
        
        if (i < bars)
            u8g2_.drawBox(barX, barTop, 6, barHeight);
        else
            u8g2_.drawFrame(barX, barTop, 6, barHeight);
    }
}

#if defined(BOARD_XIAO_C5)
void Display::drawHuntingReticle()
{
    int cx = 110, cy = 12, r = 6;
    uint32_t now = millis();

    u8g2_.drawCircle(cx, cy, r);

    float angle = fmod(now / 400.0f, 2.0f) * PI;
    int ex = cx + (int)(sin(angle) * r);
    int ey = cy - (int)(cos(angle) * r);
    u8g2_.drawLine(cx, cy, ex, ey);

    u8g2_.drawPixel(cx, cy);
    u8g2_.drawPixel(cx - 2, cy);
    u8g2_.drawPixel(cx + 2, cy);
    u8g2_.drawPixel(cx, cy - 2);
    u8g2_.drawPixel(cx, cy + 2);

    u8g2_.drawPixel(cx, cy - r - 1);
    u8g2_.drawPixel(cx, cy + r + 1);
    u8g2_.drawPixel(cx - r - 1, cy);
    u8g2_.drawPixel(cx + r + 1, cy);
}

void Display::drawAlprAlertMarks()
{
    u8g2_.setFont(u8g2_font_6x12_tr);
    uint32_t now = millis();

    if ((now / 150) % 2)
    {
        u8g2_.drawStr(6, 14, "!");
        u8g2_.drawStr(116, 58, "!");
    }
    else
    {
        u8g2_.drawStr(116, 14, "!");
        u8g2_.drawStr(6, 58, "!");
    }

    u8g2_.setFont(u8g2_font_5x7_tr);
    if ((now / 300) % 2)
    {
        const char *alertText = "TARGET";
        int tw = u8g2_.getStrWidth(alertText);
        u8g2_.drawStr((OLED_WIDTH - tw) / 2, 10, alertText);
    }
}
#endif

void Display::drawSpeedometer(double speed, const char *unit, bool hasFix, int sats)
{
    clear();
    u8g2_.setDrawColor(1);
    u8g2_.drawRBox(4, 2, OLED_WIDTH - 8, 48, 4);
    u8g2_.setDrawColor(0);
    
    char speedBuf[8];
    if (speed < 1.0)
        snprintf(speedBuf, sizeof(speedBuf), "0");
    else
        snprintf(speedBuf, sizeof(speedBuf), "%.0f", speed);
    
    u8g2_.setFont(u8g2_font_logisoso28_tn);
    int numW = u8g2_.getStrWidth(speedBuf);
    u8g2_.drawStr((OLED_WIDTH - numW) / 2, 36, speedBuf);
    
    u8g2_.setFont(u8g2_font_7x14B_tr);
    int unitW = u8g2_.getStrWidth(unit);
    u8g2_.drawStr((OLED_WIDTH - unitW) / 2, 48, unit);
    
    u8g2_.setDrawColor(1);
    u8g2_.setFont(u8g2_font_5x7_tr);
    
    if (hasFix)
    {
        char satBuf[16];
        snprintf(satBuf, sizeof(satBuf), "FIX %dS", sats);
        u8g2_.drawStr(4, 62, satBuf);
    }
    else
    {
        u8g2_.drawStr(4, 62, "NO FIX");
    }
    u8g2_.drawStr(78, 62, "Tap:Unit");
    u8g2_.drawRFrame(0, 0, OLED_WIDTH, OLED_HEIGHT, 6);
    render();
}

void Display::drawClock(const char *time, const char *date, const char *timezone)
{
    clear();
    u8g2_.setDrawColor(1);
    u8g2_.drawRBox(4, 2, OLED_WIDTH - 8, 38, 4);
    u8g2_.setDrawColor(0);
    
    u8g2_.setFont(u8g2_font_logisoso20_tr);
    int timeW = u8g2_.getStrWidth(time);
    u8g2_.drawStr((OLED_WIDTH - timeW) / 2, 32, time);
    
    u8g2_.setDrawColor(1);
    u8g2_.setFont(u8g2_font_7x14B_tr);
    int dateW = u8g2_.getStrWidth(date);
    u8g2_.drawStr((OLED_WIDTH - dateW) / 2, 52, date);
    
    u8g2_.setFont(u8g2_font_5x7_tr);
    u8g2_.drawStr(4, 62, timezone);
    u8g2_.drawStr(82, 62, "Hold:Back");
    u8g2_.drawRFrame(0, 0, OLED_WIDTH, OLED_HEIGHT, 6);
    render();
}

void Display::drawDiceScreen(bool tiltMode)
{
    if (!ready_) return;
    clear();
    diceRoller_.updateAndDraw(tiltMode);
    render();
}

void Display::drawMagic8BallScreen(bool tiltMode)
{
    if (!ready_) return;
    clear();
    magic8Ball_.updateAndDraw(tiltMode);
    render();
}

void Display::drawBlackjackScreen(bool tiltMode)
{
    if (!ready_) return;
    clear();
    blackjack_.updateAndDraw(tiltMode);
    render();
}

void Display::drawMenu(const char *title, const char **items, uint8_t itemCount, int selectedIdx, int arrowIdx)
{
    clear();
    u8g2_.setFont(u8g2_font_6x10_tr);
    u8g2_.setDrawColor(1);
    
    const int lineHeight = 12;
    const int menuTop = 15;
    int scrollOffset = (selectedIdx >= 4) ? selectedIdx - 3 : 0;

    drawCentered(title, 11);
    u8g2_.drawHLine(2, 13, OLED_WIDTH - 4);

    u8g2_.setClipWindow(3, 14, OLED_WIDTH - 3, OLED_HEIGHT - 2);

    for (int i = 0; i < 4; i++)
    {
        int idx = i + scrollOffset;
        if (idx >= itemCount) break;
        
        int boxY = menuTop + i * lineHeight;
        
        if (idx == selectedIdx)
        {
            u8g2_.drawBox(2, boxY, OLED_WIDTH - 4, lineHeight);
            u8g2_.setDrawColor(0);
        }
        if (idx == arrowIdx)
            u8g2_.drawStr(4, boxY + lineHeight - 2, "->");

        int strWidth = u8g2_.getStrWidth(items[idx]);
        int maxWidth = OLED_WIDTH - 8;
        int textY = boxY + lineHeight - 2;

        if (strWidth <= maxWidth)
        {
            drawCentered(items[idx], textY);
        }
        else
        {
            int overflow = strWidth - maxWidth;
            int pause = 25;
            int cycle = (overflow * 2) + (pause * 2);
            int step = (millis() / 35) % cycle;

            int offset = 0;
            if (step < pause)                            offset = 0;
            else if (step < pause + overflow)            offset = step - pause;
            else if (step < (pause * 2) + overflow)      offset = overflow;
            else                                          offset = overflow - (step - ((pause * 2) + overflow));

            u8g2_.drawStr(4 - offset, textY, items[idx]);
        }

        u8g2_.setDrawColor(1);
    }

    u8g2_.setMaxClipWindow();
    u8g2_.drawRFrame(0, 0, OLED_WIDTH, OLED_HEIGHT, 6);
    render();
}

void Display::drawConfirm(const char *line1, const char *line2)
{
    clear();
    u8g2_.setFont(u8g2_font_6x10_tr);
    u8g2_.drawStr(54, 28, "OK");
    if (line1) drawCentered(line1, 42);
    if (line2) drawCentered(line2, 54);
    u8g2_.drawRFrame(0, 0, OLED_WIDTH, OLED_HEIGHT, 6);
    render();
}

void Display::drawStatusBar(bool touched, int volume)
{
    u8g2_.drawHLine(0, 63, OLED_WIDTH);
    
    if (touched)
    {
        u8g2_.drawBox(2, 59, 8, 3);
        u8g2_.setFont(u8g2_font_4x6_tr);
        u8g2_.drawStr(12, 63, "TOUCH");
    }
    
    for (int i = 0; i < 3; i++)
    {
        int h = (volume > i * 85) ? 3 : 1;
        u8g2_.drawBox(118 - i * 4, 63 - h, 2, h);
    }
}