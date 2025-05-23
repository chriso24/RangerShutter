#ifndef Button_h
#define Button_h
#include "Arduino.h" 






class Button {
public:
    void Loop();

    byte ButtonPressed();

    byte ButtonLongPressed();

private:
    // int currentSpeed;
    // int currentDirection;
    TickType_t timeOfLastStateChange;
    TickType_t timeOfLastButtonPress;
    TickType_t timeAtButtonDown;
    byte buttonDown = 0;
    byte buttonUp = 1;
    byte notificationRequired = 0;
    byte notificationLongRequired = 0;
    const int eventDebounceTime = 10;
    const int buttonDebounceAmount = 2000;

    const int releaseWithin = 3000;
    const int longPressTime = 10000;
    const int holdForAtleast = 60;

};
#endif



