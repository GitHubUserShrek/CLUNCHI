#pragma once
#include <Arduino.h>

// ═══════════════════════════════════════════════════════
//  menu.h — Menu UI system
//
//  OWNS: menu navigation, menu rendering, menu actions
//  READS: touch globals (raw state, not events)
//  EXPOSES: menuWantsRadar() flag for main.cpp
//
//  Internal action/draw functions are NOT in this header.
//  They live in menu.cpp only.
// ═══════════════════════════════════════════════════════

enum MenuMode {
    MENU_OFF,
    MENU_MAIN,
    MENU_WIFI,
    MENU_WIFI_CONNECT,
    MENU_WIFI_PORTAL,
    MENU_WIFI_INFO,
    MENU_BLE,
    MENU_BLE_SCAN,
    MENU_SETTINGS,
    MENU_VOLUME,
    MENU_SLEEP_TIMER,
    MENU_CONFIRM
};

void menuBegin();
void menuUpdate();
void enterMenu();
void exitMenu();
bool isMenuActive();
bool menuWantsRadar();   // true ONCE after user selects radar
void showConfirm(const char* line1, const char* line2,
                 MenuMode returnTo);