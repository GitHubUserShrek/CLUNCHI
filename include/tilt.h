#pragma once
#include <Arduino.h>

void tiltBegin();
void tiltUpdate();
bool tiltShakeDetected();
bool tiltSingleHit();
bool tiltEnabled();
void tiltSetEnabled(bool enabled);
void tiltLoadSettings();
void tiltSaveSettings();