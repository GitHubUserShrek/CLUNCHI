#include "animation.h"

// Smooth lerp helper — eases current toward target
static float lerpTo(float current, float target, float speed) {
    float diff = target - current;
    if (fabs(diff) < 0.1f) return target;
    return current + diff * speed;
}

void Animation::begin() {
    lastBlink_ = millis() + random(2000, 5000);
    lastLookTime_ = millis();
}

void Animation::update(Mood currentMood) {
    uint32_t now = millis();

    // ── Blink timing ─────────────────────────────────────────────
    uint32_t blinkMin = 3000, blinkMax = 7000;
    if (currentMood == VIGILANT) { blinkMin = 8000; blinkMax = 15000; }
    if (currentMood == ENRAGED)  { blinkMin = 6000; blinkMax = 12000; }

    if (isBlinking_) {
        blinkPhase_ += 0.15f;
        if (blinkPhase_ >= 1.0f) {
            isBlinking_ = false;
            blinkPhase_ = 0;
            lastBlink_ = now + random(blinkMin, blinkMax);
        }
    } else if (now > lastBlink_) {
        isBlinking_ = true;
        blinkPhase_ = 0;
    }

    // ── Timed eye effects ────────────────────────────────────────
    if (showHeartEyes_ && now > heartEyesEndTime_) {
        showHeartEyes_ = false;
    }
    if (showSpiralEyes_ && now > spiralEyesEndTime_) {
        showSpiralEyes_ = false;
    }
    if (isDribbling_ && now > dribbleEndTime_) {
        isDribbling_ = false;
        dribbleBounce_ = 0;
        dribblePos_ = 0;
        dribbleVelocity_ = 0;
    }

    // ── Spiral rotation ──────────────────────────────────────────
    if (showSpiralEyes_) {
        spiralAngle_ = fmod(now / 200.0f, 2.0f * PI);
    }

    // ── Dribble physics ──────────────────────────────────────────
    if (isDribbling_) {
        const float gravity = 0.6f;
        const float damping = 0.85f;

        dribbleVelocity_ += (-dribblePos_ * gravity);
        dribbleVelocity_ *= damping;
        dribblePos_ += dribbleVelocity_;

        if (fabs(dribblePos_) < 0.1f && fabs(dribbleVelocity_) < 0.1f) {
            dribblePos_ = 0;
            dribbleVelocity_ = 0;
        }

        dribbleBounce_ = dribblePos_;

        // ── Scatter: drift back to center ────────────────────────
        // Speed increases as we get closer (smooth ease-out)
        const float reformSpeed = 0.08f;

        scatterLeftEyeX_  = lerpTo(scatterLeftEyeX_,  0, reformSpeed);
        scatterLeftEyeY_  = lerpTo(scatterLeftEyeY_,  0, reformSpeed);
        scatterRightEyeX_ = lerpTo(scatterRightEyeX_, 0, reformSpeed);
        scatterRightEyeY_ = lerpTo(scatterRightEyeY_, 0, reformSpeed);
        scatterMouthX_    = lerpTo(scatterMouthX_,     0, reformSpeed);
        scatterMouthY_    = lerpTo(scatterMouthY_,     0, reformSpeed);
    } else {
        // Not dribbling — snap everything to center
        scatterLeftEyeX_  = 0; scatterLeftEyeY_  = 0;
        scatterRightEyeX_ = 0; scatterRightEyeY_ = 0;
        scatterMouthX_    = 0; scatterMouthY_     = 0;
    }

    // ── Pupil position ───────────────────────────────────────────
    if (currentMood == CURIOUS) {
        pupilX_ = 3;
        pupilY_ = -1;
    } else if (currentMood == VIGILANT) {
        pupilX_ = (int)(sin(now / 800.0f) * 4.0f);
        pupilY_ = (int)(sin(now / 1200.0f) * 1.0f);
    } else if (currentMood == ENRAGED || currentMood == DEAD) {
        pupilX_ = 0;
        pupilY_ = 0;
    } else if (now - lastLookTime_ > (uint32_t)random(2000, 5000)) {
        pupilX_ = random(-3, 4);
        pupilY_ = random(-2, 3);
        lastLookTime_ = now;
    }

    // ── Squish decay ─────────────────────────────────────────────
    if (squishY_ > 0) {
        squishY_ -= 0.8f;
        if (squishY_ < 0) squishY_ = 0;
    }

    // ── Bounce ───────────────────────────────────────────────────
    if (isDribbling_) {
        bounceY_ = dribbleBounce_;
    }
    else if (bounceActivate_) {
        float elapsed = (now - bounceStart_) / 1000.0f;
        if (elapsed < 0.5f) {
            bounceY_ = sin(elapsed * PI * 4) * 3.0f * (1.0f - elapsed * 2);
        } else {
            bounceActivate_ = false;
            bounceY_ = 0;
        }
    }
    else if (currentMood == HAPPY) {
        bounceY_ = sin(now / 80.0f) * 3.0f;
    }
    else if (currentMood == JAZZED) {
        bounceY_ = sin(now / 40.0f) * 2.0f;
    }
    else if (currentMood == ENRAGED) {
        bounceY_ = sin(now / 25.0f) * 2.5f;
    }
    else if (currentMood == VIGILANT) {
        bounceY_ = sin(now / 600.0f) * 0.8f;
    }
    else {
        bounceY_ = 0;
    }

    // ── Breathing ────────────────────────────────────────────────
    if (currentMood == SLEEPY) {
        breathY_ = sin(now / 1000.0f) * 2.0f;
    } else if (currentMood == VIGILANT) {
        breathY_ = sin(now / 1500.0f) * 1.0f;
    } else {
        breathY_ = 0;
    }

    // ── Head tilt ────────────────────────────────────────────────
    if (isDribbling_) {
        headTilt_ = dribbleBounce_ * 0.5f;
    }
    else if (currentMood == CURIOUS) {
        headTilt_ = sin(now / 600.0f) * 2.5f;
    }
    else if (currentMood == ENRAGED) {
        headTilt_ = sin(now / 150.0f) * 1.5f;
    }
    else {
        headTilt_ = 0;
    }
}

void Animation::onMoodChange() {
    // Clear all mood-triggered visual effects
    // Preserves touch-driven state (dribble, squish, bounce)
    showHeartEyes_  = false;
    showSpiralEyes_ = false;
    headTilt_       = 0;
    breathY_        = 0;

    // Future-proof: add any new mood-triggered effects here
    // e.g.:
    // showCheekBlush_ = false;
    // showSweatDrop_  = false;
    // tongueOut_      = false;
}

void Animation::triggerBlink() {
    isBlinking_ = true;
    blinkPhase_ = 0;
}

void Animation::triggerHeartEyes(uint32_t duration) {
    showHeartEyes_ = true;
    showSpiralEyes_ = false;
    isDribbling_ = false;
    heartEyesEndTime_ = millis() + duration;
}

void Animation::triggerSpiralEyes(uint32_t duration) {
    showSpiralEyes_ = true;
    showHeartEyes_ = false;
    spiralEyesEndTime_ = millis() + duration;
    spiralAngle_ = 0;
}

void Animation::triggerDribble(uint32_t duration) {
    isDribbling_ = true;
    dribbleEndTime_ = millis() + duration;
    dribblePos_ = 0;
    dribbleVelocity_ = 0;
    // Clear scatter positions
    scatterLeftEyeX_  = 0; scatterLeftEyeY_  = 0;
    scatterRightEyeX_ = 0; scatterRightEyeY_ = 0;
    scatterMouthX_    = 0; scatterMouthY_     = 0;
    triggerSpiralEyes(duration);
}

void Animation::triggerDribbleHit() {
    if (!isDribbling_) return;

    // Slam downward
    dribbleVelocity_ = 6.0f;

    // Scatter eyes and mouth to random positions
    // Range: -12 to +12 pixels X, -8 to +8 pixels Y
    scatterLeftEyeX_  = random(-12, 13);
    scatterLeftEyeY_  = random(-8, 9);
    scatterRightEyeX_ = random(-12, 13);
    scatterRightEyeY_ = random(-8, 9);
    scatterMouthX_    = random(-10, 11);
    scatterMouthY_    = random(-6, 7);

    // Extend timers
    dribbleEndTime_ = millis() + 5000;
    spiralEyesEndTime_ = dribbleEndTime_;

}


void Animation::triggerSquish() {
    squishY_ = 4.0f;
}

void Animation::triggerBounce() {
    bounceActivate_ = true;
    bounceStart_ = millis();
}

AnimState Animation::getState() const {
    AnimState state;
    state.blink       = isBlinking_ ? sin(blinkPhase_ * PI) : 0;
    state.heartEyes   = showHeartEyes_;
    state.spiralEyes  = showSpiralEyes_;
    state.dribbling   = isDribbling_;
    state.pupilX      = pupilX_;
    state.pupilY      = pupilY_;
    state.bounceY     = bounceY_;
    state.breathY     = breathY_;
    state.squishY     = squishY_;
    state.headTilt    = headTilt_;
    state.spiralAngle = spiralAngle_;

    state.leftEyeOffX  = scatterLeftEyeX_;
    state.leftEyeOffY  = scatterLeftEyeY_;
    state.rightEyeOffX = scatterRightEyeX_;
    state.rightEyeOffY = scatterRightEyeY_;
    state.mouthOffX    = scatterMouthX_;
    state.mouthOffY    = scatterMouthY_;

    return state;
}