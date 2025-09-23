#include "wifi.h"
#include "Arduino.h"
#include <WiFi.h>
#include <ESPmDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>

Wifi::Wifi(ILogger* logger) : logger(logger), Task1(NULL) {}

bool Wifi::Init(wifiShutdownCallback callBackShutdown)
{
    if (WiFi.waitForConnectResult() == WL_CONNECTED)
    {
        // Already connected. Quit.
        return true;
    }

    this->callBackOnUpdate = callBackShutdown;
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, password);    TickType_t startTickTime = xTaskGetTickCount();

    while (WiFi.waitForConnectResult() != WL_CONNECTED &&
           (xTaskGetTickCount() - startTickTime) < pdMS_TO_TICKS(10000))
    {
        delay(500);
    }

    if (WiFi.waitForConnectResult() != WL_CONNECTED)
    {
        logger->LogEvent("Wifi connection not available.");
        return false;
    }

    ArduinoOTA.setPassword(password);

    ArduinoOTA
        .onStart([this]()
                 {
logger->LogEvent("Wifi update starting, shuting down system."); 
            if (callBackOnUpdate != NULL)
               callBackOnUpdate(); 
               logger->LogEvent("Done."); 

      String type;
      if (ArduinoOTA.getCommand() == U_FLASH)
        type = "sketch";
      else // U_SPIFFS
        type = "filesystem";

      // NOTE: if updating SPIFFS this would be the place to unmount SPIFFS using SPIFFS.end()
      logger->LogEvent("Start updating " + std::string(type.c_str())); 
    })
        .onEnd([]()
               { Serial.println("\nEnd"); })
        .onProgress([](unsigned int progress, unsigned int total)
                    { Serial.printf("Progress: %u%%\r", (progress / (total / 100))); })
        .onError([](ota_error_t error)
                 {
      Serial.printf("Error[%u]: ", error);
      if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
      else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
      else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
      else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
      else if (error == OTA_END_ERROR) Serial.println("End Failed"); });

    ArduinoOTA.begin();

    Serial.println("Wifi Ready");
    logger->LogEvent("WiFi connected with IP: " + std::string(WiFi.localIP().toString().c_str()));

    return true;
}

Wifi::~Wifi()
{
    if (Task1 != NULL)
    {
        vTaskDelete(Task1);
        Task1 = NULL;
    }
}

void Wifi::StartWifi()
{
    TickType_t currentTick = xTaskGetTickCount();
    shutdownWifiAt = currentTick + pdMS_TO_TICKS(WifiWakeTime*1000);

    if (Task1 != NULL)
    {
        eTaskState taskState = eTaskGetState(Task1);
        if (taskState != eDeleted)
        {
            return;
        }
    }
    
    xTaskCreatePinnedToCore(
            Loop, /* Function to implement the task */
            "Wifi",   /* Name of the task */
            5000,              /* Stack size in words */
            this,               /* Task input parameter */
            2,                  /* Priority of the task */
            &Task1,             /* Task handle. */
            1);                 /* Core where the task should run */
}

void Wifi::Loop(void *pvParameters)
{
    Wifi* p_pThis = static_cast<Wifi*>(pvParameters);

    p_pThis->HandleWifi();
}

void Wifi::HandleWifi()
{
    logger->LogEvent("\nWifi listening");
    for (;;)
    {
        ArduinoOTA.handle();
        vTaskDelay(pdMS_TO_TICKS(100));

        if (xTaskGetTickCount() > shutdownWifiAt)
        {
            UBaseType_t uxHighWaterMark = uxTaskGetStackHighWaterMark(NULL);
            logger->LogEvent("Wifi stack high water mark: " + std::to_string(uxHighWaterMark) + " words");
            logger->LogEvent("\nWifi shutdown");
            WiFi.disconnect(true);
            WiFi.mode(WIFI_OFF);
            Task1 = NULL; // Set to null before deleting
            vTaskDelete(NULL); // Deletes the current task
        }
    }
}