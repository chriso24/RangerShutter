#include "button.h"
#include "Arduino.h"

void Button::Init()
{
    for (size_t i = 0; i < 3; i++)
    {
        previousTouchValues[i] = 100;
    }
}

void Button::Loop()
{
    touch_value_t touch = touchRead(T0);
    touch_value_t averageTouch = previousTouchValues[0];

    for (int i = 0; i < WINDOW_SIZE-1; i++)
    {
        previousTouchValues[i] = previousTouchValues[i + 1];
        averageTouch = averageTouch + previousTouchValues[i];
    }
    previousTouchValues[WINDOW_SIZE-1] = touch;
    //Serial.print("Count = ") ; Serial.println(averageTouch );
    averageTouch = (averageTouch + touch) / 4.0;

    //Serial.print("Touch = ") ; Serial.println(touch );
    //Serial.print("Avera = ") ; Serial.println(averageTouch );

   
    //averageTouch = touch;
    //Serial.print("Avera = ") ; Serial.println(averageTouch );

      bool isPressed = averageTouch < longRunningAvg;
     bool isChange = buttonDown != isPressed;
     cyclesSinceAverageTake++;

     if(!isPressed)
     {
         if (cyclesSinceAverageTake > 100)
         {
            cyclesSinceAverageTake = 0;
            longRunningAvg = ((longRunningAvg + averageTouch)/2)* 0.8;


            //Serial.print("Updating avergae. New average = ") ; Serial.println(longRunningAvg );
            if (longRunningAvg > 15)
            {
                longRunningAvg = 15;
            }
            //Serial.print("final longRunningAvg = ") ; Serial.println(longRunningAvg );
         }
     }

    // debounce
    if ((isChange && (timeOfLastStateChange + eventDebounceTime) < xTaskGetTickCount()))
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
            if (holdTime < releaseWithin && holdTime > holdForAtleast)
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
                Serial.print("Button press rejected (");
                Serial.print(holdTime);
                Serial.println(")");
            }
        }
        buttonDown = isPressed;
    }
    else if (isChange)
    {
        Serial.print("Button press rejected debounce (");
        Serial.print((timeOfLastStateChange + eventDebounceTime) - xTaskGetTickCount());
        Serial.println(")");
    }
    else if (isPressed)
    {
        Serial.print(averageTouch);
        Serial.print("|");
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
