
#ifndef WIFI_H
#define WIFI_H

#include "Arduino.h"
#include "../logger/ILogger.h"

class Wifi {
public:
    typedef void(*wifiShutdownCallback)(void);
    Wifi(ILogger* logger);
    void StartWifi();
    bool Init(wifiShutdownCallback callBackShutdown);
    void HandleWifi();
    static void Loop(void* p_pParam);

private:
    ILogger* logger;
    const char* ssid = "Wiffy5G";
    const char* password = "29232681";
    volatile TickType_t shutdownWifiAt;
    wifiShutdownCallback callBackOnUpdate;
    TaskHandle_t Task1;
    static const int WifiWakeTime = 1000;
};

#endif // WIFI_H



