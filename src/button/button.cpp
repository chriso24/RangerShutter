#include "button.h"
#include "Arduino.h"
#include <esp_sleep.h>

TickType_t timeForNextSleep = 0;
volatile TickType_t Button::timeOfLastStateChange = 0;
volatile TickType_t Button::timeAtButtonDown = 0;

static void IRAM_ATTR ButtonCallback()
{
    // This function is called when the button state changes
    Button::timeOfLastStateChange = xTaskGetTickCountFromISR();
}

void Button::Init()
{
    pinMode(BUTTON_PIN, INPUT_PULLUP);
    
    // Enable wakeup from deep sleep / light sleep via GPIO.
    // ext0 allows waking up on LOW state.
    esp_sleep_enable_ext0_wakeup((gpio_num_t)BUTTON_PIN, 0); // 0 = Low

    // We attach an interrupt on CHANGE to catch both press and release
    attachInterrupt(digitalPinToInterrupt(BUTTON_PIN), ButtonCallback, CHANGE);
}


void Button::Loop(bool sleepMode)
{
    bool isPressed = (digitalRead(BUTTON_PIN) == LOW);
    bool isChange = buttonDown != isPressed;

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
        // periodic action while pressed (if needed)
    }


    if (sleepMode && !isPressed && !isChange && timeForNextSleep < xTaskGetTickCount())
    {
        Serial.println("Entering sleep mode from button");
        
        esp_sleep_enable_ext0_wakeup((gpio_num_t)BUTTON_PIN, 0); // wake up on LOW
        esp_sleep_enable_timer_wakeup(random(2, 4) * 1000000); // wake up in a random time between 2 and 4 seconds for the BLE
        esp_light_sleep_start();

        if (esp_sleep_get_wakeup_cause() == ESP_SLEEP_WAKEUP_TIMER)
        {
            Serial.println("Woke up from timer");
            timeForNextSleep = xTaskGetTickCount() + pdMS_TO_TICKS(500); // 500ms wakup
        }
        else
        {
            timeForNextSleep = xTaskGetTickCount() + pdMS_TO_TICKS(20000); // 20 seconds from now
            Serial.println("Woke up from button/other reason");
        }
        
    }
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

bool Button::IsHeldFor(int ms)
{
    if (buttonDown && (xTaskGetTickCount() - timeAtButtonDown) > pdMS_TO_TICKS(ms))
    {
        return true;
    }
    return false;
}

bool Button::IsCurrentlyPressed()
{
    return buttonDown;
}
