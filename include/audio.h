#pragma once
#include <Arduino.h>
#include "config.h"

class Audio {
public:
    void begin();
    void stop();
    void setVolume(uint8_t vol);
    uint8_t getVolume() const { return volume_; }
    void toggleMute();
    bool isMuted() const { return muted_; }

    void beep(int freq, uint32_t duration);

    void chirp();          

    void happy();           
    void sleepy();          
    void annoyed();         
    void curious();         
    void jazzed();          
    void enraged();       
    void dead();            
    void dribble();         

    void spiralEyes();      
    void heartEyes();       

    void radarOn();         
    void radarOff();        
    void radarPing();       
    void radarAlert();      

private:
    uint8_t volume_ = DEFAULT_VOLUME;
    uint8_t savedVolume_ = DEFAULT_VOLUME;  
    bool    muted_ = false;                 
    void tone(int freq);
};