#pragma once
#include <Arduino.h>

struct DeauthEvent {
    uint8_t  source[6];
    uint8_t  destination[6];
    uint8_t  bssid[6];
    uint16_t reasonCode;
    int      rssi;
    uint32_t timestamp;
    bool     isTargeted;
    bool     isDisassoc;
};

#define DEAUTH_LOG_SIZE 10

extern DeauthEvent deauthLog[DEAUTH_LOG_SIZE];
extern int         deauthLogCount;
extern uint32_t    deauthTotalCount;
extern uint32_t    deauthLastTime;
extern bool        deauthDetectorActive;

// ── Lifecycle ────────────────────────────────────────
void deauthDetectorBegin();
void deauthDetectorEnd();
void deauthDetectorUpdate();
bool isDeauthDetectorActive();

// ── Alert queries ────────────────────────────────────
bool     deauthHasRecentAlert(uint32_t withinMs = 10000);
uint32_t deauthRecentCount(uint32_t withinMs = 10000);

// ── Threat analysis ──────────────────────────────────
uint8_t  deauthThreatScore();
bool     deauthUnderAttack();
uint8_t  deauthUniqueSourceCount();
int      deauthMostActiveSourceIndex();

// ── Debug ────────────────────────────────────────────
void deauthPrintInfo();
void deauthPrintLog();

// ── Helpers ──────────────────────────────────────────
String      deauthMacToString(const uint8_t* mac);
const char* deauthReasonString(uint16_t reason);