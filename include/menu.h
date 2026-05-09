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
    MENU_GPS,
    MENU_GPS_STATUS,
    MENU_GPS_SPEED,
    MENU_GPS_CLOCK,
    MENU_GPS_SAT_INFO,
    MENU_SETTINGS,
    MENU_VOLUME,
    MENU_SLEEP_TIMER,
    MENU_TIMEZONE,
    MENU_GAMES,
    MENU_DICE,
    MENU_CONFIRM
};

void menuBegin();
void menuUpdate();
void enterMenu();
void exitMenu();
bool isMenuActive();
void showConfirm(const char* line1, const char* line2,
                 MenuMode returnTo);