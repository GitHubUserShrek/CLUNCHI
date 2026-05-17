#include "magic_8ball.h"
#include "logo_bitmaps.h"
#include "audio.h" 

extern Audio audio;

const char* Magic8Ball::EIGHT_BALL_QUOTES_[16] = {
    "YES", "NO", "MAYBE", "FRICK YEAH", "HELL NO", 
    "ASK LATER", "DOUBT", "FOR SURE", "BAD IDEA", 
    "WHY NOT?", "100% YES", "NO WAY", "FACTS", 
    "DON'T BET", "LOOKS GOOD", "CURSED"
};

const char* Magic8Ball::getRandomQuote() {
    // Calculated safely inside class scope where private members are fully accessible
    int count = sizeof(EIGHT_BALL_QUOTES_) / sizeof(EIGHT_BALL_QUOTES_[0]);
    return EIGHT_BALL_QUOTES_[random(count)];
}

void Magic8Ball::reset() {
    shaking_        = false;
    shakeStartTime_ = 0;
    shakeIntensity_ = 0;
    shakeX_ = shakeY_ = 0;
    quote_          = nullptr;
}

void Magic8Ball::ask() {
    if (shaking_) return;
    shaking_        = true;
    shakeStartTime_ = millis();
    frame_          = 0;
    quote_          = nullptr;
    audio.beep(1000, 30);
}

void Magic8Ball::updateAndDraw(bool tiltMode) {
    const int CX = 64; const int CY = 30;
    uint32_t now = millis();

    if (shaking_) {
        frame_++;
        uint32_t elapsed = now - shakeStartTime_;
        float t = elapsed / 1500.0f; 

        if (t < 1.0f) {
            shakeIntensity_ = 1.0f - (t * t);
            shakeX_ = (int)(random(-3, 4) * shakeIntensity_);
            shakeY_ = (int)(random(-3, 4) * shakeIntensity_);
        } else { shakeIntensity_ = 0; shakeX_ = shakeY_ = 0; }

        drawBallShape(CX + shakeX_, CY + shakeY_);

        if (elapsed > SHAKE_DURATION) {
            shaking_ = false;
            quote_   = EIGHT_BALL_QUOTES_[random(16)];
            audio.beep(1200, 50); delay(50); audio.beep(1500, 80);
        }
    } else {
        drawBallShape(CX, CY);
        if (quote_ && strlen(quote_) > 0) drawAnswerWindow(CX, CY, quote_);
    }

    // --- SPLIT TOP TITLE BANNER ---
    u8g2_.setDrawColor(1); 
    u8g2_.setFont(u8g2_font_7x14B_tr); 
    u8g2_.drawStr(4, 12, "MAGIC");
    int rightW = u8g2_.getStrWidth("8 BALL");
    u8g2_.drawStr(128 - rightW - 4, 12, "8 BALL");
    // ------------------------------

    u8g2_.setFont(u8g2_font_5x7_tr);
    if (shaking_) {
        const char* dots[] = { "Shaking   ", "Shaking.  ", "Shaking.. ", "Shaking..." };
        const char* txt = dots[(frame_ >> 2) % 4];
        int w = u8g2_.getStrWidth(txt); u8g2_.drawStr((128 - w) / 2, 62, txt);
    } else {
        u8g2_.drawStr(4, 62, "Hold:Back");
        const char* hint = tiltMode ? "Shake:Ask" : "x2:Ask";
        int w = u8g2_.getStrWidth(hint); u8g2_.drawStr(128 - w - 4, 62, hint);
    }
    u8g2_.drawRFrame(0, 0, 128, 64, 6);
}

void Magic8Ball::drawBallShape(int cx, int cy) {
    u8g2_.drawXBMP(cx - 24, cy - 24, 48, 48, magic_8ball_48x48);
}

void Magic8Ball::drawAnswerWindow(int cx, int cy, const char* quote) {
    // --- POSITION OVERRIDE ---
    // Move window 5 pixels right (+5) and 3 pixels up (-3)
    int winX = cx + 5;
    int winY = cy - 3;

    // 1. Black out the white '8' circle at the new offset position
    u8g2_.setDrawColor(0); 
    u8g2_.drawDisc(winX, winY, 13);

    // 2. Format and center text inside the new dark circular window
    u8g2_.setDrawColor(1); 
    u8g2_.setFont(u8g2_font_4x6_tr); 

    String qStr(quote); int spaceIdx = qStr.indexOf(' ');
    
    // If quote has two words, split into two cleanly stacked lines inside the circle
    if (spaceIdx > 0 && qStr.length() > 5) {
        String l1 = qStr.substring(0, spaceIdx); String l2 = qStr.substring(spaceIdx + 1);
        int w1 = u8g2_.getStrWidth(l1.c_str()); int w2 = u8g2_.getStrWidth(l2.c_str());
        u8g2_.drawStr(winX - (w1 / 2), winY - 1, l1.c_str()); 
        u8g2_.drawStr(winX - (w2 / 2), winY + 7, l2.c_str());
    } else {
        // Single line quote perfectly centered at winX, winY
        int w = u8g2_.getStrWidth(quote); u8g2_.drawStr(winX - (w / 2), winY + 3, quote);
    }
}