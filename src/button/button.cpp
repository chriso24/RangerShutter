#include "button.h"
#include "Arduino.h"
#include <driver/touch_pad.h>
#include <esp_sleep.h>

TickType_t timeForNextSleep = 0;

static void IRAM_ATTR TouchCallback()
{
    // This function is called when the touchpad is touched
    // You can add code here to handle the touch event
 Serial.println("Touchpad touched!");
 
}

void Button::Init()
{
    for (size_t i = 0; i < 3; i++)
    {
        previousTouchValues[i] = 50;
    }

    esp_sleep_enable_touchpad_wakeup();
    //esp_sleep_enable_ext0_wakeup(GPIO_NUM_4, 5); // 1 = High, 0 = Low

    touchAttachInterrupt(T0, TouchCallback, 40);

    
}


void Button::Loop(bool sleepMode)
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
            if (longRunningAvg > 40)
            {
                longRunningAvg = 40;
            }
            //Serial.print("final longRunningAvg = ") ; Serial.println(longRunningAvg );
         }
     }

    // debounce
    if ((isChange && (timeOfLastStateChange + eventDebounceTime) < xTaskGetTickCount()))
    {
        timeForNextSleep = xTaskGetTickCount() + pdMS_TO_TICKS(2000); // 2 seconds from now
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


    if (sleepMode && !isPressed && !isChange && timeForNextSleep < xTaskGetTickCount())
    {
        Serial.println("Entering sleep mode from button with touch value: " + String(longRunningAvg));
        //Serial.println(esp_sleep_enable_touchpad_wakeup());
        //esp_sleep_enable_timer_wakeup(5000);
        touchAttachInterrupt(T0, TouchCallback, longRunningAvg);
        esp_sleep_enable_touchpad_wakeup();
        esp_sleep_enable_timer_wakeup(random(2, 4) * 1000000); // wake up in a random time between 2 and 7 seconds for the BLE
        esp_light_sleep_start();

        if (esp_sleep_get_wakeup_cause() == ESP_SLEEP_WAKEUP_TIMER)
        {
            timeForNextSleep = xTaskGetTickCount() + pdMS_TO_TICKS(500); // 500ms wakup
        }
        else
        {
            touchDetachInterrupt(T0);
            timeForNextSleep = xTaskGetTickCount() + pdMS_TO_TICKS(10000); // 10 seconds from now
            Serial.println("Woke up from other reason");
        }
        
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
