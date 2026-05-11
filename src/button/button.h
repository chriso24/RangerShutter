
#ifndef BUTTON_H
#define BUTTON_H

#include "Arduino.h"

class Button {
public:
    void Loop();
    
    void Init();
    void Loop(bool sleepMode);
    byte ButtonPressed();
    byte ButtonLongPressed();
    bool IsHeldFor(int ms);
    bool IsCurrentlyPressed();
    static volatile TickType_t timeOfLastStateChange;
    static volatile TickType_t timeAtButtonDown;

private:
    static const int BUTTON_PIN = 4; // T0 maps to GPIO 4
    
    TickType_t timeOfLastButtonPress;
    byte buttonDown = 0;
    byte buttonUp = 1;
    byte notificationRequired = 0;
    byte notificationLongRequired = 0;
    const int eventDebounceTime = 10;
    const int buttonDebounceAmount = 1000;
    const int releaseWithin = 1500;
    const int longPressTime = 10000;
    const int holdForAtleast = 50;
};

#endif // BUTTON_H



