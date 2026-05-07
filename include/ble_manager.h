#pragma once
#include <Arduino.h>

struct BLEResult {
    String address;
    String name;
    int    rssi;
    bool   isKnown;
    String manufacturer;
    String deviceType;
    bool   isAlert;
    String alertLabel;
    String signal;
    bool   isPublicAddr;
};

extern BLEResult bleResults[40];
extern int       bleCount;
extern bool      bleScanActive;

extern uint32_t bleAlertsLoggedTotal;

void bleGetSortedIndices(int* idx, int count);

void     bleBegin();
void     bleDeinit();
void     bleUpdate();
bool     isBleInitialised();

void     bleStartScan();
void     bleStopScan();
void     bleCancelScan();
uint32_t bleScanStartTime();

int      bleAlertCount();
bool     bleHasAlerts();
void     blePrintInfo();
void     blePrintAlerts();

void     bleStartRadar();
void     bleStopRadar();
void     bleForceSweep();