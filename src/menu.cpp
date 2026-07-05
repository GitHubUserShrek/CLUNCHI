#include "menu.h"
#include "touch.h"
#include "display.h"
#include "wifi_manager.h"
#include "ble_manager.h"
#include "audio.h"
#include "config.h"
#include "mood.h"
#include "wardriving.h"
#include "alpr_detector.h"
#include "sd_manager.h"
#include "gps_manager.h"
#include "tilt.h"
#include <Preferences.h>
#include <algorithm>

extern Display display;
extern Audio audio;

static MenuMode currentMenu = MENU_OFF;
static int menuCursor = 0;
static bool menuLongHandled = true;

static bool wifiMenuScanning = false;
static int wifiMenuCursor = 0;
static int connectMenuCursor = 0;
static bool connectInProgress = false;
static int wifiInfoPage = 0;
static const int WIFI_INFO_PAGES = 4;

static int bleMenuCursor = 0;
static bool bleMenuScanning = false;

static int gpsMenuCursor = 0;
static int gpsSatPage = 0;
static int gpsSpeedUnit = 0;
static MenuMode returnMenuFromSpeed = MENU_GPS;

static int settTimezone = 0;
static Preferences prefs;
static int sleepTimerCursor = 1;
static int sleepTimerActiveIdx = 1;

static char confirmLine1[32] = "";
static char confirmLine2[32] = "";
static MenuMode confirmReturnTo = MENU_MAIN;

struct TimerOption
{
    const char *label;
    uint32_t seconds;
};
static const TimerOption sleepTimerOpts[] = {
    {"1 Minute", 60},
    {"5 Minutes", 300},
    {"15 Minutes", 900},
    {"30 Minutes", 1800},
    {"Never", 0}
};

struct MenuItem
{
    const char *name;
    void (*action)();
};

static void act_back();
static void act_exit();
static void act_reboot();
static void act_settings();

static void act_wifi();
static void act_ble();
static void act_gps();
static void act_games();

static void act_wifi_scan();
static void act_wifi_connect();
static void act_wifi_portal();
static void act_wifi_info();
static void act_wifi_clear_nvs();
static void act_wifi_disconnect();
static void act_wardriving();
#if defined(BOARD_XIAO_C5)
static void act_alpr_hunter();
#endif

static void act_ble_scan();
static void act_ble_radar();
static void act_ble_finder();
static void act_finder_scan();
static void act_finder_select();
static void act_finder_track();

static void act_gps_status();
static void act_gps_speed();
static void act_gps_clock();
static void act_gps_sat_info();
static void act_gps_toggle();

static void act_volume();
static void act_vol_up();
static void act_vol_down();
static void act_mute();

static void act_dice();
static void act_magic_8ball();
static void act_blackjack();

static void act_sleep_timer();
static void act_timezone();
static void act_dst_toggle();
static void act_tilt_toggle();

static void drawWifiScanScreen();
static void drawWifiConnectScreen();
static void drawWifiPortalScreen();
static void drawWifiInfoScreen();
static void drawBleScanScreen();
static void drawSleepTimerScreen();
static void drawTimezoneScreen();
static void drawConfirmScreen();
static void drawGpsStatusScreen();
static void drawGpsSpeedScreen();
static void drawGpsClockScreen();
static void drawGpsSatInfoScreen();
static void drawFinderScanScreen();
static void drawFinderSelectScreen();
static void drawFinderTrackScreen();

static const char *mainItems[] = {"WiFi", "BLE", "GPS", "Games", "Settings", "Exit"};
static const MenuItem mainOpts[] = {
    {"WiFi", act_wifi},
    {"BLE", act_ble},
    {"GPS", act_gps},
    {"Games", act_games},
    {"Settings", act_settings},
    {"Exit", act_exit}
};

static const char *settItems[] = {"Volume", "Sleep Timer", "Clock", "Timezone",
                                    "DST On/Off", "Tilt Sensor", "Reboot", "Back"};
static const MenuItem settOpts[] = {
    {"Volume", act_volume},
    {"Sleep Timer", act_sleep_timer},
    {"Clock", act_gps_clock},
    {"Timezone", act_timezone},
    {"DST On/Off", act_dst_toggle},
    {"Tilt Sensor", act_tilt_toggle},
    {"Reboot", act_reboot},
    {"Back", act_back}
};

static const char *volItems[] = {"Vol +", "Vol -", "Mute/Unmute", "Back"};
static const MenuItem volOpts[] = {
    {"Vol +", act_vol_up},
    {"Vol -", act_vol_down},
    {"Mute/Unmute", act_mute},
    {"Back", act_back}
};

#if defined(BOARD_XIAO_C5)
static const char *wifiItems[] = {"Scan", "Connect", "Disconnect", "Net Info",
                                    "Setup Portal", "Clear Saved", "Wardriving",
                                    "ALPR Hunter", "Back"};
static const MenuItem wifiOpts[] = {
    {"Scan", act_wifi_scan},
    {"Connect", act_wifi_connect},
    {"Disconnect", act_wifi_disconnect},
    {"Net Info", act_wifi_info},
    {"Setup Portal", act_wifi_portal},
    {"Clear Saved", act_wifi_clear_nvs},
    {"Wardriving", act_wardriving},
    {"ALPR Hunter", act_alpr_hunter},
    {"Back", act_back}
};
static const int WIFI_MENU_SIZE = 9;
#else
static const char *wifiItems[] = {"Scan", "Connect", "Disconnect", "Net Info",
                                    "Setup Portal", "Clear Saved", "Wardriving",
                                    "Back"};
static const MenuItem wifiOpts[] = {
    {"Scan", act_wifi_scan},
    {"Connect", act_wifi_connect},
    {"Disconnect", act_wifi_disconnect},
    {"Net Info", act_wifi_info},
    {"Setup Portal", act_wifi_portal},
    {"Clear Saved", act_wifi_clear_nvs},
    {"Wardriving", act_wardriving},
    {"Back", act_back}
};
static const int WIFI_MENU_SIZE = 8;
#endif

static const char *bleItems[] = {"Scan", "Radar", "Find Device", "Back"};
static const MenuItem bleOpts[] = {
    {"Scan", act_ble_scan},
    {"Radar", act_ble_radar},
    {"Find Device", act_ble_finder},
    {"Back", act_back}
};

static const char *gpsItems[] = {"Status", "Speedometer", "Satellites", "Start/Stop", "Back"};
static const MenuItem gpsOpts[] = {
    {"Status", act_gps_status},
    {"Speedometer", act_gps_speed},
    {"Satellites", act_gps_sat_info},
    {"Start/Stop", act_gps_toggle},
    {"Back", act_back}
};
static const int GPS_MENU_SIZE = 5;

static const char *gamesItems[] = {"Dice Roller", "Magic 8-Ball", "Blackjack", "Back"};
static const MenuItem gamesOpts[] = {
    {"Dice Roller", act_dice},
    {"Magic 8-Ball", act_magic_8ball},
    {"Blackjack", act_blackjack},
    {"Back", act_back}
};
static const int GAMES_MENU_SIZE = 4;

static const char *finderItems[] = {"Scan", "Select", "Track", "Back"};
static const MenuItem finderOpts[] = {
    {"Scan", act_finder_scan},
    {"Select", act_finder_select},
    {"Track", act_finder_track},
    {"Back", act_back}
};
static const int FINDER_MENU_SIZE = 4;

void menuBegin() { currentMenu = MENU_OFF; }
void exitMenu() { currentMenu = MENU_OFF; }
bool isMenuActive() { return currentMenu != MENU_OFF; }

void enterMenu()
{
    currentMenu = MENU_MAIN;
    menuCursor = 0;
    menuLongHandled = true;
    audio.beep(800, 30);
}

void showConfirm(const char *l1, const char *l2, MenuMode ret)
{
    strncpy(confirmLine1, l1, 31);
    strncpy(confirmLine2, l2 ? l2 : "", 31);
    confirmReturnTo = ret;
    currentMenu = MENU_CONFIRM;
    menuLongHandled = true;
}

void openSpeedometerFromWardriving()
{
    currentMenu = MENU_GPS_SPEED;
    returnMenuFromSpeed = MENU_OFF;
    gpsSpeedUnit = 0;
    menuLongHandled = true;
}

static void act_back()
{
    if (currentMenu == MENU_VOLUME || currentMenu == MENU_SLEEP_TIMER || currentMenu == MENU_TIMEZONE)
    {
        currentMenu = MENU_SETTINGS;
    }
    else if (currentMenu == MENU_GPS_STATUS || currentMenu == MENU_GPS_SPEED || currentMenu == MENU_GPS_SAT_INFO)
    {
        currentMenu = MENU_GPS;
        menuCursor = gpsMenuCursor;
    }
    else if (currentMenu == MENU_DICE || currentMenu == MENU_MAGIC_8BALL || currentMenu == MENU_BLACKJACK)
    {
        currentMenu = MENU_GAMES;
        if (currentMenu == MENU_DICE)
            menuCursor = 0;
        else if (currentMenu == MENU_MAGIC_8BALL)
            menuCursor = 1;
        else
            menuCursor = 2;
    }
    else
    {
        if (currentMenu == MENU_WIFI && !wifiConnected() && !wifiIsPortalActive())
            wifiDeinit();
        currentMenu = MENU_MAIN;
    }
    menuCursor = 0;
    menuLongHandled = true;
}

static void act_exit()
{
    exitMenu();
    audio.saveSettings();
}

static void act_reboot() { ESP.restart(); }

static void act_settings()
{
    currentMenu = MENU_SETTINGS;
    menuCursor = 0;
    menuLongHandled = true;
    audio.beep(900, 20);
}

static void act_wifi()
{
    wifiBegin();
    currentMenu = MENU_WIFI;
    menuCursor = 0;
    menuLongHandled = true;
    audio.beep(900, 20);
}

static void act_ble()
{
    currentMenu = MENU_BLE;
    menuCursor = 0;
    menuLongHandled = true;
    audio.beep(900, 20);
}

static void act_games()
{
    currentMenu = MENU_GAMES;
    menuCursor = 0;
    menuLongHandled = true;
    audio.beep(900, 20);
}

static void act_gps()
{
    if (!gpsActive) gpsBegin();
    currentMenu = MENU_GPS;
    menuCursor = 0;
    gpsMenuCursor = 0;
    menuLongHandled = true;
    audio.beep(900, 20);
}

static void act_gps_status()
{
    currentMenu = MENU_GPS_STATUS;
    menuLongHandled = true;
}

static void act_gps_speed()
{
    currentMenu = MENU_GPS_SPEED;
    returnMenuFromSpeed = MENU_GPS;
    gpsSpeedUnit = 0;
    menuLongHandled = true;
}

static void act_gps_clock()
{
    if (!gpsActive) gpsBegin();
    currentMenu = MENU_GPS_CLOCK;
    menuLongHandled = true;
}

static void act_gps_sat_info()
{
    gpsSatPage = 0;
    currentMenu = MENU_GPS_SAT_INFO;
    menuLongHandled = true;
}

static void act_gps_toggle()
{
    if (gpsActive)
    {
        gpsEnd();
        showConfirm("GPS Stopped", "", MENU_GPS);
    }
    else
    {
        gpsBegin();
        showConfirm("GPS Started", "Acquiring fix...", MENU_GPS);
    }
}

static void act_volume()
{
    currentMenu = MENU_VOLUME;
    menuCursor = 0;
    menuLongHandled = true;
}

static void act_vol_up()
{
    audio.setVolume(std::min(255, (int)audio.getVolume() + 32));
    audio.beep(440, 20);
    audio.saveSettings();
}

static void act_vol_down()
{
    audio.setVolume(std::max(0, (int)audio.getVolume() - 32));
    audio.beep(440, 20);
    audio.saveSettings();
}

static void act_mute()
{
    audio.toggleMute();
    if (!audio.isMuted()) audio.beep(440, 20);
    audio.saveSettings();
}

static void act_wifi_info()
{
    if (!wifiConnected())
    {
        showConfirm("Not Connected", "Connect first", MENU_WIFI);
        return;
    }
    wifiInfoPage = 0;
    currentMenu = MENU_WIFI_INFO;
    menuLongHandled = true;
}

static void act_wifi_scan()
{
    wifiStartScan();
    wifiMenuScanning = true;
    wifiMenuCursor = 0;
    menuLongHandled = true;
    audio.beep(1000, 30);
}

static void act_wifi_connect()
{
    if (scanCount == 0)
    {
        wifiStartScan();
        wifiMenuScanning = true;
    }
    currentMenu = MENU_WIFI_CONNECT;
    connectMenuCursor = 0;
    connectInProgress = false;
    menuLongHandled = true;
}

static void act_wifi_portal()
{
    wifiStartPortal();
    currentMenu = MENU_WIFI_PORTAL;
    menuLongHandled = true;
    audio.beep(800, 50);
}

static void act_wifi_clear_nvs()
{
    wifiClearPortalCredentials();
    showConfirm("WiFi Cleared", "Rebooting...", MENU_OFF);
    delay(1000);
    ESP.restart();
}

static void act_wifi_disconnect()
{
    wifiDisconnect();
    showConfirm("Disconnected", "", MENU_WIFI);
    menuLongHandled = true;
}

static void act_wardriving()
{
    if (!sdActive)
    {
        display.drawConfirm("No SD Card", "Serial log only");
        delay(1500);
    }
    exitMenu();
    triggerWardriving();
}

#if defined(BOARD_XIAO_C5)
static void act_alpr_hunter()
{
    if (!sdActive)
    {
        display.drawConfirm("No SD Card", "Serial log only");
        delay(1500);
    }
    exitMenu();
    triggerAlprHunter();
}
#endif

static void act_ble_scan()
{
#if !BLE_KEEP_STACK_ALIVE
    wifiDeinit();
    delay(500);
    bleBegin();
#endif
    delay(100);
    if (isBleInitialised())
    {
        bleReset(); 
        bleCount = 0;
        bleMenuCursor = 0;
        bleStartScan();
        bleMenuScanning = true;
        currentMenu = MENU_BLE_SCAN;
        menuLongHandled = true;
        audio.beep(1000, 30);
    }
    else
    {
        showConfirm("BLE Init", "Failed!", MENU_BLE);
    }
}

static void act_ble_radar()
{
    exitMenu();
    triggerRadar();
    menuLongHandled = true;
}

static void act_ble_finder()
{
#if !BLE_KEEP_STACK_ALIVE
    wifiDeinit();
    delay(500);
    bleBegin();
#endif
    delay(100);
    if (!isBleInitialised())
    {
        showConfirm("BLE Init", "Failed!", MENU_BLE);
        return;
    }
    currentMenu = MENU_BLE_FINDER;
    menuCursor = 0;
    menuLongHandled = true;
    audio.beep(900, 20);
}

static void act_finder_scan()
{
    bleCount = 0;
    bleMenuCursor = 0;
    bleStartScan();
    bleMenuScanning = true;
    currentMenu = MENU_BLE_FINDER_SCAN;
    menuLongHandled = true;
    audio.beep(1000, 30);
}

static void act_finder_select()
{
    if (bleCount == 0)
    {
        showConfirm("No devices", "Scan first!", MENU_BLE_FINDER);
        return;
    }
    bleMenuCursor = 0;
    currentMenu = MENU_BLE_FINDER_SELECT;
    menuLongHandled = true;
    audio.beep(900, 20);
}

static void act_finder_track()
{
    if (bleCount == 0)
    {
        showConfirm("No devices", "Scan first!", MENU_BLE_FINDER);
        return;
    }

    int idx[40];
    bleGetSortedIndices(idx, bleCount);
    int sortedIdx = idx[bleMenuCursor];

    bleStartRssiTracker(bleResults[sortedIdx].address);
    currentMenu = MENU_BLE_FINDER_TRACK;
    menuLongHandled = true;
    audio.beep(1200, 50);
    delay(30);
    audio.beep(1500, 50);
}

static void act_dice()
{
    display.diceReset();
    currentMenu = MENU_DICE;
    menuLongHandled = true;
}

static void act_magic_8ball()
{
    display.m8bReset();
    currentMenu = MENU_MAGIC_8BALL;
    menuLongHandled = true;
}

static void act_blackjack()
{
    display.bjReset();
    currentMenu = MENU_BLACKJACK;
    menuLongHandled = true;
}

static void act_sleep_timer()
{
    currentMenu = MENU_SLEEP_TIMER;
    prefs.begin("clunchi", true);
    uint32_t saved = prefs.getUInt("sleep_time", 60);
    prefs.end();
    for (int i = 0; i < 5; i++)
        if (sleepTimerOpts[i].seconds == saved)
            sleepTimerActiveIdx = i;
    sleepTimerCursor = sleepTimerActiveIdx;
    menuLongHandled = true;
}

static void act_timezone()
{
    settTimezone = deviceTimezone;
    currentMenu = MENU_TIMEZONE;
    menuLongHandled = true;
}

static void act_dst_toggle()
{
    deviceDST = !deviceDST;
    gpsSaveTimeSettings();
    if (deviceDST)
        showConfirm("DST: ON", "+1 hour applied", MENU_SETTINGS);
    else
        showConfirm("DST: OFF", "Standard time", MENU_SETTINGS);
    audio.beep(1000, 30);
}

static void act_tilt_toggle()
{
    tiltSetEnabled(!tiltEnabled());
    tiltSaveSettings();
    if (tiltEnabled())
        showConfirm("Tilt: ON", "Shake to dribble", MENU_SETTINGS);
    else
        showConfirm("Tilt: OFF", "", MENU_SETTINGS);
    audio.beep(1000, 30);
}

static void drawGpsStatusScreen()
{
    if (!gpsActive)
    {
        const char *itm[] = {"GPS is off", "Hold: Back"};
        display.drawMenu("GPS STATUS", itm, 2, -1);
        return;
    }
    
    char l1[32], l2[32], l3[32], l4[32];
    snprintf(l1, 31, "%s  Sats:%d", gpsData.valid ? "LOCKED" : "SEARCHING", gpsData.satellites);
    
    if (gpsData.valid)
    {
        snprintf(l2, 31, "%.5f", gpsData.latitude);
        snprintf(l3, 31, "%.5f", gpsData.longitude);
        snprintf(l4, 31, "Alt:%.0fm HDOP:%.1f", gpsData.altitude, gpsData.hdop / 10.0f);
    }
    else
    {
        snprintf(l2, 31, "Lat: ---");
        snprintf(l3, 31, "Lon: ---");
        snprintf(l4, 31, "HDOP: %.1f", gpsData.hdop / 10.0f);
    }
    
    const char *itm[] = {l1, l2, l3, l4, "Hold: Back"};
    display.drawMenu("GPS STATUS", itm, 5, -1);
}

static void drawGpsSpeedScreen()
{
    static const char *unitLabels[] = {"km/h", "mph", "kts"};
    double speed = gpsData.speed;
    if (gpsSpeedUnit == 1) speed *= 0.621371;
    if (gpsSpeedUnit == 2) speed *= 0.539957;
    display.drawSpeedometer(speed, unitLabels[gpsSpeedUnit], gpsData.valid, gpsData.satellites);
}

static void drawGpsClockScreen()
{
    if (!gpsActive)
    {
        const char *itm[] = {"GPS is off", "Hold: Back"};
        display.drawMenu("GPS CLOCK", itm, 2, -1);
        return;
    }
    
    LocalTime lt = gpsGetLocalTime();
    if (!lt.valid)
    {
        const char *itm[] = {"Waiting for", "time fix...", "Hold: Back"};
        display.drawMenu("GPS CLOCK", itm, 3, -1);
        return;
    }
    
    char timeBuf[16];
    snprintf(timeBuf, sizeof(timeBuf), "%02d:%02d:%02d", lt.hour, lt.minute, lt.second);
    
    char dateBuf[24];
    static const char *months[] = {"", "Jan", "Feb", "Mar", "Apr", "May", "Jun",
                                    "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
    if (lt.month >= 1 && lt.month <= 12)
        snprintf(dateBuf, sizeof(dateBuf), "%s %d, %d", months[lt.month], lt.day, lt.year);
    else
        snprintf(dateBuf, sizeof(dateBuf), "%02d/%02d/%04d", lt.month, lt.day, lt.year);
    
    char tzBuf[16];
    snprintf(tzBuf, sizeof(tzBuf), "%s%s", timezones[deviceTimezone].label, deviceDST ? "+DST" : "");
    
    display.drawClock(timeBuf, dateBuf, tzBuf);
}

static void formatSnrBar(char *buf, int snr)
{
    int barLen = (snr > 0) ? map(constrain(snr, 0, 50), 0, 50, 0, 10) : 0;
    buf[0] = '[';
    for (int i = 0; i < 10; i++)
        buf[i + 1] = (i < barLen) ? '|' : ' ';
    buf[11] = ']';
    buf[12] = '\0';
}

static void drawGpsSatInfoScreen()
{
    if (!gpsActive)
    {
        const char *itm[] = {"GPS is off", "Hold: Back"};
        display.drawMenu("SATELLITES", itm, 2, -1);
        return;
    }

    char l1[32], l2[32], l3[32], l4[32];

    if (gpsData.satCount == 0)
    {
        snprintf(l1, 31, "Sats in fix: %d", gpsData.satellites);
        snprintf(l2, 31, "No GSV data yet");
        snprintf(l3, 31, "HDOP: %.1f", gpsData.hdop / 10.0f);
        const char *itm[] = {l1, l2, l3, "Hold: Back"};
        display.drawMenu("SATELLITES", itm, 4, -1);
        return;
    }

    int satCount = gpsData.satCount;
    int satPages = (satCount + 1) / 2;
    int totalPages = 1 + satPages;

    if (gpsSatPage >= totalPages) gpsSatPage = totalPages - 1;
    if (gpsSatPage < 0) gpsSatPage = 0;

    char title[16];
    snprintf(title, 15, "SATS %d/%d", gpsSatPage + 1, totalPages);

    if (gpsSatPage == 0)
    {
        float h = gpsData.hdop / 10.0f;
        const char *quality;
        if (h < 1.0f)       quality = "Ideal";
        else if (h < 2.0f)  quality = "Excellent";
        else if (h < 5.0f)  quality = "Good";
        else if (h < 10.0f) quality = "Moderate";
        else if (h < 20.0f) quality = "Fair";
        else                quality = "Poor";

        snprintf(l1, 31, "Visible: %d  Trk: %d", satCount, gpsData.satsTracked);
        snprintf(l2, 31, "Fix sats: %d", gpsData.satellites);
        snprintf(l3, 31, "HDOP:%.1f  %s", h, quality);
        snprintf(l4, 31, "Tap:Details Hold:Back");
        const char *itm[] = {l1, l2, l3, l4};
        display.drawMenu(title, itm, 4, -1);
        return;
    }

    int startIdx = (gpsSatPage - 1) * 2;
    if (startIdx >= satCount)
    {
        gpsSatPage = 0;
        drawGpsSatInfoScreen();
        return;
    }

    SatInfo &s1 = gpsData.sats[startIdx];
    snprintf(l1, 31, "#%d %s EL:%d AZ:%d",
             s1.prn, gpsConstellation(s1.prn), s1.elevation, s1.azimuth);
    
    char bar[17];
    formatSnrBar(bar, s1.snr);
    snprintf(l2, 31, "SNR:%2d %s", s1.snr, bar);

    int count = 2;
    if (startIdx + 1 < satCount)
    {
        SatInfo &s2 = gpsData.sats[startIdx + 1];
        snprintf(l3, 31, "#%d %s EL:%d AZ:%d",
                 s2.prn, gpsConstellation(s2.prn), s2.elevation, s2.azimuth);
        
        char bar2[17];
        formatSnrBar(bar2, s2.snr);
        snprintf(l4, 31, "SNR:%2d %s", s2.snr, bar2);
        count = 4;
    }

    const char *itm[] = {l1, l2, l3, l4};
    display.drawMenu(title, itm, count, -1);
}

static void drawWifiInfoScreen()
{
    static char l1[32], l2[32], l3[32], l4[32];
    char title[16];
    snprintf(title, 15, "NET %d/%d", wifiInfoPage + 1, WIFI_INFO_PAGES);
    
    switch (wifiInfoPage)
    {
    case 0:
        snprintf(l1, 31, "SSID: %s", wifiCurrentSSID().c_str());
        snprintf(l2, 31, "IP: %s", wifiIP().c_str());
        snprintf(l3, 31, "Mask: %s", wifiSubnetMask().c_str());
        snprintf(l4, 31, "Tap:Next Hold:Back");
        break;
    case 1:
        snprintf(l1, 31, "GW: %s", wifiGatewayIP().c_str());
        snprintf(l2, 31, "DNS: %s", wifiDNSIP().c_str());
        snprintf(l3, 31, "Host: %s", wifiHostname().c_str());
        snprintf(l4, 31, "Tap:Next Hold:Back");
        break;
    case 2:
        snprintf(l1, 31, "RSSI: %ld dBm", (long)wifiRSSI());
        snprintf(l2, 31, "Ch: %d", wifiConnectedChannel());
        snprintf(l3, 31, "BSSID:");
        snprintf(l4, 31, " %s", wifiBSSID().c_str());
        break;
    case 3:
        snprintf(l1, 31, "MAC: %s", wifiMACAddress().c_str());
        {
            uint32_t up = wifiConnUptime();
            snprintf(l2, 31, "Up: %lum %lus", (unsigned long)(up / 60), (unsigned long)(up % 60));
        }
        snprintf(l3, 31, "Tap:Next Hold:Back");
        l4[0] = '\0';
        break;
    }
    
    const char *itm[] = {l1, l2, l3, l4};
    int count = 0;
    for (int i = 0; i < 4; i++)
        if (itm[i][0] != '\0') count++;
    display.drawMenu(title, itm, count, -1);
}

static void drawWifiScanScreen()
{
    if (scanActive)
    {
        uint32_t elapsed = (millis() - wifiScanStartTime()) / 1000;
        uint32_t totalSec = WIFI_SCAN_DURATION / 1000;
        uint32_t remaining = (elapsed < totalSec) ? (totalSec - elapsed) : 0;

        char l[32];
        snprintf(l, sizeof(l), "Scanning... %lus", (unsigned long)remaining);
        const char *itm[] = {l, "Hold: Cancel"};
        display.drawMenu("WIFI SCAN", itm, 2, -1);
        return;
    }
    
    if (scanCount == 0)
    {
        const char *itm[] = {"No networks", "Hold: Back"};
        display.drawMenu("WIFI SCAN", itm, 2, -1);
        return;
    }
    
    char l1[64], l2[64], l3[64];
    snprintf(l1, sizeof(l1), "%d/%d: %s", wifiMenuCursor + 1, scanCount,
             scanResults[wifiMenuCursor].ssid.c_str());
    snprintf(l2, sizeof(l2), "RSSI: %d dBm", (int)scanResults[wifiMenuCursor].rssi);
    snprintf(l3, sizeof(l3), "Ch:%d %s", scanResults[wifiMenuCursor].channel,
             scanResults[wifiMenuCursor].isOpen ? "OPEN" : "SECURED");
    const char *itm[] = {l1, l2, l3, "Hold: Back"};
    display.drawMenu("SCAN RESULTS", itm, 4, -1);
}

static void drawWifiConnectScreen()
{
    if (connectInProgress)
    {
        static char l1[32], l2[32];
        if (connectState == CONN_TRYING)
        {
            const char *itm[] = {"Connecting...", "Please wait..."};
            display.drawMenu("WIFI", itm, 2, -1);
        }
        else if (connectState == CONN_SUCCESS)
        {
            snprintf(l1, 31, "Connected!");
            snprintf(l2, 31, "%s", wifiIP().c_str());
            const char *itm[] = {l1, l2, "Hold: Back"};
            display.drawMenu("WIFI", itm, 3, -1);
        }
        else
        {
            const char *itm[] = {"Failed!", "Hold: Back"};
            display.drawMenu("WIFI", itm, 2, -1);
        }
        return;
    }
    
    if (scanCount == 0)
    {
        const char *itm[] = {"Scanning..."};
        display.drawMenu("WIFI", itm, 1, -1);
        return;
    }
    
    static char apLines[20][32];
    static const char *apPtrs[20];
    for (int i = 0; i < scanCount; i++)
    {
        snprintf(apLines[i], 31, "%s%s", scanResults[i].isSaved ? "*" : " ",
                 scanResults[i].ssid.c_str());
        apPtrs[i] = apLines[i];
    }
    display.drawMenu("SELECT AP", apPtrs, scanCount, connectMenuCursor);
}

static void drawWifiPortalScreen()
{
    const char *itm[] = {"Connect:", "CLUNCHI_Setup", "192.168.4.1", "Hold: Stop"};
    display.drawMenu("PORTAL", itm, 4, -1);
}

static void drawBleScanScreen()
{
    if (bleScanActive)
    {
        uint32_t elapsed = (millis() - bleScanStartTime()) / 1000;
        uint32_t totalSec = BLE_SCAN_DURATION / 1000;
        uint32_t remaining = (elapsed < totalSec) ? (totalSec - elapsed) : 0;

        char l1[32], l2[32];
        snprintf(l1, sizeof(l1), "Scanning... %lus", (unsigned long)remaining);
        snprintf(l2, sizeof(l2), "Found: %d", bleCount);
        const char *itm[] = {l1, l2, "Hold: Cancel"};
        display.drawMenu("BLE SCAN", itm, 3, -1);
        return;
    }
    
    if (scanFinished && bleCount == 0)
    {
        const char *itm[] = {"No devices", "Hold: Back"};
        display.drawMenu("BLE SCAN", itm, 2, -1);
        return;
    }
    
    int idx[40];
    bleGetSortedIndices(idx, bleCount);
    int sortedIdx = idx[bleMenuCursor];
    const BLEResult &r = bleResults[sortedIdx];
    
    char title[32];
    char l1[64], l2[64], l3[64], l4[64];
    
    String primary;
    if (!r.name.isEmpty())          primary = r.name;
    else if (!r.deviceType.isEmpty()) primary = r.deviceType;
    else                             primary = "Unknown Device";
    
    String manufacturer = r.manufacturer.isEmpty() ? "Unknown" : r.manufacturer;
    const char *addrType = r.isPublicAddr ? "Public" : "Private";

    snprintf(title, sizeof(title), "BLE %d/%d", bleMenuCursor + 1, bleCount);
    snprintf(l1, sizeof(l1), "%s", primary.c_str());
    snprintf(l2, sizeof(l2), "%s", manufacturer.c_str());
    snprintf(l3, sizeof(l3), "RSSI:%d %s%s", r.rssi, addrType, r.isAlert ? " ALERT" : "");
    snprintf(l4, sizeof(l4), "%s", r.address.c_str());
    const char *itm[] = {l1, l2, l3, l4};
    display.drawMenu(title, itm, 4, -1);
}

static void drawFinderScanScreen()
{
    if (bleScanActive)
    {
        uint32_t elapsed = (millis() - bleScanStartTime()) / 1000;
        uint32_t totalSec = BLE_SCAN_DURATION / 1000;
        uint32_t remaining = (elapsed < totalSec) ? (totalSec - elapsed) : 0;

        char l1[32], l2[32];
        snprintf(l1, sizeof(l1), "Scanning... %lus", (unsigned long)remaining);
        snprintf(l2, sizeof(l2), "Found: %d", bleCount);
        const char *itm[] = {l1, l2, "Hold: Cancel"};
        display.drawMenu("SCANNING", itm, 3, -1);
        return;
    }
    
    char l1[32];
    snprintf(l1, sizeof(l1), "Found %d devices", bleCount);
    const char *itm[] = {l1, "Scan complete", "Hold: Back"};
    display.drawMenu("SCAN DONE", itm, 3, -1);
}

static void drawFinderSelectScreen()
{
    if (bleCount == 0)
    {
        const char *itm[] = {"No devices", "Hold: Back"};
        display.drawMenu("SELECT", itm, 2, -1);
        return;
    }

    int idx[40];
    bleGetSortedIndices(idx, bleCount);
    int sortedIdx = idx[bleMenuCursor];
    const BLEResult &r = bleResults[sortedIdx];

    char title[24];
    char l1[64], l2[64], l3[64], l4[64];
    String primary = !r.name.isEmpty() ? r.name : r.deviceType;

    snprintf(title, sizeof(title), "SEL %d/%d", bleMenuCursor + 1, bleCount);
    snprintf(l1, sizeof(l1), "%s", primary.c_str());
    snprintf(l2, sizeof(l2), "%s", r.manufacturer.c_str());
    snprintf(l3, sizeof(l3), "RSSI:%d dBm", r.rssi);
    snprintf(l4, sizeof(l4), "Tap:Next Hold:Save");

    const char *itm[] = {l1, l2, l3, l4};
    display.drawMenu(title, itm, 4, -1);
}

static void drawFinderTrackScreen()
{
    display.clear();

    display.drawCentered("TRACKING", 10);

    String tail = targetTrackerMac;
    if (tail.length() > 8) tail = tail.substring(tail.length() - 8);
    display.drawCentered(tail.c_str(), 20);

    char rssiBuf[24];
    uint32_t sinceLastSeen = millis() - targetTrackerLastSeen;

    if (targetTrackerLastSeen == 0)
        strcpy(rssiBuf, "SEARCHING...");
    else if (sinceLastSeen > 15000)
        strcpy(rssiBuf, "LOST SIGNAL");
    else if (sinceLastSeen > 3000)
        snprintf(rssiBuf, sizeof(rssiBuf), "%d dBm (%lus ago)",
                 targetTrackerRssi, (unsigned long)(sinceLastSeen / 1000));
    else
        snprintf(rssiBuf, sizeof(rssiBuf), "%d dBm", targetTrackerRssi);
    display.drawCentered(rssiBuf, 32);

    int bars = getRssiTrackerBars();
    display.drawRssiBars(45, 40, bars);

    const char *distStr;
    if (bars == 5)                        distStr = "VERY CLOSE!";
    else if (bars == 4)                   distStr = "Close";
    else if (bars == 3)                   distStr = "Nearby";
    else if (bars == 2)                   distStr = "Far";
    else if (bars == 1)                   distStr = "Very Far";
    else if (targetTrackerLastSeen == 0)  distStr = "Waiting...";
    else                                   distStr = "Out of Range";
    display.drawCentered(distStr, 60);

    display.render();
}

static void drawSleepTimerScreen()
{
    static const char *itm[] = {"1m", "5m", "15m", "30m", "Never", "Back"};
    display.drawMenu("SLEEP", itm, 6, sleepTimerCursor, sleepTimerActiveIdx);
}

static void drawTimezoneScreen()
{
    const TimezoneEntry &tz = timezones[settTimezone];
    char l1[32], l2[32], l3[32];
    int offHours = tz.offset / 60;
    int offMins = abs(tz.offset) % 60;
    
    snprintf(l1, 31, "%s", tz.label);
    
    if (offMins != 0)
        snprintf(l2, 31, "UTC%+d:%02d", offHours, offMins);
    else if (tz.offset == 0)
        snprintf(l2, 31, "UTC");
    else
        snprintf(l2, 31, "UTC%+d", offHours);
    
    if (settTimezone == deviceTimezone)
        snprintf(l3, 31, "[ACTIVE]");
    else
        snprintf(l3, 31, "Hold: Select");
    
    char title[16];
    snprintf(title, 15, "TZ %d/%d", settTimezone + 1, timezoneCount);
    const char *itm[] = {l1, l2, l3, "Tap:Next Hold:Set"};
    display.drawMenu(title, itm, 4, -1);
}

static void drawConfirmScreen()
{
    display.drawConfirm(confirmLine1, confirmLine2);
}

struct GameTapState
{
    int taps;
    uint32_t lastTap;
};

static bool handleGameTaps(GameTapState &state)
{
    if (touchJustReleased && !touchWasLongPress)
    {
        state.taps++;
        state.lastTap = millis();
        audio.beep(400, 15);
    }
    
    return (state.taps > 0 && !isTouched && millis() - state.lastTap > 350);
}

static bool handleGameBack(int returnCursor)
{
    if (longTouchActive && !menuLongHandled)
    {
        menuLongHandled = true;
        currentMenu = MENU_GAMES;
        menuCursor = returnCursor;
        audio.beep(900, 50);
        return true;
    }
    return false;
}

void menuUpdate()
{
    if (!isMenuActive()) return;
    if (!isTouched) menuLongHandled = false;

    if (gpsActive) gpsUpdate();

    if (currentMenu == MENU_CONFIRM)
    {
        if (longTouchActive && !menuLongHandled)
        {
            menuLongHandled = true;
            currentMenu = confirmReturnTo;
        }
        drawConfirmScreen();
        return;
    }

    if (currentMenu == MENU_WIFI_PORTAL)
    {
        if (longTouchActive && !menuLongHandled)
        {
            menuLongHandled = true;
            wifiStopPortal();
            currentMenu = MENU_WIFI;
            menuCursor = 3;
            audio.beep(900, 50);
        }
        drawWifiPortalScreen();
        return;
    }

    if (currentMenu == MENU_BLE_SCAN)
    {
        bleUpdate();
        
        if (touchJustReleased && !touchWasLongPress && bleCount > 0)
        {
            bleMenuCursor = (bleMenuCursor + 1) % bleCount;
            audio.beep(600, 20);
        }
        
        if (longTouchActive && !menuLongHandled)
        {
            menuLongHandled = true;
            bleDeinit();
            wifiBegin();
            currentMenu = MENU_BLE;
            menuCursor = 0;
            audio.beep(900, 50);
        }
        
        drawBleScanScreen();
        return;
    }

    if (currentMenu == MENU_BLE_FINDER_SCAN)
    {
        bleUpdate();

        if (longTouchActive && !menuLongHandled)
        {
            menuLongHandled = true;
            if (bleScanActive) bleStopScan();
            currentMenu = MENU_BLE_FINDER;
            menuCursor = 0;
            audio.beep(900, 50);
        }

        drawFinderScanScreen();
        return;
    }

    if (currentMenu == MENU_BLE_FINDER_SELECT)
    {
        if (touchJustReleased && !touchWasLongPress && bleCount > 0)
        {
            bleMenuCursor = (bleMenuCursor + 1) % bleCount;
            audio.beep(600, 20);
        }

        if (longTouchActive && !menuLongHandled)
        {
            menuLongHandled = true;
            currentMenu = MENU_BLE_FINDER;
            menuCursor = 1;
            audio.beep(1200, 50);
        }

        drawFinderSelectScreen();
        return;
    }

    if (currentMenu == MENU_BLE_FINDER_TRACK)
    {
        bleRssiTrackerUpdate();

        if (longTouchActive && !menuLongHandled)
        {
            menuLongHandled = true;
            bleStopRssiTracker();
            currentMenu = MENU_BLE_FINDER;
            menuCursor = 2;
            audio.beep(900, 50);
        }

        drawFinderTrackScreen();
        return;
    }

    if (currentMenu == MENU_WIFI && wifiMenuScanning)
    {
        wifiUpdate();
        
        if (touchJustReleased && !touchWasLongPress && !scanActive && scanCount > 0)
        {
            wifiMenuCursor = (wifiMenuCursor + 1) % scanCount;
            audio.beep(600, 20);
        }
        
        if (longTouchActive && !menuLongHandled)
        {
            menuLongHandled = true;
            wifiMenuScanning = false;
            if (scanActive) wifiCancelScan();
            audio.beep(900, 50);
        }
        
        drawWifiScanScreen();
        return;
    }

    if (currentMenu == MENU_WIFI_CONNECT)
    {
        wifiUpdate();
        wifiMenuScanning = false;
        
        if (connectInProgress)
        {
            if (longTouchActive && !menuLongHandled)
            {
                menuLongHandled = true;
                if (connectState == CONN_SUCCESS)
                {
                    connectInProgress = false;
                    exitMenu();
                    audio.beep(1200, 50);
                }
                else
                {
                    connectInProgress = false;
                    currentMenu = MENU_WIFI;
                    menuCursor = 1;
                    audio.beep(400, 50);
                }
            }
        }
        else
        {
            if (touchJustReleased && !touchWasLongPress && scanCount > 0)
            {
                connectMenuCursor = (connectMenuCursor + 1) % scanCount;
                audio.beep(600, 20);
            }
            if (longTouchActive && !menuLongHandled)
            {
                menuLongHandled = true;
                connectInProgress = true;
                wifiConnect(scanResults[connectMenuCursor].ssid.c_str());
                audio.beep(1000, 30);
            }
        }
        
        drawWifiConnectScreen();
        return;
    }

    if (currentMenu == MENU_WIFI_INFO)
    {
        if (!wifiConnected())
        {
            currentMenu = MENU_WIFI;
            menuCursor = 2;
            return;
        }
        
        if (touchJustReleased && !touchWasLongPress)
        {
            wifiInfoPage = (wifiInfoPage + 1) % WIFI_INFO_PAGES;
            audio.beep(600, 20);
        }
        
        if (longTouchActive && !menuLongHandled)
        {
            menuLongHandled = true;
            currentMenu = MENU_WIFI;
            menuCursor = 2;
            audio.beep(900, 50);
        }
        
        drawWifiInfoScreen();
        return;
    }

    if (currentMenu == MENU_SLEEP_TIMER)
    {
        if (touchJustReleased && !touchWasLongPress)
        {
            sleepTimerCursor = (sleepTimerCursor + 1) % 6;
            audio.beep(600, 20);
        }
        
        if (longTouchActive && !menuLongHandled)
        {
            menuLongHandled = true;
            if (sleepTimerCursor < 5)
            {
                prefs.begin("clunchi", false);
                prefs.putUInt("sleep_time", sleepTimerOpts[sleepTimerCursor].seconds);
                prefs.end();
                sleepTimerActiveIdx = sleepTimerCursor;
                audio.beep(1200, 40);
            }
            else
            {
                currentMenu = MENU_SETTINGS;
            }
        }
        
        drawSleepTimerScreen();
        return;
    }

    if (currentMenu == MENU_TIMEZONE)
    {
        if (touchJustReleased && !touchWasLongPress)
        {
            settTimezone = (settTimezone + 1) % timezoneCount;
            audio.beep(600, 20);
        }
        
        if (longTouchActive && !menuLongHandled)
        {
            menuLongHandled = true;
            deviceTimezone = settTimezone;
            gpsSaveTimeSettings();
            showConfirm("Timezone Set", timezones[deviceTimezone].label, MENU_SETTINGS);
            audio.beep(1200, 40);
        }
        
        drawTimezoneScreen();
        return;
    }

    if (currentMenu == MENU_GPS_STATUS)
    {
        if (longTouchActive && !menuLongHandled)
        {
            menuLongHandled = true;
            currentMenu = MENU_GPS;
            menuCursor = gpsMenuCursor;
            audio.beep(900, 50);
        }
        drawGpsStatusScreen();
        return;
    }

    if (currentMenu == MENU_GPS_SPEED)
    {
        if (touchJustReleased && !touchWasLongPress)
        {
            gpsSpeedUnit = (gpsSpeedUnit + 1) % 3;
            audio.beep(600, 20);
        }
        
        if (longTouchActive && !menuLongHandled)
        {
            menuLongHandled = true;
            audio.beep(900, 50);

            currentMenu = returnMenuFromSpeed;

            if (returnMenuFromSpeed == MENU_GPS)
                menuCursor = gpsMenuCursor;
            else if (returnMenuFromSpeed == MENU_OFF)
                resumeWardrivingView();
        }
        
        drawGpsSpeedScreen();
        return;
    }

    if (currentMenu == MENU_GPS_CLOCK)
    {
        if (longTouchActive && !menuLongHandled)
        {
            menuLongHandled = true;
            currentMenu = MENU_SETTINGS;
            menuCursor = 2;
            audio.beep(900, 50);
        }
        drawGpsClockScreen();
        return;
    }

    if (currentMenu == MENU_GPS_SAT_INFO)
    {
        if (touchJustReleased && !touchWasLongPress)
        {
            int satPages = (gpsData.satCount > 0) ? (gpsData.satCount + 1) / 2 : 0;
            int totalPages = 1 + satPages;
            gpsSatPage = (gpsSatPage + 1) % totalPages;
            audio.beep(600, 20);
        }
        
        if (longTouchActive && !menuLongHandled)
        {
            menuLongHandled = true;
            currentMenu = MENU_GPS;
            menuCursor = gpsMenuCursor;
            audio.beep(900, 50);
        }
        
        drawGpsSatInfoScreen();
        return;
    }

    if (currentMenu == MENU_DICE)
    {
        static GameTapState diceState = {0, 0};
        
        if (!display.diceIsRolling())
        {
            if (handleGameTaps(diceState))
            {
                if (diceState.taps >= 2)
                    display.diceRoll();
                else
                    display.diceNext();
                diceState.taps = 0;
            }
            
            if (tiltEnabled() && tiltSingleHit())
            {
                display.diceRoll();
                diceState.taps = 0;
            }
            
            if (handleGameBack(0))
            {
                diceState.taps = 0;
                return;
            }
        }
        display.drawDiceScreen(tiltEnabled());
        return;
    }

    if (currentMenu == MENU_MAGIC_8BALL)
    {
        static GameTapState m8bState = {0, 0};
        
        if (!display.m8bIsShaking())
        {
            if (handleGameTaps(m8bState))
            {
                if (m8bState.taps >= 2) display.m8bAsk();
                m8bState.taps = 0;
            }
            
            if (tiltEnabled() && tiltSingleHit())
            {
                display.m8bAsk();
                m8bState.taps = 0;
            }
            
            if (handleGameBack(1))
            {
                m8bState.taps = 0;
                return;
            }
        }
        display.drawMagic8BallScreen(tiltEnabled());
        return;
    }

    if (currentMenu == MENU_BLACKJACK)
    {
        static GameTapState bjState = {0, 0};

        if (handleGameTaps(bjState))
        {
            if (bjState.taps >= 2)
                display.bjStand();
            else
                display.bjHit();
            bjState.taps = 0;
        }

        if (handleGameBack(2))
        {
            bjState.taps = 0;
            return;
        }

        display.drawBlackjackScreen(tiltEnabled());
        return;
    }

    const char **items = nullptr;
    const MenuItem *opts = nullptr;
    int size = 0;
    const char *title = "SUBMENU";

    switch (currentMenu)
    {
    case MENU_MAIN:
        items = mainItems; opts = mainOpts; size = 6; title = "MENU";
        break;
    case MENU_SETTINGS:
        items = settItems; opts = settOpts; size = 8; title = "SETTINGS";
        break;
    case MENU_WIFI:
        items = wifiItems; opts = wifiOpts; size = WIFI_MENU_SIZE; title = "WIFI";
        break;
    case MENU_BLE:
        items = bleItems; opts = bleOpts; size = 4; title = "BLE";
        break;
    case MENU_BLE_FINDER:
        items = finderItems; opts = finderOpts; size = FINDER_MENU_SIZE; title = "FIND DEVICE";
        break;
    case MENU_VOLUME:
        items = volItems; opts = volOpts; size = 4; title = "VOLUME";
        break;
    case MENU_GPS:
        items = gpsItems; opts = gpsOpts; size = GPS_MENU_SIZE; title = "GPS";
        break;
    case MENU_GAMES:
        items = gamesItems; opts = gamesOpts; size = GAMES_MENU_SIZE; title = "GAMES";
        break;
    default:
        return;
    }

    if (touchJustPressed)
    {
        menuLongHandled = false;
        audio.beep(400, 15);
    }
    
    if (touchJustReleased && !touchWasLongPress)
    {
        menuCursor = (menuCursor + 1) % size;
        if (currentMenu == MENU_GPS) gpsMenuCursor = menuCursor;
        audio.beep(600, 20);
    }
    
    if (longTouchActive && !menuLongHandled)
    {
        menuLongHandled = true;
        if (opts && opts[menuCursor].action)
            opts[menuCursor].action();
    }

    display.drawMenu(title, items, size, menuCursor);
}