// sleep.c
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <sys/time.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_sleep.h"
#include "esp_log.h"
#include "../logger/ILogger.h"
#include "driver/touch_pad.h"
#include "driver/rtc_io.h"
#include "driver/touch_pad.h"
#include "driver/adc.h"
#include "esp_wifi.h"
#include "soc/rtc.h"
#include <ble/ble.h>

static const char *TAG = "Sleep";

static RTC_DATA_ATTR struct timeval sleep_enter_time;

int Wakeup(ILogger* logger)
{
    struct timeval now;
    gettimeofday(&now, NULL);
    int sleep_time_ms = (now.tv_sec - sleep_enter_time.tv_sec) * 1000 + (now.tv_usec - sleep_enter_time.tv_usec) / 1000;
    
    int cause = esp_sleep_get_wakeup_cause();
    esp_reset_reason_t reset_reason = esp_reset_reason();

    {
        char buf[128];
        snprintf(buf, sizeof(buf), "esp_reset_reason=%d  Timespent in deep sleep=%dms", reset_reason, sleep_time_ms);
        logger->LogEvent(std::string(buf));
    }

    switch (cause) {
        case ESP_SLEEP_WAKEUP_TIMER: {
            {
                char buf[128];
                snprintf(buf, sizeof(buf), "Wake up from timer. Time spent in deep sleep: %dms", sleep_time_ms);
                logger->LogEvent(std::string(buf));
            }
            break;
        }
        case ESP_SLEEP_WAKEUP_TOUCHPAD: {
            {
                char buf[128];
                snprintf(buf, sizeof(buf), "Wake up from touch on pad %d", esp_sleep_get_touchpad_wakeup_status());
                logger->LogEvent(std::string(buf));
            }
            break;
        }
        case ESP_SLEEP_WAKEUP_ULP: {
            {
                char buf[128];
                snprintf(buf, sizeof(buf), "Wake up from ULP. Time spent in deep sleep: %dms", sleep_time_ms);
                logger->LogEvent(std::string(buf));
            }
            break;
        }

        case ESP_SLEEP_WAKEUP_UNDEFINED:
        default:
            logger->LogEvent(std::string("Not a deep sleep reset"));
    }
    return cause;
}

void GoToSleep(int sleepTimeInSeconds,ILogger* logger) 
{
    /* Initialize touch pad peripheral. */
    touch_pad_init();
    /* Only support one touch channel in sleep mode. */
    touch_pad_config( TOUCH_PAD_NUM0,10);

    /* Enable touch sensor clock. Work mode is "timer trigger". */
    touch_pad_set_fsm_mode(TOUCH_FSM_MODE_TIMER);
    vTaskDelay(100 / portTICK_RATE_MS);
    /* read sleep touch pad value */
    uint16_t touch_value;
    touch_pad_read_filtered(TOUCH_PAD_NUM0, &touch_value);
    touch_pad_set_thresh(TOUCH_PAD_NUM0, touch_value * 0.1); //10%
    {
        char buf[128];
        snprintf(buf, sizeof(buf), "test init: touch pad [%d] slp %d, thresh %d", TOUCH_PAD_NUM0, touch_value, (uint32_t)(touch_value * 0.1));
        logger->LogEvent(std::string(buf));
    }

    uint32_t sleep_time_in_ms = 1000;
    uint32_t touchpad_sleep_ticks = (uint32_t)((uint64_t)sleep_time_in_ms * rtc_clk_slow_freq_get_hz() / 1000);

    uint16_t oldslp, oldmeas;
    touch_pad_get_meas_time(&oldslp, &oldmeas);
    {
        char buf[128];
        snprintf(buf, sizeof(buf), "oldslp=%d  newslp=%d oldmeas=%d", oldslp, touchpad_sleep_ticks, oldmeas);
        logger->LogEvent(std::string(buf));
    }

    touch_pad_set_meas_time(touchpad_sleep_ticks, 500);

    logger->LogEvent(std::string("Enabling touch pad wakeup"));
    esp_sleep_enable_touchpad_wakeup();

    // Enable timer wake up 
    {
        char buf[128];
        snprintf(buf, sizeof(buf), "Enabling timer wakeup, %llds", sleepTimeInSeconds);
        logger->LogEvent(std::string(buf));
    }
    esp_sleep_enable_timer_wakeup(sleepTimeInSeconds * 1000000);

    esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_PERIPH, ESP_PD_OPTION_ON);

    esp_wifi_stop();
    adc_power_off();
    logger->LogEvent(std::string("Entering deep sleep"));
    gettimeofday(&sleep_enter_time, NULL);

    esp_light_sleep_start();
}