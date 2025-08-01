#ifndef Wifi_h
#define Wifi_h
#include "Arduino.h" 



class Wifi {
public:
    typedef void(*wifiShutdownCallback)(void);

    void StartWifi();
    void Init(wifiShutdownCallback callBackShutdown);
    void HandleWifi();
    // void Loop(void *pvParameters);

    static void Loop(void* p_pParam);
    
private:
    const char *ssid = "Wiffy5G";
    const char *password = "29232681";
    volatile TickType_t shutdownWifiAt;
    wifiShutdownCallback callBackOnUpdate;

    TaskHandle_t Task1;    
    static const int WifiWakeTime = 1000; // seconds
};
#endif



