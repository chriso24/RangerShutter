/*
   control app https://x.thunkable.com/copy/e8f633a8a9a3e979025112e18446d3b4
    Video: https://wwwtube.com/watch?v=oCMOYS71NIU
    Based on Neil Kolban example for IDF: https://github.com/nkolban/esp32-snippets/blob/master/cpp_utils/tests/BLE%20Tests/SampleNotify.cpp
    Ported to Arduino ESP32 by Evandro Copercini

   Create a BLE server that, once we receive a connection, will send periodic notifications.
   The service advertises itself as: 6E400001-B5A3-F393-E0A9-E50E24DCCA9E
   Has a characteristic of: 6E400002-B5A3-F393-E0A9-E50E24DCCA9E - used for receiving data with "WRITE" 
   Has a characteristic of: 6E400003-B5A3-F393-E0A9-E50E24DCCA9E - used to send data with  "NOTIFY"

   The design of creating the BLE server is:
   1. Create a BLE Server
   2. Create a BLE Service
   3. Create a BLE Characteristic on the Service
   4. Create a BLE Descriptor on the characteristic
   5. Start the service.
   6. Start advertising.

   In this example rxValue is the data received (only accessible inside that function).
   And txValue is the data to be sent, in this example just a byte incremented every second. 
*/
#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include "ble.h"

#define SERVICE_UUID           "1e8f8e98-d58a-437a-96ac-de446ddde3d3" // UART service UUID
#define CHARACTERISTIC_UUID_RX "5c374371-7f68-4af7-a24b-880868a65f0e"
#define CHARACTERISTIC_UUID_TX "dc66407a-798b-4412-b36a-f816ba8726d3"

class BleLogger::MyServerCallbacks: public BLEServerCallbacks {
    BleLogger* pBleLogger;
public:
    MyServerCallbacks(BleLogger* logger) : pBleLogger(logger) {}
    void onConnect(BLEServer* pServer) {
        Serial.println("Device connected.");
      
      pBleLogger->startSendingMessagesAt = xTaskGetTickCount() + 2000 / portTICK_PERIOD_MS; // start sending messages after 2 second
      pBleLogger->deviceConnected = true;
      pBleLogger->lastMessageTime = xTaskGetTickCount();
    };

    void onDisconnect(BLEServer* pServer) {
        Serial.println("Device disconnected.");
      pBleLogger->deviceConnected = false;
      pBleLogger->restartBleAdvertisment();
    }
};

class BleLogger::MyCallbacks: public BLECharacteristicCallbacks {
    BleLogger* pBleLogger;
public:
    MyCallbacks(BleLogger* logger) : pBleLogger(logger) {}
    void onRead(BLECharacteristic *pCharacteristic) {

        Serial.println("Ble onRead");
    }
    void onWrite(BLECharacteristic *pCharacteristic) {
        std::string rxValue = pCharacteristic->getValue();

        Serial.println("Ble onWrite");
      if (rxValue.length() > 0) {
      pBleLogger->recieveMessage(rxValue);
    }
}
};

BleLogger::BleLogger() : pServer(nullptr), pServerCallbacks(nullptr), pRxCallbacks(nullptr), pTxCallbacks(nullptr), pCharacteristic_tx(nullptr), pCharacteristic_rx(nullptr), deviceConnected(false), txValue(0) {}

// In BleLogger class, add destructor
BleLogger::~BleLogger() {
    if (pServer) {
        BLEDevice::deinit(true); // This will clean up all BLE resources
    }
    delete pServerCallbacks;
    delete pRxCallbacks;
    delete pTxCallbacks;
}

void BleLogger::init(bleActivationCallback callBack) {

    this->callBackOnUpdate = callBack;

    // Create the BLE Device
  BLEDevice::init("Mighty"); // Give it a name

  // Create the BLE Server
  pServer = BLEDevice::createServer();
  pServerCallbacks = new MyServerCallbacks(this);
  pServer->setCallbacks(pServerCallbacks);

  // Create the BLE Service
  BLEService *pService = pServer->createService(SERVICE_UUID);

  // Create a BLE Characteristic for TX
  pCharacteristic_tx = pService->createCharacteristic(
                      CHARACTERISTIC_UUID_TX,
                      BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY
                    );
                      
  pCharacteristic_tx->addDescriptor(new BLE2902());
  pTxCallbacks = new MyCallbacks(this);
  pCharacteristic_tx->setCallbacks(pTxCallbacks);

  // Create a BLE Characteristic for RX
  pCharacteristic_rx = pService->createCharacteristic(
                                         CHARACTERISTIC_UUID_RX,
                                          BLECharacteristic::PROPERTY_WRITE
                                       );

  pRxCallbacks = new MyCallbacks(this);
  pCharacteristic_rx->setCallbacks(pRxCallbacks);  // Start the service
  pService->start();

  // Start advertising
  pServer->getAdvertising()->start();
}

void BleLogger::restartBleAdvertisment() {
    pServer->getAdvertising()->start();
}

void BleLogger::LogEvent(const std::string& message) {
    if (logQueue.size() > 100) {
        logQueue.pop(); // Remove oldest message if queue is too large
    }
    logQueue.push(message);

    Serial.println(message.c_str());
}

std::string BleLogger::getNextLog() {
    if (logQueue.empty()) {
        return "";
    }
    std::string message = logQueue.front();
    logQueue.pop();
    return message;
}

bool BleLogger::isLogMessageReady() {
    return !logQueue.empty();
}

void BleLogger::loop() {
  if (deviceConnected && isLogMessageReady() && xTaskGetTickCount() >= startSendingMessagesAt) {

    std::string logMessage = logQueue.front();

    Serial.println("Start sending logs");

    if (!logMessage.empty()) {
        pCharacteristic_tx->setValue(logMessage);
        pCharacteristic_tx->notify();
        lastMessageTime = xTaskGetTickCount();
        Serial.println("Log sent");
    }
    logQueue.pop();

    if ((xTaskGetTickCount() - lastMessageTime) > silenceTimeout) {
        LogEvent("Client disconnected due to inactivity");
        pServer->disconnect(pServer->getConnId());
        restartBleAdvertisment();
    }
  }
}

void BleLogger::recieveMessage(const std::string& message) {
    if (message == "Open") {
        LogEvent("Received open command via BLE");
        this->callBackOnUpdate(Command::OPEN);
    }
    else if (message == "Close") {
        LogEvent("Received close command via BLE");
        this->callBackOnUpdate(Command::CLOSE);
    }
    else if (message == "Reset") {
        LogEvent("Reset command via BLE");
        this->callBackOnUpdate(Command::RESET);
    }
    else if (message == "WIFI") {
        LogEvent("Enable Wifi");
        this->callBackOnUpdate(Command::WIFI);
    }
    else
        LogEvent("Received unknown command via BLE");
    
}

bool BleLogger::isConnected() {
    return deviceConnected;
}
