#pragma once
#include <Arduino.h>

extern bool wardrivingActive;
extern uint32_t wardrivingNetworksLogged;

void wardrivingBegin();
void wardrivingEnd();
void wardrivingUpdate();
void wardrivingForceScan();

bool isWardrivingActive();
void wardrivingPrintStatus();
