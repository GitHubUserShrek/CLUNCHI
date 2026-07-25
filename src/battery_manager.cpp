#include "battery_manager.h"
#include "config.h"

#if defined(BOARD_XIAO_C5)
static float _lastVoltage = 0.0f;
static uint32_t _lastCheck = 0;

void batteryBegin() {
    pinMode(BAT_VOLT_PIN, INPUT);
    pinMode(BAT_VOLT_PIN_EN, OUTPUT);
    digitalWrite(BAT_VOLT_PIN_EN, HIGH); 
    Serial.println("[Battery] ADC Initialized & Enabled on GPIO6/26");
}

void batteryUpdate() {
    if (millis() - _lastCheck < 3000) return;
    _lastCheck = millis();
    
    uint32_t Vbatt = 0;
    for(int i = 0; i < 16; i++) {
        Vbatt += analogReadMilliVolts(BAT_VOLT_PIN);
    }
    
    _lastVoltage = (2.0f * (float)Vbatt / 16.0f) / 1000.0f;
    
    Serial.printf("[Battery] Voltage: %.3f V | Raw ADC Avg: %lu mV\n", 
                  _lastVoltage, (unsigned long)(Vbatt / 16));
}

float getBatteryVoltage() { return _lastVoltage; }

int getBatteryPercentage() {
    float pct = (_lastVoltage - 3.3f) / (4.2f - 3.3f) * 100.0f;
    return constrain((int)pct, 0, 100);
}
#else
void batteryBegin() {}
void batteryUpdate() {}
float getBatteryVoltage() { return 0.0f; }
int getBatteryPercentage() { return 0; }
#endif