#pragma once
#include <Arduino.h>

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
bool menuWantsRadar();  
void showConfirm(const char* line1, const char* line2,
                 MenuMode returnTo);