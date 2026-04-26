#pragma once
#include <Arduino.h>
#include "config.h"

class Audio {
public:
    void begin();
    void stop();
    void setVolume(uint8_t vol);
    uint8_t getVolume() const { return volume_; }
    void toggleMute();
    bool isMuted() const { return muted_; }

    // ── Raw ──────────────────────────────────────────
    void beep(int freq, uint32_t duration);

    // ── Universal ────────────────────────────────────
    void chirp();           // tap feedback

    // ── Mood sounds ──────────────────────────────────
    void happy();           // ascending cheerful
    void sleepy();          // descending drowsy
    void annoyed();         // single low grunt
    void curious();         // two rising notes
    void jazzed();          // two bouncy notes
    void enraged();         // alarm sequence
    void dead();            // descending death
    void dribble();         // chaotic entry

    // ── Spiral / heart eyes ──────────────────────────
    void spiralEyes();      // dizzy sound
    void heartEyes();       // same as happy()

    // ── Radar ────────────────────────────────────────
    void radarOn();         // ascending triple
    void radarOff();        // descending double
    void radarPing();       // fast triple high
    void radarAlert();      // alarm (same as enraged)

private:
    uint8_t volume_ = DEFAULT_VOLUME;
    uint8_t savedVolume_ = DEFAULT_VOLUME;  // NEW
    bool    muted_ = false;                 // NEW
    void tone(int freq);
};