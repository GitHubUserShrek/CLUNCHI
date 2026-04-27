#include "audio.h"
#include <Preferences.h>

static bool _nvsLoaded = false;

void Audio::begin() {
    ledcSetup(TONE_CHANNEL, 2000, 8);
    ledcAttachPin(PIN_SPEAKER, TONE_CHANNEL);
    stop();


    Preferences prefs;
    if (prefs.begin("clunchi", true)) {
        volume_      = prefs.getUChar("volume", DEFAULT_VOLUME);
        savedVolume_ = prefs.getUChar("savedVol", DEFAULT_VOLUME);
        muted_       = prefs.getBool("muted", false);
        prefs.end();
    }
    if (muted_) volume_ = 0;
    _nvsLoaded = true;
}

void Audio::tone(int freq) {
    ledcWriteTone(TONE_CHANNEL, freq);
    ledcWrite(TONE_CHANNEL, volume_);
}

void Audio::stop() {
    ledcWrite(TONE_CHANNEL, 0);
}

void Audio::beep(int freq, uint32_t duration) {
    tone(freq);
    delay(duration);
    stop();
}

void Audio::saveSettings() {
    Preferences prefs;
    if (prefs.begin("clunchi", false)) {
        prefs.putUChar("volume", muted_ ? savedVolume_ : volume_);
        prefs.putUChar("savedVol", savedVolume_);
        prefs.putBool("muted", muted_);
        prefs.end();
        Serial.println("[Audio] Settings saved to NVS.");
    }
}

void Audio::setVolume(uint8_t vol) {
    volume_ = vol;
    if (vol > 0) muted_ = false;
}

void Audio::toggleMute() {
    if (muted_) {
        muted_ = false;
        volume_ = savedVolume_;
        Serial.println("[Audio] Unmuted");
    } else {
        savedVolume_ = volume_;
        muted_ = true;
        volume_ = 0;
        Serial.println("[Audio] Muted");
    }
}

void Audio::chirp() {
    for (int f = 2000; f < 3500; f += 100) {
        tone(f);
        delay(8);
    }
    stop();
}

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