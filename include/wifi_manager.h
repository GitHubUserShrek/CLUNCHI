#pragma once
#include <Arduino.h>
#include "config.h"

// ═══════════════════════════════════════════════════════
//  wifi_manager.h — WiFi radio + connection management
//
//  OWNS: WiFi radio lifecycle, scanning, portal, connection
//  DOES NOT: call deauth/netHealth/dashboard directly
//  EXPOSES: one-shot flags for main.cpp to act on
// ═══════════════════════════════════════════════════════

enum ConnectState {
    CONN_IDLE,
    CONN_TRYING,
    CONN_SUCCESS,
    CONN_FAILED,
    CONN_NO_CREDS
};

struct APResult {
    String  ssid;
    int32_t rssi;
    uint8_t channel;
    bool    isOpen;
    bool    isSaved;
};

// ── Radio lifecycle ──────────────────────────────────
void wifiBegin();
void wifiDeinit();
void wifiUpdate();
bool isWifiInitialised();

// ── Connection ───────────────────────────────────────
void    wifiConnect(const char* ssid);
void    wifiDisconnect();
bool    wifiConnected();

// ── One-shot connection event flags ──────────────────
bool    wifiJustConnected();
bool    wifiJustDisconnected();

// ── Scanning ─────────────────────────────────────────
void     wifiStartScan();
void     wifiCancelScan();
uint32_t wifiScanStartTime();

extern APResult     scanResults[20];
extern int          scanCount;
extern bool         scanActive;
extern ConnectState connectState;
extern String       connectedSSID;

// ── Captive portal ───────────────────────────────────
void   wifiStartPortal();
void   wifiStopPortal();
void   wifiProcessPortal();
bool   wifiIsPortalActive();
bool   wifiHasPortalCredentials();
String wifiGetPortalSSID();
void   wifiClearPortalCredentials();

// ── Network info ─────────────────────────────────────
String   wifiIP();
String   wifiCurrentSSID();
String   wifiSubnetMask();
String   wifiGatewayIP();
String   wifiDNSIP();
String   wifiMACAddress();
String   wifiBSSID();
int32_t  wifiRSSI();
int      wifiConnectedChannel();
String   wifiHostname();
uint32_t wifiConnUptime();
void     wifiPrintInfo();

// ── Extended network info ────────────────────────────
float    wifiTxPower();
String   wifiUptimeFormatted();
int      wifiSignalPercent();
String   wifiSignalLabel();