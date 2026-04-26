#pragma once
#include <Arduino.h>
#include "config.h"

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

void wifiBegin();
void wifiDeinit();
void wifiUpdate();
bool isWifiInitialised();

void    wifiConnect(const char* ssid);
void    wifiDisconnect();
bool    wifiConnected();

bool    wifiJustConnected();
bool    wifiJustDisconnected();

void     wifiStartScan();
void     wifiCancelScan();
uint32_t wifiScanStartTime();

extern APResult     scanResults[20];
extern int          scanCount;
extern bool         scanActive;
extern ConnectState connectState;
extern String       connectedSSID;

void   wifiStartPortal();
void   wifiStopPortal();
void   wifiProcessPortal();
bool   wifiIsPortalActive();
bool   wifiHasPortalCredentials();
String wifiGetPortalSSID();
void   wifiClearPortalCredentials();

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

float    wifiTxPower();
String   wifiUptimeFormatted();
int      wifiSignalPercent();
String   wifiSignalLabel();