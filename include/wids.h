#pragma once
#include <Arduino.h>

enum AttackType {
    ATTACK_NONE = 0,
    ATTACK_DEAUTH,
    ATTACK_DISASSOC,
    ATTACK_EVIL_TWIN,
    ATTACK_CTS_JAMMING,
    ATTACK_HANDSHAKE_CAPTURE,
    ATTACK_BEACON_FLOOD
};

struct ThreatEvent {
    AttackType type;
    uint8_t  source[6];
    uint8_t  destination[6];
    uint8_t  bssid[6];
    uint16_t param;        
    int      rssi;
    uint32_t timestamp;     
    uint32_t firstTimestamp; 
    uint16_t burstCount;    
    bool     isTargeted;    
    bool     isBroadcast;   
    
    double   latitude;
    double   longitude;
    bool     gpsValid;
    char     customNotes[32];
};

#define WIDS_LOG_SIZE 50 

extern ThreatEvent widsLog[WIDS_LOG_SIZE];
extern int         widsLogCount;
extern int         widsLogHead;
extern uint32_t    widsTotalCount;
extern uint32_t    widsLastTime;
extern bool        widsActive;

void widsBegin();
void widsEnd();
void widsUpdate();
bool isWidsActive();

bool     widsHasRecentAlert(uint32_t withinMs = 10000);
uint32_t widsRecentCount(uint32_t withinMs = 10000);

uint8_t  widsThreatScore();
bool     widsUnderAttack();
uint8_t  widsUniqueSourceCount();
int      widsMostActiveSourceIndex();

void widsPrintInfo();
void widsPrintLog();

String      widsMacToString(const uint8_t* mac);
const char* widsReasonString(uint16_t reason);
const char* widsAttackTypeString(AttackType type);