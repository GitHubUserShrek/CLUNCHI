#pragma once
#include <Arduino.h>


#if defined(BOARD_XIAO_C5)

  //OLED
  #define PIN_I2C_SDA   24
  #define PIN_I2C_SCL   23

  //Speaker
  #define PIN_SPEAKER    7

  //Touch / Button
  #define PIN_TOUCH      1

  //SD Card
  #define SD_CS         12
  #define SD_SCK         8
  #define SD_MISO        9
  #define SD_MOSI       10

  //GPS
  #define GPS_RX_PIN    25    // MCU RX -> GPS TX
  #define GPS_TX_PIN     0    // MCU TX -> GPS RX

  //Tilt
  #define TILT_PIN      11

  #define BAT_VOLT_PIN 6
  #define BAT_VOLT_PIN_EN 26

#elif defined(BOARD_C3_MINI)

  //OLED
  #define PIN_I2C_SDA   20
  #define PIN_I2C_SCL   21

  //Speaker
  #define PIN_SPEAKER    2

  //Touch / Button
  #define PIN_TOUCH      1

  //SD Card
  #define SD_CS          5
  #define SD_SCK         8
  #define SD_MISO        7
  #define SD_MOSI        6

  //GPS
  #define GPS_RX_PIN     3    // MCU RX -> GPS TX
  #define GPS_TX_PIN     4    // MCU TX -> GPS RX

  //Tilt
  #define TILT_PIN      10

#else
  #error "No board defined! Add -DBOARD_XIAO_C5 or -DBOARD_C3_MINI to build_flags in platformio.ini"
#endif


#if defined(BOARD_XIAO_C5)
  #define BLE_INIT_AT_BOOT        true
  #define BLE_KEEP_STACK_ALIVE    true

  #define WIFI_SET_LEGACY_PROTOCOL  false
  #define WIFI_KEEP_RADIO_ALIVE     true

#elif defined(BOARD_C3_MINI)
  #define BLE_INIT_AT_BOOT        false
  #define BLE_KEEP_STACK_ALIVE    false

  #define WIFI_SET_LEGACY_PROTOCOL  true
  #define WIFI_KEEP_RADIO_ALIVE     false
#endif


//OLED Settings
#define I2C_FREQ        400000UL
#define OLED_ADDR       0x3C
#define OLED_WIDTH      128
#define OLED_HEIGHT     64
#define FPS_TARGET      30


//Speaker Settings
#define TONE_CHANNEL    0
#define DEFAULT_VOLUME  128     // 0-255

//Touch / Button Settings
#define LONG_TOUCH_TIME     1200
#define TAP_SEQUENCE_TIME   3000

//GPS Settings
#define GPS_BAUD        9600

//WiFi Settings
#define WIFI_ENABLED            true
#define WIFI_SCAN_DURATION      10000
#define WIFI_CONNECT_TIMEOUT    20000
#define WIFI_AP_NAME            "CLUNCHI_Setup"
#define WIFI_AP_PASS            "clunchi123"

//BLE Settings
#define BLE_SCAN_DURATION       10000