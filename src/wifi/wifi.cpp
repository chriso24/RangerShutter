#include "wifi.h"
#include "Arduino.h"
#include <WiFi.h>
#include <ESPmDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include <functional>



void Wifi::Init(wifiShutdownCallback callBackShutdown)
{
    this->callBackOnUpdate = callBackShutdown;
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, password);
    while (WiFi.waitForConnectResult() != WL_CONNECTED)
    {
        Serial.println("Connection Failed! Rebooting...");
        delay(1000);
        ESP.restart();
    }

    ArduinoOTA.setPassword(password);

    ArduinoOTA
        .onStart([this]()
                 {
Serial.println("Wifi update starting, shuting down system."); 
            if (callBackOnUpdate != NULL)
               callBackOnUpdate(); 
               Serial.println("Done."); 

      String type;
      if (ArduinoOTA.getCommand() == U_FLASH)
        type = "sketch";
      else // U_SPIFFS
        type = "filesystem";

      // NOTE: if updating SPIFFS this would be the place to unmount SPIFFS using SPIFFS.end()
      Serial.println("Start updating " + type); })
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

    Serial.println("Ready");
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
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
            10000,              /* Stack size in words */
            this,               /* Task input parameter */
            1,                  /* Priority of the task */
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
    Serial.println("\nWifi listening");
    TickType_t currentTick = xTaskGetTickCount();
    for(;;)
    {
        ArduinoOTA.handle();
        vTaskDelay(100);
        currentTick = xTaskGetTickCount();

        if (currentTick > shutdownWifiAt)
            vTaskDelete(Task1);
    }
    Serial.println("\nWifi shutdown");
}