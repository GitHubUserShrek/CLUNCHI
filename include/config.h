#pragma once
#include <Arduino.h>

//OLED
#define PIN_I2C_SDA        20
#define PIN_I2C_SCL        21
#define I2C_FREQ           400000UL  
#define OLED_ADDR          0x3C
#define OLED_WIDTH         128
#define OLED_HEIGHT        64
#define FPS_TARGET         30

//SPEAKER
#define PIN_SPEAKER        2 
#define TONE_CHANNEL       0
#define DEFAULT_VOLUME     128       // 0-255 

//Touch/Button
#define PIN_TOUCH          1  

// Timing logic (No thresholds needed for Digital)
#define LONG_TOUCH_TIME    1200      
#define TAP_SEQUENCE_TIME  3000     

//SD
#define SD_CS   5
#define SD_MOSI 6
#define SD_MISO 7
#define SD_SCK  8

//GPS
#define GPS_RX_PIN 3
#define GPS_TX_PIN 4
#define GPS_BAUD   9600

//Tilt
#define TILT_PIN 10

#define WIFI_ENABLED       true
#define WIFI_SCAN_DURATION 10000     
#define WIFI_CONNECT_TIMEOUT 20000   

#define WIFI_AP_NAME       "CLUNCHI_Setup"
#define WIFI_AP_PASS       "clunchi123"

#define BLE_SCAN_DURATION  15000     