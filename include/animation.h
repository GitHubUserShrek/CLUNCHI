#pragma once
#include <Arduino.h>
#include "config.h"
#include "mood.h"
#include "wids.h" 

enum AttackAnimStage {
    STAGE_IDLE = 0,
    STAGE_ENRAGED_BUFFER,
    STAGE_ATTACK_FACE
};

struct AnimState {
    float blink = 0;
    bool  heartEyes = false;
    bool  spiralEyes = false;
    bool  dribbling = false;

    bool       showAttackFace = false;
    AttackType currentAttack  = ATTACK_NONE;

    int   pupilX = 0;
    int   pupilY = 0;
    float bounceY = 0;
    float breathY = 0;
    float squishY = 0;
    float headTilt = 0;
    float spiralAngle = 0;

    float leftEyeOffX  = 0; float leftEyeOffY  = 0;
    float rightEyeOffX = 0; float rightEyeOffY = 0;
    float mouthOffX    = 0; float mouthOffY    = 0;
};

class Animation {
public:
    void begin();
    void update(Mood currentMood);
    void onMoodChange(); 

    void triggerBlink();
    void triggerHeartEyes(uint32_t duration = 4000);
    void triggerSpiralEyes(uint32_t duration = 4000);
    void triggerDribble(uint32_t duration = 5000);
    void triggerDribbleHit();
    void triggerSquish();
    void triggerBounce();
    
    void triggerAttackFace(AttackType type, uint32_t duration = 5000);

    bool isDribbling() const { return isDribbling_; }
    AnimState getState() const;

private:
    uint32_t lastBlink_ = 0;
    float    blinkPhase_ = 0;
    bool     isBlinking_ = false;

    uint32_t heartEyesEndTime_  = 0; bool showHeartEyes_  = false;
    uint32_t spiralEyesEndTime_ = 0; bool showSpiralEyes_ = false; float spiralAngle_ = 0;

    AttackAnimStage attackAnimStage_    = STAGE_IDLE;
    bool            isAttackAnimActive_ = false;
    uint32_t        attackDetectTime_   = 0;
    uint32_t        attackFaceEndTime_  = 0;
    AttackType      currentAttackType_  = ATTACK_NONE;

    bool     isDribbling_ = false; uint32_t dribbleEndTime_ = 0; float dribbleBounce_ = 0;
    uint32_t dribbleHitTime_ = 0; float dribbleVelocity_ = 0; float dribblePos_ = 0;

    float scatterLeftEyeX_  = 0, scatterLeftEyeY_  = 0;
    float scatterRightEyeX_ = 0, scatterRightEyeY_ = 0;
    float scatterMouthX_    = 0, scatterMouthY_     = 0;

    float targetLeftEyeX_   = 0, targetLeftEyeY_    = 0;
    float targetRightEyeX_  = 0, targetRightEyeY_   = 0;
    float targetMouthX_     = 0, targetMouthY_       = 0;

    uint32_t lastLookTime_ = 0; int pupilX_ = 0; int pupilY_ = 0;
    float squishY_ = 0; float bounceY_ = 0; float breathY_ = 0; float headTilt_ = 0;
    bool bounceActivate_ = false; uint32_t bounceStart_ = 0;
};