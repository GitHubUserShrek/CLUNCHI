#include "dice_roller.h"
#include "dice_bitmaps.h"

void DiceRoller::reset() {
    rollStartTime_    = 0;
    spinSpeed_        = 0;
    currentRotation_  = 0;
    shakeX_           = 0;
    shakeY_           = 0;
    hasCriticalBurst_ = false;
    for (auto& p : particles_) p.life = 0;
}

void DiceRoller::draw(const char* dieLabel, int result,
                      bool rolling, int frame, bool tiltMode) {
    int dieType = parseDieType(dieLabel);

    const int CX = 64;
    const int CY = 30;

    if (rolling) {
        if (rollStartTime_ == 0) {
            rollStartTime_    = millis();
            spinSpeed_        = 0.8f + random(0, 60) / 100.0f;
            hasCriticalBurst_ = false;
            for (auto& p : particles_) p.life = 0;
        }

        uint32_t elapsed = millis() - rollStartTime_;
        float    t       = elapsed / 1800.0f;

        if (t < 1.0f) {
            spinSpeed_ = 0.8f * (1.0f - t * t);

            if (spinSpeed_ > 0.3f) {
                shakeX_ = random(-2, 3);
                shakeY_ = random(-1, 2);
            } else if (spinSpeed_ > 0.1f) {
                shakeX_ = random(-1, 2);
                shakeY_ = random(-1, 2);
            } else {
                shakeX_ = shakeY_ = 0;
            }
        } else {
            spinSpeed_ = 0;
            shakeX_    = 0;
            shakeY_    = 0;
        }

        int displayValue = (spinSpeed_ > 0.05f)
                         ? random(1, dieType + 1)
                         : result;

        drawDieWithPips(CX + shakeX_, CY + shakeY_,
                        dieType, displayValue, true);

    } else {
        rollStartTime_ = 0;
        drawDieWithPips(CX, CY, dieType,
                        result > 0 ? result : 0, false);

        if (dieType == 20 && result > 0 &&
            (result == 20 || result == 1)) {
            drawParticleBurst(CX, CY - 8,
                              result == 20 ? 1.0f : 0.6f);
        }
    }

    u8g2_.setDrawColor(1);
    u8g2_.setFont(u8g2_font_7x14B_tr);
    u8g2_.drawStr(4, 12, dieLabel);

    if (!rolling && result > 0 && dieType == 20) {
        u8g2_.setFont(u8g2_font_5x7_tr);
        if (result == 20) {
            u8g2_.drawStr(84, 8, "NAT 20!");
        } else if (result == 1) {
            u8g2_.drawStr(78, 8, "CRIT FAIL");
        }
    }

    u8g2_.setFont(u8g2_font_5x7_tr);
    if (rolling) {
        const char* dots[] = {
            "Rolling   ", "Rolling.  ", "Rolling.. ", "Rolling..."
        };
        const char* txt = dots[(frame >> 2) % 4];
        int w = u8g2_.getStrWidth(txt);
        u8g2_.drawStr((128 - w) / 2, 62, txt);
    } else {
        u8g2_.drawStr(4, 62, "Tap:Die");
        const char* hint = tiltMode ? "Shake:Roll" : "x2:Roll";
        int w = u8g2_.getStrWidth(hint);
        u8g2_.drawStr(128 - w - 4, 62, hint);
    }

    u8g2_.drawRFrame(0, 0, 128, 64, 6);
}

int DiceRoller::parseDieType(const char* label) {
    if (strstr(label, "D100")) return 100;
    if (strstr(label, "D20"))  return 20;
    if (strstr(label, "D12"))  return 12;
    if (strstr(label, "D10"))  return 10;
    if (strstr(label, "D8"))   return 8;
    if (strstr(label, "D6"))   return 6;
    if (strstr(label, "D4"))   return 4;
    return 20;
}

void DiceRoller::drawDieShape(int cx, int cy, int dieType) {
    const unsigned char* bmp = nullptr;

    switch (dieType) {
        case 4:   bmp = d4_bitmap;   break;
        case 6:   bmp = d6_bitmap;   break;
        case 8:   bmp = d8_bitmap;   break;
        case 10:  bmp = d10_bitmap;  break;
        case 12:  bmp = d12_bitmap;  break;
        case 20:  bmp = d20_bitmap;  break;
        case 100: bmp = d100_bitmap; break;
        default:  bmp = d20_bitmap;  break;
    }

    if (bmp) {
        u8g2_.drawXBMP(cx - DIE_W / 2, cy - DIE_H / 2,
                       DIE_W, DIE_H, bmp);
    }
}

void DiceRoller::drawD6Pips(int cx, int cy, int value) {
    if (value < 1 || value > 6) return;

    int faceCX = cx - 4;
    int faceCY = cy + 5;

    int lx = faceCX - 12;
    int mx = faceCX;
    int rx = faceCX + 12;
    int ty = faceCY - 12;
    int my = faceCY;
    int by = faceCY + 12;
    int pr = 3;

    switch (value) {
        case 1:
            u8g2_.drawDisc(mx, my, pr);
            break;
        case 2:
            u8g2_.drawDisc(rx, ty, pr);
            u8g2_.drawDisc(lx, by, pr);
            break;
        case 3:
            u8g2_.drawDisc(rx, ty, pr);
            u8g2_.drawDisc(mx, my, pr);
            u8g2_.drawDisc(lx, by, pr);
            break;
        case 4:
            u8g2_.drawDisc(lx, ty, pr);
            u8g2_.drawDisc(rx, ty, pr);
            u8g2_.drawDisc(lx, by, pr);
            u8g2_.drawDisc(rx, by, pr);
            break;
        case 5:
            u8g2_.drawDisc(lx, ty, pr);
            u8g2_.drawDisc(rx, ty, pr);
            u8g2_.drawDisc(mx, my, pr);
            u8g2_.drawDisc(lx, by, pr);
            u8g2_.drawDisc(rx, by, pr);
            break;
        case 6:
            u8g2_.drawDisc(lx, ty, pr);
            u8g2_.drawDisc(rx, ty, pr);
            u8g2_.drawDisc(lx, my, pr);
            u8g2_.drawDisc(rx, my, pr);
            u8g2_.drawDisc(lx, by, pr);
            u8g2_.drawDisc(rx, by, pr);
            break;
    }
}

void DiceRoller::drawNumber(int cx, int cy, int dieType, int value) {
    if (value < 1) return;

    char buf[8];
    snprintf(buf, sizeof(buf), "%d", value);

    if (value >= 100) {
        u8g2_.setFont(u8g2_font_7x14B_tr);
    } else {
        u8g2_.setFont(u8g2_font_logisoso16_tn);
    }

    int w    = u8g2_.getStrWidth(buf);
    int numY = (dieType == 4) ? cy + 13 : cy + 7;
    int numX = cx - w / 2;

    u8g2_.setDrawColor(0);
    u8g2_.drawBox(numX - 2, numY - 16, w + 4, 19);
    u8g2_.setDrawColor(1);
    u8g2_.drawStr(numX, numY, buf);
}

void DiceRoller::drawDieWithPips(int cx, int cy, int dieType,
                                  int value, bool rolling) {
    drawDieShape(cx, cy, dieType);

    if (value < 1) return;  

    if (dieType == 6) {
        drawD6Pips(cx, cy, value);
    } else {
        drawNumber(cx, cy, dieType, value);
    }
}

void DiceRoller::drawParticleBurst(int cx, int cy, float intensity) {
    if (!hasCriticalBurst_) {
        hasCriticalBurst_ = true;
        for (auto& p : particles_) {
            p.x   = cx + random(-10, 11);
            p.y   = cy + random(-10, 11);
            p.vx  = random(-4, 5);
            p.vy  = random(-5, 2);
            p.life = (uint8_t)(18 + random(12));
        }
    }

    for (auto& p : particles_) {
        if (p.life > 0) {
            p.x += p.vx;
            p.y += p.vy;
            p.vy += 1; 
            p.life--;
            u8g2_.drawPixel(p.x, p.y);
            if (p.life > 8) {
                u8g2_.drawPixel(p.x + 1, p.y);
            }
        }
    }
}