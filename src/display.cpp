#include "display.h"
#include <Wire.h>

bool Display::begin() {
    Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL, I2C_FREQ);
    delay(10);

    u8g2_.setI2CAddress(OLED_ADDR << 1);
    if (!u8g2_.begin()) return false;

    u8g2_.setBusClock(I2C_FREQ);
    u8g2_.clearDisplay();
    u8g2_.setFontMode(1);
    u8g2_.setDrawColor(1);
    ready_ = true;
    return true;
}

void Display::clear()  { if (ready_) u8g2_.clearBuffer(); }
void Display::render() { if (ready_) u8g2_.sendBuffer(); }

void Display::drawCentered(const char* text, int y) {
    int w = u8g2_.getStrWidth(text);
    u8g2_.drawStr((OLED_WIDTH - w) / 2, y, text);
}

void Display::drawSplash() {
    clear();
    u8g2_.setFont(u8g2_font_logisoso16_tr);
    drawCentered("CLUNCHI", 26);
    u8g2_.setFont(u8g2_font_5x7_tr);
    drawCentered("BETA 1.0", 44);
    u8g2_.drawRFrame(0, 0, OLED_WIDTH, OLED_HEIGHT, 8);
    render();
}

void Display::drawSpiralEye(int cx, int cy, int radius, float angle) {
    const int   arms       = 2;    
    const float armSpacing = 3.5f;   
    const float thetaStep  = 0.15f;  
    const float maxTheta   = radius * (2.0f * PI) / armSpacing; 

    for (int arm = 0; arm < arms; arm++) {
        float armOffset = arm * (2.0f * PI / arms);

        for (float theta = 0.5f; theta < maxTheta; theta += thetaStep) {
            float r = (armSpacing / (2.0f * PI)) * theta;

            if (r > radius) break; 

            float drawAngle = theta + angle + armOffset;

            int px = cx + (int)(cos(drawAngle) * r);
            int py = cy + (int)(sin(drawAngle) * r);

            if (px >= 0 && px < OLED_WIDTH && py >= 0 && py < OLED_HEIGHT) {
                u8g2_.drawPixel(px, py);
            }
        }
    }

    u8g2_.drawDisc(cx, cy, 1);

    u8g2_.drawCircle(cx, cy, radius);
}

void Display::drawFace(Mood currentMood, AnimState anim) {
    clear();

    const int baseEyeY = 26;
    const int baseLeftX = 38, baseRightX = 90;
    const int eyeR = 7;

    int offsetY = (int)anim.bounceY + (int)anim.breathY + (int)anim.squishY;

    int leftX  = baseLeftX  + (int)anim.leftEyeOffX;
    int rightX = baseRightX + (int)anim.rightEyeOffX;
    int leftEyeYOff  = (int)anim.leftEyeOffY;
    int rightEyeYOff = (int)anim.rightEyeOffY;
    int mouthXOff    = (int)anim.mouthOffX;
    int mouthYOff    = (int)anim.mouthOffY;

    int eyeYLeft  = baseEyeY + offsetY + leftEyeYOff;
    int eyeYRight = baseEyeY + offsetY + rightEyeYOff;
    int mouthY    = 46 + offsetY + mouthYOff;
    int mouthCX   = 64 + mouthXOff; 

    int eyeHeight = eyeR * 2;
    if (anim.blink > 0) {
        eyeHeight = std::max(2, (int)(eyeHeight * (1.0f - anim.blink)));
    }

    if (currentMood == SLEEPY) {
        u8g2_.drawHLine(leftX - 7, eyeYLeft, 14);
        u8g2_.drawHLine(rightX - 7, eyeYRight, 14);
    }
    else if (currentMood == JAZZED) {
        u8g2_.drawHLine(leftX - 5, eyeYLeft, 10);
        u8g2_.drawHLine(rightX - 5, eyeYRight, 10);
    }
    else if (anim.spiralEyes) {
        if (anim.blink > 0.5f) {
            u8g2_.drawHLine(leftX - eyeR, eyeYLeft, eyeR * 2);
            u8g2_.drawHLine(rightX - eyeR, eyeYRight, eyeR * 2);
        } else {
            drawSpiralEye(leftX, eyeYLeft, eyeR, anim.spiralAngle);
            drawSpiralEye(rightX, eyeYRight, eyeR, -anim.spiralAngle);
        }
    }
    else if (anim.heartEyes) {
        drawHeart(leftX, eyeYLeft);
        drawHeart(rightX, eyeYRight);

        int cheekYL = eyeYLeft + 9;
        int cheekYR = eyeYRight + 9;
        u8g2_.drawVLine(leftX - 3, cheekYL, 4);
        u8g2_.drawVLine(leftX - 1, cheekYL, 6);
        u8g2_.drawVLine(leftX + 1, cheekYL, 6);
        u8g2_.drawVLine(leftX + 3, cheekYL, 4);
        u8g2_.drawVLine(rightX - 3, cheekYR, 4);
        u8g2_.drawVLine(rightX - 1, cheekYR, 6);
        u8g2_.drawVLine(rightX + 1, cheekYR, 6);
        u8g2_.drawVLine(rightX + 3, cheekYR, 4);
    }
    else if (currentMood == CURIOUS) {
        int tilt = (int)anim.headTilt;
        int lEY = eyeYLeft + tilt;
        int rEY = eyeYRight - tilt;
        u8g2_.drawDisc(leftX, lEY, eyeR);
        u8g2_.drawDisc(rightX, rEY, eyeR + 3);
        u8g2_.setDrawColor(0);
        u8g2_.drawDisc(leftX + 3, lEY - 1, 2);
        u8g2_.drawDisc(rightX + 3, rEY - 2, 3);
        u8g2_.setDrawColor(1);
        u8g2_.drawLine(rightX - 6, rEY - 14, rightX + 6, rEY - 12);
    }
    else if (currentMood == VIGILANT) {
        const int slitWidth  = 14;
        const int slitHeight = 3;
        const int pupilSize  = 2;

        int leftSlitX  = leftX - (slitWidth / 2);
        int rightSlitX = rightX - (slitWidth / 2);
        u8g2_.drawBox(leftSlitX, eyeYLeft - (slitHeight / 2), slitWidth, slitHeight);
        u8g2_.drawBox(rightSlitX, eyeYRight - (slitHeight / 2), slitWidth, slitHeight);

        u8g2_.setDrawColor(0);
        u8g2_.drawDisc(leftX + anim.pupilX, eyeYLeft + anim.pupilY, pupilSize);
        u8g2_.drawDisc(rightX + anim.pupilX, eyeYRight + anim.pupilY, pupilSize);
        u8g2_.setDrawColor(1);

        u8g2_.drawHLine(leftX - 8, eyeYLeft - 5, 16);
        u8g2_.drawHLine(rightX - 8, eyeYRight - 5, 16);
    }
    else if (currentMood == ENRAGED) {
        int wideR = eyeR + 1;
        if (eyeHeight < 4) {
            u8g2_.drawHLine(leftX - 7, eyeYLeft, 14);
            u8g2_.drawHLine(rightX - 7, eyeYRight, 14);
        } else {
            u8g2_.drawDisc(leftX, eyeYLeft, wideR);
            u8g2_.drawDisc(rightX, eyeYRight, wideR);
            u8g2_.setDrawColor(0);
            u8g2_.drawDisc(leftX + anim.pupilX, eyeYLeft + anim.pupilY, 1);
            u8g2_.drawDisc(rightX + anim.pupilX, eyeYRight + anim.pupilY, 1);
            u8g2_.setDrawColor(1);
        }

        u8g2_.drawLine(leftX - 8, eyeYLeft - 13, leftX + 5, eyeYLeft - 7);
        u8g2_.drawLine(rightX + 8, eyeYRight - 13, rightX - 5, eyeYRight - 7);

        u8g2_.drawLine(leftX - 14, eyeYLeft - 8, leftX - 10, eyeYLeft - 4);
        u8g2_.drawLine(leftX - 10, eyeYLeft - 8, leftX - 14, eyeYLeft - 4);
        u8g2_.drawLine(rightX + 14, eyeYRight - 8, rightX + 10, eyeYRight - 4);
        u8g2_.drawLine(rightX + 10, eyeYRight - 8, rightX + 14, eyeYRight - 4);
    }
    else if (currentMood == DEAD) {
        int xSize = 6;
        u8g2_.drawLine(leftX - xSize, eyeYLeft - xSize, leftX + xSize, eyeYLeft + xSize);
        u8g2_.drawLine(leftX + xSize, eyeYLeft - xSize, leftX - xSize, eyeYLeft + xSize);
        u8g2_.drawLine(rightX - xSize, eyeYRight - xSize, rightX + xSize, eyeYRight + xSize);
        u8g2_.drawLine(rightX + xSize, eyeYRight - xSize, rightX - xSize, eyeYRight + xSize);
    }
    else {
        if (eyeHeight < 4) {
            u8g2_.drawHLine(leftX - 6, eyeYLeft, 12);
            u8g2_.drawHLine(rightX - 6, eyeYRight, 12);
        } else {
            u8g2_.drawDisc(leftX, eyeYLeft, eyeR);
            u8g2_.drawDisc(rightX, eyeYRight, eyeR);
            if (eyeHeight > 8) {
                u8g2_.setDrawColor(0);
                u8g2_.drawDisc(leftX + 2 + anim.pupilX, eyeYLeft - 1 + anim.pupilY, 2);
                u8g2_.drawDisc(rightX + 2 + anim.pupilX, eyeYRight - 1 + anim.pupilY, 2);
                u8g2_.setDrawColor(1);
            }
        }
        if (currentMood == ANNOYED) {
            u8g2_.drawLine(leftX - 5, eyeYLeft - 10, leftX + 4, eyeYLeft - 6);
            u8g2_.drawLine(rightX + 5, eyeYRight - 10, rightX - 4, eyeYRight - 6);
        }
    }

    if (anim.spiralEyes && anim.dribbling) {
        int wave = (int)(sin(millis() / 200.0f) * 3.0f);
        u8g2_.drawLine(mouthCX - 12, mouthY + wave, mouthCX - 6, mouthY - wave);
        u8g2_.drawLine(mouthCX - 6, mouthY - wave, mouthCX, mouthY + wave);
        u8g2_.drawLine(mouthCX, mouthY + wave, mouthCX + 6, mouthY - wave);
        u8g2_.drawLine(mouthCX + 6, mouthY - wave, mouthCX + 12, mouthY + wave);
    }
    else if (anim.spiralEyes) {
        int wave = (int)(sin(millis() / 200.0f) * 3.0f);
        u8g2_.drawLine(52, mouthY + wave, 58, mouthY - wave);
        u8g2_.drawLine(58, mouthY - wave, 64, mouthY + wave);
        u8g2_.drawLine(64, mouthY + wave, 70, mouthY - wave);
        u8g2_.drawLine(70, mouthY - wave, 76, mouthY + wave);
    }
    else if (currentMood == HAPPY) {
        u8g2_.drawCircle(mouthCX, mouthY - 4, 12, U8G2_DRAW_LOWER_LEFT | U8G2_DRAW_LOWER_RIGHT);
    }
    else if (currentMood == JAZZED) {
        if (sin(millis() / 150.0f) > 0.3f)
            u8g2_.drawCircle(mouthCX, mouthY - 2, 5);
        else
            u8g2_.drawCircle(mouthCX, mouthY - 4, 12, U8G2_DRAW_LOWER_LEFT | U8G2_DRAW_LOWER_RIGHT);
    }
    else if (currentMood == SLEEPY) {
        u8g2_.drawHLine(mouthCX - 12, mouthY, 24);
    }
    else if (currentMood == CURIOUS) {
        u8g2_.drawCircle(mouthCX, mouthY, 4);
    }
    else if (currentMood == ANNOYED) {
        u8g2_.drawLine(mouthCX - 10, mouthY, mouthCX - 6, mouthY - 2);
        u8g2_.drawLine(mouthCX - 6, mouthY - 2, mouthCX, mouthY);
        u8g2_.drawLine(mouthCX, mouthY, mouthCX + 6, mouthY - 2);
        u8g2_.drawLine(mouthCX + 6, mouthY - 2, mouthCX + 10, mouthY);
    }
    else if (currentMood == VIGILANT) {
        u8g2_.drawHLine(mouthCX - 8, mouthY, 16);
        u8g2_.drawPixel(mouthCX - 9, mouthY + 1);
        u8g2_.drawPixel(mouthCX + 8, mouthY + 1);
        u8g2_.drawPixel(mouthCX + 9, mouthY + 1);
    }
    else if (currentMood == ENRAGED) {
        u8g2_.drawLine(mouthCX - 14, mouthY, mouthCX - 9, mouthY - 4);
        u8g2_.drawLine(mouthCX - 9, mouthY - 4, mouthCX - 4, mouthY);
        u8g2_.drawLine(mouthCX - 4, mouthY, mouthCX + 1, mouthY - 4);
        u8g2_.drawLine(mouthCX + 1, mouthY - 4, mouthCX + 6, mouthY);
        u8g2_.drawLine(mouthCX + 6, mouthY, mouthCX + 11, mouthY - 4);
        u8g2_.drawLine(mouthCX + 11, mouthY - 4, mouthCX + 14, mouthY);
    }
    else if (currentMood == DEAD) {
        u8g2_.drawHLine(mouthCX - 10, mouthY, 20);
        u8g2_.drawPixel(mouthCX - 11, mouthY + 1);
        u8g2_.drawPixel(mouthCX + 8, mouthY + 1);
        u8g2_.drawPixel(mouthCX + 11, mouthY + 1);
    }
    else {
        u8g2_.drawHLine(mouthCX - 10, mouthY, 20);
    }

    if (!anim.dribbling) {
        if (currentMood == HAPPY)   drawHappySparkles();
        if (currentMood == SLEEPY && (millis() / 500) % 2) drawSleepZzz(millis() / 300);
        if (currentMood == CURIOUS) drawCuriousQuestion();
        if (currentMood == JAZZED)  drawMusicNotes();
        if (currentMood == VIGILANT) drawRadarSweep();
        if (currentMood == ENRAGED) {
            drawAngryAura();
            drawAlertMarks();
        }
    }

    if (currentMood == ENRAGED) {
        u8g2_.drawFrame(0, 0, OLED_WIDTH, OLED_HEIGHT);
    } else {
        u8g2_.drawRFrame(0, 0, OLED_WIDTH, OLED_HEIGHT, 6);
    }

    render();
}

void Display::drawSleepZzz(int frame) {
    u8g2_.setFont(u8g2_font_5x7_tr);
    int x = 100 + (frame % 3) * 2;
    int y = 18 - (frame % 4) * 3;
    u8g2_.drawStr(x, y, "z");
    u8g2_.drawStr(x + 4, y - 3, "z");
    u8g2_.drawStr(x + 8, y - 6, "Z");
}

void Display::drawMusicNotes() {
    int offset = (millis() / 500) % 3;
    for (int i = 0; i < 3; i++) {
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

void Display::drawCuriousQuestion() {
    int bobY = 12 + (int)(sin(millis() / 300.0) * 3);
    u8g2_.setFont(u8g2_font_6x12_tr);
    u8g2_.drawStr(106, bobY, "?");
}

void Display::drawHeart(int x, int y) {
    u8g2_.drawDisc(x - 3, y - 2, 3);
    u8g2_.drawDisc(x + 3, y - 2, 3);
    u8g2_.drawBox(x - 2, y, 5, 3);
    u8g2_.drawHLine(x - 3, y + 2, 7);
    u8g2_.drawHLine(x - 2, y + 3, 5);
    u8g2_.drawHLine(x - 1, y + 4, 3);
    u8g2_.drawHLine(x, y + 5, 1);
}

void Display::drawHappySparkles() {
    u8g2_.drawPixel(20, 15);
    u8g2_.drawPixel(19, 16); u8g2_.drawPixel(21, 16); u8g2_.drawPixel(20, 17);
    u8g2_.drawPixel(108, 16);
    u8g2_.drawPixel(107, 17); u8g2_.drawPixel(109, 17); u8g2_.drawPixel(108, 18);
}

void Display::drawRadarSweep() {
    int cx = 114, cy = 12, r = 8;
    u8g2_.drawCircle(cx, cy, r);
    float angle = fmod(millis() / 600.0f, 2.0f) * PI;
    int ex = cx + (int)(sin(angle) * r);
    int ey = cy - (int)(cos(angle) * r);
    u8g2_.drawLine(cx, cy, ex, ey);
    for (int i = 1; i <= 3; i++) {
        float trailAngle = angle - (i * 0.3f);
        int tx = cx + (int)(sin(trailAngle) * (r - i));
        int ty = cy - (int)(cos(trailAngle) * (r - i));
        u8g2_.drawPixel(tx, ty);
    }
    u8g2_.drawDisc(cx, cy, 1);
}

void Display::drawAngryAura() {
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

void Display::drawAlertMarks() {
    u8g2_.setFont(u8g2_font_6x12_tr);
    if ((millis() / 200) % 2) {
        u8g2_.drawStr(6, 14, "!");
        u8g2_.drawStr(116, 58, "!");
    } else {
        u8g2_.drawStr(116, 14, "!");
        u8g2_.drawStr(6, 58, "!");
    }
}

void Display::drawMenu(const char* title, const char** items, uint8_t itemCount, int selectedIdx, int arrowIdx) {
    clear();
    u8g2_.setFont(u8g2_font_6x10_tr);
    u8g2_.setDrawColor(1);

    const int lineHeight = 12;
    const int menuTop = 15;
    int scrollOffset = (selectedIdx >= 4) ? selectedIdx - 3 : 0;

    drawCentered(title, 11);
    u8g2_.drawHLine(2, 13, OLED_WIDTH - 4);

    for (int i = 0; i < 4; i++) {
        int idx = i + scrollOffset;
        if (idx >= itemCount) break;
        int boxY = menuTop + i * lineHeight;
        if (idx == selectedIdx) {
            u8g2_.drawBox(2, boxY, OLED_WIDTH - 4, lineHeight);
            u8g2_.setDrawColor(0);
        }
        if (idx == arrowIdx) u8g2_.drawStr(4, boxY + lineHeight - 2, "->");
        drawCentered(items[idx], boxY + lineHeight - 2);
        u8g2_.setDrawColor(1);
    }
    u8g2_.drawRFrame(0, 0, OLED_WIDTH, OLED_HEIGHT, 6);
    render();
}

void Display::drawConfirm(const char* line1, const char* line2) {
    clear();
    u8g2_.setFont(u8g2_font_6x10_tr);
    u8g2_.drawStr(54, 28, "OK");
    if (line1) drawCentered(line1, 42);
    if (line2) drawCentered(line2, 54);
    u8g2_.drawRFrame(0, 0, OLED_WIDTH, OLED_HEIGHT, 6);
    render();
}

void Display::drawStatusBar(bool touched, int volume) {
    u8g2_.drawHLine(0, 63, OLED_WIDTH);
    if (touched) {
        u8g2_.drawBox(2, 59, 8, 3);
        u8g2_.setFont(u8g2_font_4x6_tr);
        u8g2_.drawStr(12, 63, "TOUCH");
    }
    for (int i = 0; i < 3; i++) {
        int h = (volume > i * 85) ? 3 : 1;
        u8g2_.drawBox(118 - i * 4, 63 - h, 2, h);
    }
}