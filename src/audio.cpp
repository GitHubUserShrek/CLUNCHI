#include "audio.h"
#include <Preferences.h>

void Audio::begin() {
    #if ESP_ARDUINO_VERSION_MAJOR >= 3
        ledcAttach(PIN_SPEAKER, 2000, 8);
    #else
        ledcSetup(TONE_CHANNEL, 2000, 8);
        ledcAttachPin(PIN_SPEAKER, TONE_CHANNEL);
    #endif
    stop();

    // Load saved state
    Preferences prefs;
    if (prefs.begin("clunchi", true)) {
        volume_      = prefs.getUChar("volume", DEFAULT_VOLUME);
        savedVolume_ = prefs.getUChar("savedVol", DEFAULT_VOLUME);
        muted_       = prefs.getBool("muted", false);
        prefs.end();
    }
    if (muted_) volume_ = 0;
}

void Audio::tone(int freq) {
    #if ESP_ARDUINO_VERSION_MAJOR >= 3
        ledcWriteTone(PIN_SPEAKER, freq);
        ledcWrite(PIN_SPEAKER, volume_);
    #else
        ledcWriteTone(TONE_CHANNEL, freq);
        ledcWrite(TONE_CHANNEL, volume_);
    #endif
}

void Audio::stop() {
    #if ESP_ARDUINO_VERSION_MAJOR >= 3
        ledcWrite(PIN_SPEAKER, 0);
    #else
        ledcWrite(TONE_CHANNEL, 0);
    #endif
}

void Audio::beep(int freq, uint32_t duration) {
    tone(freq);
    delay(duration);
    stop();
}

void Audio::setVolume(uint8_t vol) {
    volume_ = vol;
    if (vol > 0) muted_ = false;

    Preferences prefs;
    if (prefs.begin("clunchi", false)) {
        prefs.putUChar("volume", vol);
        prefs.putBool("muted", false);
        prefs.end();
    }
}

void Audio::toggleMute() {
    Preferences prefs;
    if (muted_) {
        muted_ = false;
        volume_ = savedVolume_;
        if (prefs.begin("clunchi", false)) {
            prefs.putBool("muted", false);
            prefs.putUChar("volume", volume_);
            prefs.end();
        }
        Serial.println("[Audio] Unmuted");
    } else {
        savedVolume_ = volume_;
        muted_ = true;
        volume_ = 0;
        if (prefs.begin("clunchi", false)) {
            prefs.putBool("muted", true);
            prefs.putUChar("savedVol", savedVolume_);
            prefs.end();
        }
        Serial.println("[Audio] Muted");
    }
}

// ── Universal ────────────────────────────────────────
void Audio::chirp() {
    for (int f = 2000; f < 3500; f += 100) {
        tone(f);
        delay(8);
    }
    stop();
}

// ── Mood sounds ──────────────────────────────────────
void Audio::happy() {
    beep(1500, 60);
    delay(30);
    beep(2000, 60);
    delay(30);
    beep(2500, 80);
}

void Audio::sleepy() {
    for (int f = 800; f > 400; f -= 20) {
        tone(f);
        delay(15);
    }
    stop();
}

void Audio::annoyed() {
    beep(300, 100);
}

void Audio::curious() {
    beep(600, 60);
    delay(40);
    beep(800, 60);
}

void Audio::jazzed() {
    beep(400, 50);
    delay(100);
    beep(600, 50);
}

void Audio::enraged() {
    beep(200, 200);
    delay(100);
    beep(200, 200);
    delay(100);
    beep(150, 300);
}

void Audio::dead() {
    beep(400, 200);
    delay(150);
    beep(200, 400);
}

void Audio::dribble() {
    beep(800, 40);
    delay(50);
    beep(600, 40);
    delay(50);
    beep(900, 40);
    delay(50);
    beep(500, 40);
}

void Audio::spiralEyes() {
    beep(500, 80);
    delay(60);
    beep(350, 120);
}

void Audio::heartEyes() {
    happy();
}

// ── Radar ────────────────────────────────────────────
void Audio::radarOn() {
    beep(800, 50);
    delay(80);
    beep(1000, 50);
    delay(80);
    beep(1200, 50);
}

void Audio::radarOff() {
    beep(1200, 50);
    delay(80);
    beep(800, 50);
}

void Audio::radarPing() {
    beep(1000, 30);
    delay(50);
    beep(1200, 30);
    delay(50);
    beep(1400, 30);
}

void Audio::radarAlert() {
    enraged();
}