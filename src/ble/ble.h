#ifndef BLE_H
#define BLE_H

#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include <queue>
#include <string>
#include "../logger/ILogger.h"

#define SERVICE_UUID           "1e8f8e98-d58a-437a-96ac-de446ddde3d3"
#define CHARACTERISTIC_UUID_RX "5c374371-7f68-4af7-a24b-880868a65f0e"
#define CHARACTERISTIC_UUID_TX "dc66407a-798b-4412-b36a-f816ba8726d3"

class BleLogger : public ILogger {
public:
    enum Command {
        OPEN,
        CLOSE,
        RESET
    };
    typedef void(*bleActivationCallback)(Command);

    BleLogger();
    void init(bleActivationCallback callBackShutdown);
    void restartBleAdvertisment();
    void loop();
    void LogEvent(const std::string& message) override;
    bool isConnected();
    

private:
    std::string getNextLog();
    void recieveMessage(const std::string& message);
    class MyServerCallbacks;
    class MyCallbacks;

    BLEServer *pServer;
    BLECharacteristic *pCharacteristic_tx;
    BLECharacteristic *pCharacteristic_rx;
    bool deviceConnected;
    float txValue;
    std::queue<std::string> logQueue;
    bleActivationCallback callBackOnUpdate;
};

#endif // BLE_H
