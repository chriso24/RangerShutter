
#include <Arduino.h>


#include "motor/motor.h"
#include "button/button.h"
#include "current/current.h"
#include "wifi/wifi.h"
#include "orchestrator/orch.h"
#include "ble/ble.h"

BleLogger bleLogger;
Current currentMonitor(&bleLogger);
Motor motorController(&bleLogger);
Button button;
Orch orchestrator(&bleLogger);
//Wifi wifi;


void WifiUpdateStarting() {
    // TODO: Shutdown everything
    Serial.println("Wifi update starting, shutting down system."); 
    Serial.println("Stop motor.");
    motorController.Stop(true);
    Serial.println("Shutdown current monitor.");
    currentMonitor.EndMonitor();
    orchestrator.EndThread();   
    
    Serial.println("Done.");
}


void triggerFromBle() {
    // TODO: Shutdown everything
    bleLogger.LogEvent("Toggle shutter from BLE");
    orchestrator.StartMovement();
}



void setup() {
    Serial.begin(115200);
    
    bleLogger.init(triggerFromBle);
    //wifi.Init(WifiUpdateStarting);
    //wifi.StartWifi();
    button.Init();
    //currentMonitor.Init();
    //motorController.Init(&currentMonitor);
    //orchestrator.Init(&motorController, &currentMonitor);

    Serial.println("\nTriton management starting");
}

unsigned long previousMillis = 0; // Stores the last time the message was printed
const long interval = 1000;       // Interval at which to print (1000 milliseconds = 1 second)



void loop() {
    button.Loop();
    bleLogger.loop();
    motorController.Loop();
    unsigned long currentMillis = millis(); // Get the current time
    if (currentMillis - previousMillis >= interval) {
        previousMillis = currentMillis; // Update the last printed time
        // Print the message
        bleLogger.LogEvent(("Hello " + String(currentMillis)).c_str());
    }

    if (button.ButtonPressed()) {
        orchestrator.StartMovement();
    } else if (button.ButtonLongPressed()) {
        orchestrator.Reset();
    }


    vTaskDelay(80);
}


