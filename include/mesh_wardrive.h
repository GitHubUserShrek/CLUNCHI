#pragma once
#include <Arduino.h>
#include <NimBLEDevice.h>
#include "ble_manager.h" 

void meshBegin();
void meshEnd();

bool meshProcessDevice(const NimBLEAdvertisedDevice* device, const String& name, const String& addr, bool& outIsMesh, bool& outIsNew);
void meshLogToSD(const BLEResult* bleResults, int count);
bool meshHasRecentNewNode();
int meshGetLoggedCount(); 