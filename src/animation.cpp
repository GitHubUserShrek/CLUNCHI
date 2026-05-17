#include "animation.h"

static float lerpTo(float current, float target, float speed) {
    float diff = target - current; if (fabs(diff) < 0.1f) return target; return current + diff * speed;
}

static AttackType getHighestPriorityRecentAttack(uint32_t withinMs) {
    if (widsLogCount == 0) return ATTACK_NONE;
    uint32_t now = millis();
    AttackType highest = ATTACK_NONE;
    int highestPriority = -1;

    for (int i = 0; i < widsLogCount; i++) {
        int idx = (widsLogHead - 1 - i + WIDS_LOG_SIZE) % WIDS_LOG_SIZE;
        ThreatEvent& e = widsLog[idx];

        if (now - e.timestamp <= withinMs) {
            int p = 0;
            switch(e.type) {
                case ATTACK_EVIL_TWIN:         p = 5; break; 
                case ATTACK_HANDSHAKE_CAPTURE: p = 4; break; 
                case ATTACK_CTS_JAMMING:       p = 3; break; 
                case ATTACK_BEACON_FLOOD:      p = 2; break; 
                case ATTACK_DEAUTH:
                case ATTACK_DISASSOC:          p = 1; break; 
                default:                       p = 0; break;
            }
            if (p > highestPriority) {
                highestPriority = p;
                highest = e.type;
            }
        }
    }
    return highest;
}

void Animation::begin() { lastBlink_ = millis() + random(2000, 5000); lastLookTime_ = millis(); }

void Animation::update(Mood currentMood) {
    uint32_t now = millis();

    if (attackAnimStage_ == STAGE_IDLE) {
        if (widsHasRecentAlert(3000) && widsLogCount > 0) {
            attackAnimStage_    = STAGE_ENRAGED_BUFFER;
            attackDetectTime_   = now;
            isAttackAnimActive_ = false;
        }
    } 
    else if (attackAnimStage_ == STAGE_ENRAGED_BUFFER) {
        if (now - attackDetectTime_ >= 2500) {
            currentAttackType_  = getHighestPriorityRecentAttack(5000); 
            isAttackAnimActive_ = true;  
            attackFaceEndTime_  = now + 2500; 
            attackAnimStage_    = STAGE_ATTACK_FACE;
        }
    } 
    else if (attackAnimStage_ == STAGE_ATTACK_FACE) {
        if (now > attackFaceEndTime_) {
            if (widsHasRecentAlert(3000)) {
                currentAttackType_ = getHighestPriorityRecentAttack(5000);
                attackFaceEndTime_ = now + 2500; 
            } else {
                attackAnimStage_    = STAGE_IDLE;
                isAttackAnimActive_ = false;
                currentAttackType_  = ATTACK_NONE;
            }
        }
    }

    uint32_t blinkMin = 3000, blinkMax = 7000;
    if (currentMood == VIGILANT || currentMood == TACTICAL) { blinkMin = 8000; blinkMax = 15000; }
    if (currentMood == ENRAGED)  { blinkMin = 6000; blinkMax = 12000; }

    bool suppressBlink = isAttackAnimActive_ && 
        (currentAttackType_ == ATTACK_DEAUTH || currentAttackType_ == ATTACK_DISASSOC || 
         currentAttackType_ == ATTACK_CTS_JAMMING || currentAttackType_ == ATTACK_BEACON_FLOOD);

    if (isBlinking_ && !suppressBlink) {
        blinkPhase_ += 0.15f;
        if (blinkPhase_ >= 1.0f) { isBlinking_ = false; blinkPhase_ = 0; lastBlink_ = now + random(blinkMin, blinkMax); }
    } else if (now > lastBlink_ && !suppressBlink) { isBlinking_ = true; blinkPhase_ = 0; }

    if (showHeartEyes_ && now > heartEyesEndTime_)   showHeartEyes_ = false;
    if (showSpiralEyes_ && now > spiralEyesEndTime_) showSpiralEyes_ = false;
    if (isDribbling_ && now > dribbleEndTime_) { isDribbling_ = false; dribbleBounce_ = 0; dribblePos_ = 0; dribbleVelocity_ = 0; }
    if (showSpiralEyes_ || (isAttackAnimActive_ && currentAttackType_ == ATTACK_BEACON_FLOOD)) { spiralAngle_ = fmod(now / 200.0f, 2.0f * PI); }

    if (isDribbling_) {
        const float gravity = 0.6f; const float damping = 0.85f;
        dribbleVelocity_ += (-dribblePos_ * gravity); dribbleVelocity_ *= damping; dribblePos_ += dribbleVelocity_;
        if (fabs(dribblePos_) < 0.1f && fabs(dribbleVelocity_) < 0.1f) { dribblePos_ = 0; dribbleVelocity_ = 0; }
        dribbleBounce_ = dribblePos_;
        const float reformSpeed = 0.08f;
        scatterLeftEyeX_ = lerpTo(scatterLeftEyeX_, 0, reformSpeed); scatterLeftEyeY_ = lerpTo(scatterLeftEyeY_, 0, reformSpeed);
        scatterRightEyeX_ = lerpTo(scatterRightEyeX_, 0, reformSpeed); scatterRightEyeY_ = lerpTo(scatterRightEyeY_, 0, reformSpeed);
        scatterMouthX_ = lerpTo(scatterMouthX_, 0, reformSpeed); scatterMouthY_ = lerpTo(scatterMouthY_, 0, reformSpeed);
    } else { scatterLeftEyeX_ = 0; scatterLeftEyeY_ = 0; scatterRightEyeX_ = 0; scatterRightEyeY_ = 0; scatterMouthX_ = 0; scatterMouthY_ = 0; }

    if (isAttackAnimActive_) {
        if (currentAttackType_ == ATTACK_HANDSHAKE_CAPTURE) { pupilX_ = ((now % 800) < 400) ? -5 : 5; pupilY_ = 0; } 
        else if (currentAttackType_ == ATTACK_CTS_JAMMING)  { pupilX_ = 0; pupilY_ = 2; } 
        else if (currentAttackType_ == ATTACK_EVIL_TWIN)    { pupilX_ = (int)(sin(now / 300.0f) * 2.0f); pupilY_ = 0; } 
        else { pupilX_ = 0; pupilY_ = 0; }
    } else if (currentMood == CURIOUS) { pupilX_ = 3; pupilY_ = -1; } 
    else if (currentMood == VIGILANT || currentMood == TACTICAL) { pupilX_ = (int)(sin(now / 800.0f) * 4.0f); pupilY_ = (int)(sin(now / 1200.0f) * 1.0f); } 
    else if (currentMood == DRIVING)  { pupilX_ = 0; pupilY_ = 0; } 
    else if (currentMood == ENRAGED || currentMood == DEAD) { pupilX_ = 0; pupilY_ = 0; } 
    else if (now - lastLookTime_ > (uint32_t)random(2000, 5000)) { pupilX_ = random(-3, 4); pupilY_ = random(-2, 3); lastLookTime_ = now; }

    if (squishY_ > 0) { squishY_ -= 0.8f; if (squishY_ < 0) squishY_ = 0; }

    if (isDribbling_) { bounceY_ = dribbleBounce_; }
    else if (bounceActivate_) {
        float elapsed = (now - bounceStart_) / 1000.0f;
        if (elapsed < 0.5f) bounceY_ = sin(elapsed * PI * 4) * 3.0f * (1.0f - elapsed * 2); else { bounceActivate_ = false; bounceY_ = 0; }
    }
    else if (isAttackAnimActive_) {
        if (currentAttackType_ == ATTACK_CTS_JAMMING) bounceY_ = sin(now / 20.0f) * 1.5f;
        else if (currentAttackType_ == ATTACK_EVIL_TWIN) bounceY_ = sin(now / 200.0f) * 1.0f;
        else bounceY_ = sin(now / 25.0f) * 2.5f;
    }
    else if (currentMood == HAPPY)    { bounceY_ = sin(now / 80.0f) * 3.0f; }
    else if (currentMood == JAZZED)   { bounceY_ = sin(now / 40.0f) * 2.0f; }
    else if (currentMood == ENRAGED)  { bounceY_ = sin(now / 25.0f) * 2.5f; }
    else if (currentMood == VIGILANT) { bounceY_ = sin(now / 600.0f) * 0.8f; }     
    else if (currentMood == TACTICAL) { bounceY_ = sin(now / 400.0f) * 1.2f; }    
    else if (currentMood == DRIVING)  { bounceY_ = sin(now / 300.0f) * 1.5f; }
    else { bounceY_ = 0; }

    if (currentMood == SLEEPY) breathY_ = sin(now / 1000.0f) * 2.0f; 
    else if (currentMood == VIGILANT || currentMood == TACTICAL) breathY_ = sin(now / 1500.0f) * 1.0f; 
    else breathY_ = 0;

    if (isDribbling_) headTilt_ = dribbleBounce_ * 0.5f;
    else if (currentMood == CURIOUS) headTilt_ = sin(now / 600.0f) * 2.5f;
    else if (currentMood == ENRAGED) headTilt_ = sin(now / 150.0f) * 1.5f;
    else if (currentMood == DRIVING || currentMood == TACTICAL) headTilt_ = sin(now / 1200.0f) * 2.0f;
    else headTilt_ = 0;
}

void Animation::onMoodChange() { showHeartEyes_ = false; showSpiralEyes_ = false; headTilt_ = 0; breathY_ = 0; }
void Animation::triggerBlink() { isBlinking_ = true; blinkPhase_ = 0; }
void Animation::triggerHeartEyes(uint32_t duration) { showHeartEyes_ = true; showSpiralEyes_ = false; isDribbling_ = false; isAttackAnimActive_ = false; heartEyesEndTime_ = millis() + duration; }
void Animation::triggerSpiralEyes(uint32_t duration) { showSpiralEyes_ = true; showHeartEyes_ = false; isAttackAnimActive_ = false; spiralEyesEndTime_ = millis() + duration; spiralAngle_ = 0; }

void Animation::triggerAttackFace(AttackType type, uint32_t duration) {
    isAttackAnimActive_ = true; currentAttackType_ = type; attackFaceEndTime_ = millis() + duration; showHeartEyes_ = false; showSpiralEyes_ = (type == ATTACK_BEACON_FLOOD);
}

void Animation::triggerDribble(uint32_t duration) {
    isDribbling_ = true; dribbleEndTime_ = millis() + duration; dribblePos_ = 0; dribbleVelocity_ = 0;
    scatterLeftEyeX_ = 0; scatterLeftEyeY_ = 0; scatterRightEyeX_ = 0; scatterRightEyeY_ = 0; scatterMouthX_ = 0; scatterMouthY_ = 0;
    triggerSpiralEyes(duration);
}

void Animation::triggerDribbleHit() {
    if (!isDribbling_) return; dribbleVelocity_ = 6.0f;
    scatterLeftEyeX_ = random(-12, 13); scatterLeftEyeY_ = random(-8, 9); scatterRightEyeX_ = random(-12, 13); scatterRightEyeY_ = random(-8, 9); scatterMouthX_ = random(-10, 11); scatterMouthY_ = random(-6, 7);
    dribbleEndTime_ = millis() + 5000; spiralEyesEndTime_ = dribbleEndTime_;
}

void Animation::triggerSquish() { squishY_ = 4.0f; }
void Animation::triggerBounce() { bounceActivate_ = true; bounceStart_ = millis(); }

AnimState Animation::getState() const {
    AnimState state;
    state.blink          = isBlinking_ ? sin(blinkPhase_ * PI) : 0;
    state.heartEyes      = showHeartEyes_;
    state.spiralEyes     = showSpiralEyes_;
    state.dribbling      = isDribbling_;
    state.showAttackFace = isAttackAnimActive_;
    state.currentAttack  = currentAttackType_;
    state.pupilX         = pupilX_; state.pupilY = pupilY_; state.bounceY = bounceY_; state.breathY = breathY_;
    state.squishY        = squishY_; state.headTilt = headTilt_; state.spiralAngle = spiralAngle_;
    state.leftEyeOffX    = scatterLeftEyeX_;  state.leftEyeOffY  = scatterLeftEyeY_;
    state.rightEyeOffX   = scatterRightEyeX_; state.rightEyeOffY = scatterRightEyeY_;
    state.mouthOffX      = scatterMouthX_;    state.mouthOffY    = scatterMouthY_;
    return state;
}