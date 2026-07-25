#pragma once
#include <Arduino.h>

void batteryBegin();
void batteryUpdate();
float getBatteryVoltage();
int getBatteryPercentage();