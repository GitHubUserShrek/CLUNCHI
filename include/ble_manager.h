#pragma once
#include <Arduino.h>

struct BLEResult {
    String  address;
    String  name;
    int     rssi;
    String  manufacturer;
    String  deviceType;
    bool    isAlert;          
    bool    isMeshtastic;     
    bool    isNewMeshtastic; 
    String  alertLabel;
    bool    isPublicAddr;
    bool    isKnown;
};

extern BLEResult bleResults[40];
extern int       bleCount;
extern bool      bleScanActive;
extern uint32_t  bleAlertsLoggedTotal;

void bleBegin();
void bleDeinit();
bool isBleInitialised();
void bleStartScan();
void bleStopScan();
void bleCancelScan();
uint32_t bleScanStartTime();

void bleStartRadar();
void bleStopRadar();
void bleForceSweep();
void bleUpdate();

int  bleAlertCount();
bool bleHasAlerts();

void bleGetSortedIndices(int* idx, int count);
void blePrintInfo();
void blePrintAlerts();