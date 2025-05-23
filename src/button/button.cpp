#include "button.h"
#include "Arduino.h" 


	
    void Button::Loop()
    {
        touch_value_t touch = touchRead(T0);
        bool isPressed = touch < 11;
        bool isChange = buttonDown != isPressed;

        

        // debounce
        if((isChange && (timeOfLastStateChange + eventDebounceTime) < xTaskGetTickCount()))
        {
            Serial.println("isChange");
            timeOfLastStateChange = xTaskGetTickCount();

            if (isPressed)
            {
                timeAtButtonDown = xTaskGetTickCount();
            }
            else 
            {
                int holdTime = (xTaskGetTickCount() - timeAtButtonDown);
                if(holdTime < releaseWithin && holdTime > holdForAtleast)
                {
                    if ((xTaskGetTickCount() - timeOfLastButtonPress) > buttonDebounceAmount)
                    {
                        timeOfLastButtonPress = xTaskGetTickCount();
                        notificationRequired = true;
                    }
                }
                else if (holdTime > longPressTime)
                {
                    notificationLongRequired = true;
                    Serial.print("Long Press");
                }
                else
                {
                    Serial.print("Button press rejected (");Serial.print(holdTime);Serial.println(")");
                }
            }
            buttonDown = isPressed;
        }
        else if (isChange)
        {
Serial.print("Button press rejected debounce (");Serial.print((timeOfLastStateChange + eventDebounceTime) - xTaskGetTickCount());Serial.println(")");
        }
        else if (isPressed)
        {
Serial.print(touch);Serial.print("|");
        }

        // if (isChange && (timeSinceLastButtonPress + debounceAmount) < xTaskGetTickCount())
        // {
        //     buttonDown = isPressed;
            
        //     if (!isPressed)
        //     {
        //         notificationRequired = true;
        //         buttonUp = true;
        //     }
            
        // }
    }

    byte Button::ButtonPressed()
    {
        if (notificationRequired)
        {
            notificationRequired = 0;
            return true;
        }
        return false;
    }

     byte Button::ButtonLongPressed()
    {
        if (notificationLongRequired)
        {
            notificationLongRequired = 0;
            return true;
        }
        return false;
    }




