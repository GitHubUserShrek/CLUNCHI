#include "menu.h"
#include "touch.h"
#include "display.h"
#include "wifi_manager.h"
#include "ble_manager.h"
#include "audio.h"
#include "config.h"
#include "mood.h"
#include "wardriving.h"
#include "sd_manager.h"
#include "gps_manager.h"
#include <Preferences.h>
#include <algorithm>

extern Display display;
extern Audio   audio;

static MenuMode currentMenu     = MENU_OFF;
static int      menuCursor      = 0;
static bool     menuLongHandled = true;

static bool wifiMenuScanning  = false;
static int  wifiMenuCursor    = 0;
static int  connectMenuCursor = 0;
static bool connectInProgress = false;
static int  wifiInfoPage      = 0;
static const int WIFI_INFO_PAGES = 4;

static int  bleMenuCursor   = 0;
static bool bleMenuScanning = false;

static Preferences prefs;
static int         sleepTimerCursor    = 1;
static int         sleepTimerActiveIdx = 1;

struct TimerOption { const char* label; uint32_t seconds; };
static const TimerOption sleepTimerOpts[] = {
    { "1 Minute",   60   },
    { "5 Minutes",  300  },
    { "15 Minutes", 900  },
    { "30 Minutes", 1800 },
    { "Never",      0    }
};

static char     confirmLine1[32] = "";
static char     confirmLine2[32] = "";
static MenuMode confirmReturnTo  = MENU_MAIN;

struct MenuItem { const char* name; void (*action)(); };

static void act_back();
static void act_exit();
static void act_settings();
static void act_wifi();
static void act_wifi_scan();
static void act_wifi_connect();
static void act_wifi_portal();
static void act_wifi_info();
static void act_wifi_clear_nvs();
static void act_wifi_disconnect();
static void act_wardriving();
static void act_ble();
static void act_ble_scan();
static void act_ble_radar();
static void act_volume();
static void act_vol_up();
static void act_vol_down();
static void act_mute();
static void act_sleep_timer();
static void act_reboot();

static void drawWifiScanScreen();
static void drawWifiConnectScreen();
static void drawWifiPortalScreen();
static void drawWifiInfoScreen();
static void drawBleScanScreen();
static void drawSleepTimerScreen();
static void drawConfirmScreen();

static const char*    mainItems[] = { "WiFi", "BLE", "Settings", "Exit" };
static const MenuItem mainOpts[]  = {
    { "WiFi",     act_wifi     },
    { "BLE",      act_ble      },
    { "Settings", act_settings },
    { "Exit",     act_exit     }
};

static const char*    settItems[] = { "Volume", "Sleep Timer", "Reboot", "Back" };
static const MenuItem settOpts[]  = {
    { "Volume",      act_volume      },
    { "Sleep Timer", act_sleep_timer },
    { "Reboot",      act_reboot      },
    { "Back",        act_back        }
};

static const char*    volItems[] = { "Vol +", "Vol -", "Mute/Unmute", "Back" };
static const MenuItem volOpts[]  = {
    { "Vol +",       act_vol_up   },
    { "Vol -",       act_vol_down },
    { "Mute/Unmute", act_mute     },
    { "Back",        act_back     }
};

static const char*    wifiItems[] = {
    "Scan", "Connect", "Disconnect", "Net Info",
    "Setup Portal", "Clear Saved",
    "Wardriving", "Back"
};
static const MenuItem wifiOpts[] = {
    { "Scan",         act_wifi_scan       },
    { "Connect",      act_wifi_connect    },
    { "Disconnect",   act_wifi_disconnect },
    { "Net Info",     act_wifi_info       },
    { "Setup Portal", act_wifi_portal     },
    { "Clear Saved",  act_wifi_clear_nvs  },
    { "Wardriving",   act_wardriving      },
    { "Back",         act_back            }
};

static const char*    bleItems[] = { "Scan", "Radar", "Back" };
static const MenuItem bleOpts[]  = {
    { "Scan",  act_ble_scan  },
    { "Radar", act_ble_radar },
    { "Back",  act_back      }
};

static void act_back() {
    if (currentMenu == MENU_VOLUME || currentMenu == MENU_SLEEP_TIMER) {
        currentMenu = MENU_SETTINGS;
    } else {
        if (currentMenu == MENU_WIFI &&
            !wifiConnected() && !wifiIsPortalActive()) wifiDeinit();
        currentMenu = MENU_MAIN;
    }
    menuCursor      = 0;
    menuLongHandled = true;
}

static void act_exit()     { exitMenu(); audio.saveSettings(); }
static void act_reboot()   { ESP.restart(); }
static void act_settings() { currentMenu = MENU_SETTINGS;  menuCursor = 0; menuLongHandled = true; }
static void act_volume()   { currentMenu = MENU_VOLUME;    menuCursor = 0; menuLongHandled = true; }
static void act_wifi()     { wifiBegin(); currentMenu = MENU_WIFI; menuCursor = 0; menuLongHandled = true; }
static void act_ble()      { currentMenu = MENU_BLE;       menuCursor = 0; menuLongHandled = true; }

static void act_vol_up() {
    audio.setVolume(std::min(255, (int)audio.getVolume() + 32));
    audio.beep(440, 20);
}

static void act_vol_down() {
    audio.setVolume(std::max(0, (int)audio.getVolume() - 32));
    audio.beep(440, 20);
}

static void act_mute() {
    audio.toggleMute();
    if (!audio.isMuted()) audio.beep(440, 20);
}

static void act_wifi_info() {
    if (!wifiConnected()) {
        showConfirm("Not Connected", "Connect first", MENU_WIFI);
        return;
    }
    wifiInfoPage    = 0;
    currentMenu     = MENU_WIFI_INFO;
    menuLongHandled = true;
}

static void act_wifi_scan() {
    wifiStartScan();
    wifiMenuScanning = true;
    wifiMenuCursor   = 0;
    menuLongHandled  = true;
    audio.beep(1000, 30);
}

static void act_wifi_connect() {
    if (scanCount == 0) { wifiStartScan(); wifiMenuScanning = true; }
    currentMenu       = MENU_WIFI_CONNECT;
    connectMenuCursor = 0;
    connectInProgress = false;
    menuLongHandled   = true;
}

static void act_wifi_portal() {
    wifiStartPortal();
    currentMenu     = MENU_WIFI_PORTAL;
    menuLongHandled = true;
    audio.beep(800, 50);
}

static void act_wifi_clear_nvs() {
    wifiClearPortalCredentials();
    showConfirm("WiFi Cleared", "Rebooting...", MENU_OFF);
    delay(1000);
    ESP.restart();
}

static void act_wifi_disconnect() {
    wifiDisconnect();
    showConfirm("Disconnected", "", MENU_WIFI);
    menuLongHandled = true;
}

static void act_wardriving() {
    if (!sdActive) {
        display.drawConfirm("No SD Card", "Serial log only");
        delay(1500); 
    }
    exitMenu();
    triggerWardriving();
    audio.beep(1000, 50);
    delay(80);
    audio.beep(1200, 50);
}

static void act_ble_scan() {
    wifiDeinit();
    delay(500);
    bleBegin();
    delay(100);

    if (isBleInitialised()) {
        bleCount        = 0;
        bleMenuCursor   = 0;
        bleStartScan();
        bleMenuScanning = true;
        currentMenu     = MENU_BLE_SCAN;
        menuLongHandled = true;
        audio.beep(1000, 30);
    } else {
        showConfirm("BLE Init", "Failed!", MENU_BLE);
    }
}

static void act_ble_radar() {
    exitMenu();
    triggerRadar();
    menuLongHandled = true;
}

static void act_sleep_timer() {
    currentMenu = MENU_SLEEP_TIMER;
    prefs.begin("clunchi", true);
    uint32_t saved = prefs.getUInt("sleep_time", 60);
    prefs.end();
    for (int i = 0; i < 5; i++) {
        if (sleepTimerOpts[i].seconds == saved) sleepTimerActiveIdx = i;
    }
    sleepTimerCursor = sleepTimerActiveIdx;
    menuLongHandled  = true;
}


static void drawWifiInfoScreen() {
    static char l1[32], l2[32], l3[32], l4[32];
    char title[16];
    snprintf(title, 15, "NET %d/%d", wifiInfoPage + 1, WIFI_INFO_PAGES);

    switch (wifiInfoPage) {
    case 0:
        snprintf(l1, 31, "SSID: %s",  wifiCurrentSSID().c_str());
        snprintf(l2, 31, "IP: %s",    wifiIP().c_str());
        snprintf(l3, 31, "Mask: %s",  wifiSubnetMask().c_str());
        snprintf(l4, 31, "Tap:Next Hold:Back");
        break;
    case 1:
        snprintf(l1, 31, "GW: %s",    wifiGatewayIP().c_str());
        snprintf(l2, 31, "DNS: %s",   wifiDNSIP().c_str());
        snprintf(l3, 31, "Host: %s",  wifiHostname().c_str());
        snprintf(l4, 31, "Tap:Next Hold:Back");
        break;
    case 2: {
        int32_t rssi = wifiRSSI();
        snprintf(l1, 31, "RSSI: %ld dBm", (long)rssi);
        snprintf(l2, 31, "Ch: %d",         wifiConnectedChannel());
        snprintf(l3, 31, "BSSID:");
        snprintf(l4, 31, " %s",            wifiBSSID().c_str());
        break;
    }
    case 3:
        snprintf(l1, 31, "MAC: %s",    wifiMACAddress().c_str());
        { uint32_t up = wifiConnUptime();
          snprintf(l2, 31, "Up: %lum %lus",
              (unsigned long)(up / 60), (unsigned long)(up % 60)); }
        snprintf(l3, 31, "Tap:Next Hold:Back");
        l4[0] = '\0';
        break;
    }

    const char* itm[] = { l1, l2, l3, l4 };
    int count = 0;
    for (int i = 0; i < 4; i++) if (itm[i][0] != '\0') count++;
    display.drawMenu(title, itm, count, -1);
}

static void drawWifiScanScreen() {
    if (scanActive) {
        uint32_t elapsed = (millis() - wifiScanStartTime()) / 1000;
        char l[32];
        snprintf(l, 31, "Scanning... %us",
                 (unsigned int)((WIFI_SCAN_DURATION / 1000) - elapsed));
        const char* itm[] = { l, "Hold: Cancel" };
        display.drawMenu("WIFI SCAN", itm, 2, -1);
    } else if (scanCount > 0) {
        char l1[32], l2[32], l3[32];
        snprintf(l1, 31, "%d/%d: %s", wifiMenuCursor + 1, scanCount,
                 scanResults[wifiMenuCursor].ssid.c_str());
        snprintf(l2, 31, "RSSI: %d dBm",
                 (int)scanResults[wifiMenuCursor].rssi);
        snprintf(l3, 31, "Ch:%d %s",
                 scanResults[wifiMenuCursor].channel,
                 scanResults[wifiMenuCursor].isOpen ? "OPEN" : "SECURED");
        const char* itm[] = { l1, l2, l3, "Hold: Back" };
        display.drawMenu("SCAN RESULTS", itm, 4, -1);
    } else {
        const char* itm[] = { "No networks", "Hold: Back" };
        display.drawMenu("WIFI SCAN", itm, 2, -1);
    }
}

static void drawWifiConnectScreen() {
    if (connectInProgress) {
        static char l1[32], l2[32];
        if (connectState == CONN_TRYING) {
            snprintf(l1, 31, "Connecting...");
            snprintf(l2, 31, "Please wait...");
            const char* itm[] = { l1, l2 };
            display.drawMenu("WIFI", itm, 2, -1);
        } else if (connectState == CONN_SUCCESS) {
            snprintf(l1, 31, "Connected!");
            snprintf(l2, 31, "%s", wifiIP().c_str());
            const char* itm[] = { l1, l2, "Hold: Back" };
            display.drawMenu("WIFI", itm, 3, -1);
        } else {
            const char* itm[] = { "Failed!", "Hold: Back" };
            display.drawMenu("WIFI", itm, 2, -1);
        }
    } else {
        if (scanCount == 0) {
            const char* itm[] = { "Scanning..." };
            display.drawMenu("WIFI", itm, 1, -1);
        } else {
            static char       apLines[20][32];
            static const char* apPtrs[20];
            for (int i = 0; i < scanCount; i++) {
                snprintf(apLines[i], 31, "%s%s",
                    scanResults[i].isSaved ? "*" : " ",
                    scanResults[i].ssid.c_str());
                apPtrs[i] = apLines[i];
            }
            display.drawMenu("SELECT AP", apPtrs, scanCount,
                             connectMenuCursor);
        }
    }
}

static void drawWifiPortalScreen() {
    const char* itm[] = {
        "Connect:", "CLUNCHI_Setup", "192.168.4.1", "Hold: Stop"
    };
    display.drawMenu("PORTAL", itm, 4, -1);
}

static void drawWardrivingScreen() {
    char l1[32], l2[32], l3[32], l4[32];

    snprintf(l1, 31, "Status: %s", wardrivingActive ? "ACTIVE" : "OFF");
    snprintf(l2, 31, "GPS: %s",    gpsData.valid ? "LOCKED" : "SEARCHING");
    snprintf(l3, 31, "Nets: %lu",  (unsigned long)wardrivingNetworksLogged);
    snprintf(l4, 31, "SD: %s",     sdActive ? "READY" : "NO CARD");

    const char* itm[] = { l1, l2, l3, l4, "Hold: Stop+Back" };
    display.drawMenu("WARDRIVING", itm, 5, -1);
}

static void drawBleScanScreen() {
    if (bleScanActive) {
        uint32_t elapsed = (millis() - bleScanStartTime()) / 1000;
        char l1[32], l2[32];
        snprintf(l1, 31, "Scanning... %lus",
                 (unsigned int)((BLE_SCAN_DURATION / 1000) - elapsed));
        snprintf(l2, 31, "Found: %d", bleCount);
        const char* itm[] = { l1, l2, "Hold: Cancel" };
        display.drawMenu("BLE SCAN", itm, 3, -1);
    }
    else if (bleCount > 0) {
        int idx[40];
        bleGetSortedIndices(idx, bleCount);
        int sortedIdx = idx[bleMenuCursor];

        const BLEResult& r = bleResults[sortedIdx];

        char title[20];
        char l1[32], l2[32], l3[32], l4[32];

        String primary;
        if (!r.name.isEmpty()) primary = r.name;
        else if (!r.deviceType.isEmpty()) primary = r.deviceType;
        else primary = "Unknown Device";

        String manufacturer = r.manufacturer.isEmpty() ? "Unknown" : r.manufacturer;
        const char* addrType = r.isPublicAddr ? "Public" : "Private";

        snprintf(title, sizeof(title), "BLE %d/%d", bleMenuCursor + 1, bleCount);
        snprintf(l1, 31, "%s", primary.c_str());
        snprintf(l2, 31, "%s", manufacturer.c_str());
        snprintf(l3, 31, "RSSI:%d %s%s",
                 r.rssi, addrType,
                 r.isAlert ? " ALERT" : "");
        snprintf(l4, 31, "%s", r.address.c_str());

        const char* itm[] = { l1, l2, l3, l4 };
        display.drawMenu(title, itm, 4, -1);
    }
    else {
        const char* itm[] = { "No devices", "Hold: Back" };
        display.drawMenu("BLE SCAN", itm, 2, -1);
    }
}

static void drawSleepTimerScreen() {
    static const char* itm[] = {
        "1m", "5m", "15m", "30m", "Never", "Back"
    };
    display.drawMenu("SLEEP", itm, 6, sleepTimerCursor,
                     sleepTimerActiveIdx);
}

static void drawConfirmScreen() {
    display.drawConfirm(confirmLine1, confirmLine2);
}

void menuBegin() { currentMenu = MENU_OFF; }
void exitMenu()  { currentMenu = MENU_OFF; }
bool isMenuActive() { return currentMenu != MENU_OFF; }

void enterMenu() {
    currentMenu     = MENU_MAIN;
    menuCursor      = 0;
    menuLongHandled = true;
    audio.beep(800, 30);
}

void showConfirm(const char* l1, const char* l2, MenuMode ret) {
    strncpy(confirmLine1, l1,          31);
    strncpy(confirmLine2, l2 ? l2 : "", 31);
    confirmReturnTo = ret;
    currentMenu     = MENU_CONFIRM;
    menuLongHandled = true;
}

void menuUpdate() {
    if (!isMenuActive()) return;
    if (!isTouched) menuLongHandled = false;

    if (currentMenu == MENU_CONFIRM) {
        if (longTouchActive && !menuLongHandled) {
            menuLongHandled = true;
            currentMenu = confirmReturnTo;
        }
        drawConfirmScreen();
        return;
    }

    if (currentMenu == MENU_WIFI_PORTAL) {
        if (longTouchActive && !menuLongHandled) {
            menuLongHandled = true;
            wifiStopPortal();
            currentMenu = MENU_WIFI;
            menuCursor  = 3;
            audio.beep(900, 50);
        }
        drawWifiPortalScreen();
        return;
    }

    if (currentMenu == MENU_BLE_SCAN) {
        bleUpdate();
        if (touchJustReleased && !touchWasLongPress && bleCount > 0) {
            bleMenuCursor = (bleMenuCursor + 1) % bleCount;
            audio.beep(600, 20);
        }
        if (longTouchActive && !menuLongHandled) {
            menuLongHandled = true;
            bleDeinit();
            wifiBegin();
            currentMenu = MENU_BLE;
            menuCursor  = 0;
            audio.beep(900, 50);
        }
        drawBleScanScreen();
        return;
    }

    if (currentMenu == MENU_WIFI && wifiMenuScanning) {
        wifiUpdate();
        if (touchJustReleased && !touchWasLongPress &&
            !scanActive && scanCount > 0) {
            wifiMenuCursor = (wifiMenuCursor + 1) % scanCount;
            audio.beep(600, 20);
        }
        if (longTouchActive && !menuLongHandled) {
            menuLongHandled  = true;
            wifiMenuScanning = false;
            if (scanActive) wifiCancelScan();
            audio.beep(900, 50);
        }
        drawWifiScanScreen();
        return;
    }

    if (currentMenu == MENU_WIFI_CONNECT) {
        wifiUpdate();
        wifiMenuScanning = false;
        if (connectInProgress) {
            if (longTouchActive && !menuLongHandled) {
                menuLongHandled = true;
                if (connectState == CONN_SUCCESS) {
                    connectInProgress = false;
                    exitMenu();
                    audio.beep(1200, 50);
                } else {
                    connectInProgress = false;
                    currentMenu = MENU_WIFI;
                    menuCursor  = 1;
                    audio.beep(400, 50);
                }
            }
        } else {
            if (touchJustReleased && !touchWasLongPress && scanCount > 0) {
                connectMenuCursor = (connectMenuCursor + 1) % scanCount;
                audio.beep(600, 20);
            }
            if (longTouchActive && !menuLongHandled) {
                menuLongHandled   = true;
                connectInProgress = true;
                wifiConnect(scanResults[connectMenuCursor].ssid.c_str());
                audio.beep(1000, 30);
            }
        }
        drawWifiConnectScreen();
        return;
    }

    if (currentMenu == MENU_WIFI_INFO) {
        if (!wifiConnected()) {
            currentMenu = MENU_WIFI;
            menuCursor  = 2;
            return;
        }
        if (touchJustReleased && !touchWasLongPress) {
            wifiInfoPage = (wifiInfoPage + 1) % WIFI_INFO_PAGES;
            audio.beep(600, 20);
        }
        if (longTouchActive && !menuLongHandled) {
            menuLongHandled = true;
            currentMenu     = MENU_WIFI;
            menuCursor      = 2;
            audio.beep(900, 50);
        }
        drawWifiInfoScreen();
        return;
    }

    if (currentMenu == MENU_SLEEP_TIMER) {
        if (touchJustReleased && !touchWasLongPress) {
            sleepTimerCursor = (sleepTimerCursor + 1) % 6;
            audio.beep(600, 20);
        }
        if (longTouchActive && !menuLongHandled) {
            menuLongHandled = true;
            if (sleepTimerCursor < 5) {
                prefs.begin("clunchi", false);
                prefs.putUInt("sleep_time",
                              sleepTimerOpts[sleepTimerCursor].seconds);
                prefs.end();
                sleepTimerActiveIdx = sleepTimerCursor;
                audio.beep(1200, 40);
            } else {
                currentMenu = MENU_SETTINGS;
            }
        }
        drawSleepTimerScreen();
        return;
    }

    const char**    items = nullptr;
    const MenuItem* opts  = nullptr;
    int             size  = 0;

    switch (currentMenu) {
    case MENU_MAIN:     items = mainItems; opts = mainOpts; size = 4; break;
    case MENU_SETTINGS: items = settItems; opts = settOpts; size = 4; break;
    case MENU_WIFI:     items = wifiItems; opts = wifiOpts; size = 8; break;
    case MENU_BLE:      items = bleItems;  opts = bleOpts;  size = 3; break;
    case MENU_VOLUME:   items = volItems;  opts = volOpts;  size = 4; break;
    default: return;
    }

    if (touchJustPressed) {
        menuLongHandled = false;
        audio.beep(400, 15);
    }
    if (touchJustReleased && !touchWasLongPress) {
        menuCursor = (menuCursor + 1) % size;
        audio.beep(600, 20);
    }
    if (longTouchActive && !menuLongHandled) {
        menuLongHandled = true;
        if (opts && opts[menuCursor].action) opts[menuCursor].action();
    }

    display.drawMenu(
        currentMenu == MENU_MAIN ? "MENU" : "SUBMENU",
        items, size, menuCursor);
}