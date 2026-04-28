#include "sd_manager.h"
#include "config.h"
#include <SPI.h>
#include <SD.h>


bool sdActive = false;

void sdBegin() {
    if (sdActive) return;
    
    SPI.begin(SD_SCK, SD_MISO, SD_MOSI, SD_CS);
    
    if (!SD.begin(SD_CS)) {
        Serial.println("[SD] Card mount failed! Check wiring or format card.");
        sdActive = false;
        return;
    }
    
    sdActive = true;
    Serial.println("[SD] Card mounted successfully.");
    
    sdPrintStatus();
}

void sdEnd() {
    if (!sdActive) return;
    SD.end();
    sdActive = false;
    Serial.println("[SD] Card unmounted.");
}

void sdUpdate() {
    
}

void sdPrintStatus() {
    Serial.println();
    Serial.println("[SD] ==========================================");
    Serial.printf("[SD]  Status:   %s\n", sdActive ? "MOUNTED" : "NOT MOUNTED");
    
    if (sdActive) {
        uint8_t cardType = SD.cardType();
        
        if (cardType == CARD_NONE) {
            Serial.println("[SD]  No SD card attached");
            return;
        }
        
        String typeStr;
        switch (cardType) {
            case CARD_MMC:  typeStr = "MMC"; break;
            case CARD_SD:   typeStr = "SD"; break;
            case CARD_SDHC: typeStr = "SDHC"; break;
            default:        typeStr = "UNKNOWN"; break;
        }
        
        Serial.printf("[SD]  Card Type: %s\n", typeStr.c_str());
        
        uint64_t total = SD.totalBytes() / (1024 * 1024);
        uint64_t used  = SD.usedBytes() / (1024 * 1024);
        
        Serial.printf("[SD]  Total:     %llu MB\n", total);
        Serial.printf("[SD]  Used:      %llu MB\n", used);
        Serial.printf("[SD]  Free:      %llu MB\n", total - used);
    }
    
    Serial.println("[SD] ==========================================");
    Serial.println();
}