#include <Arduino.h>
#include <esp_sleep.h>


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
Wifi wifi(&bleLogger);


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


void triggerFromBle(BleLogger::Command command) {
    bleLogger.LogEvent("Toggle shutter from BLE.");

    if (command == BleLogger::Command::OPEN) {
        orchestrator.StartMovement(Orch::OPEN);
    } else if (command == BleLogger::Command::CLOSE) {
        orchestrator.StartMovement(Orch::CLOSE);
    } else if (command == BleLogger::Command::RESET) {
        orchestrator.Reset();
    }
}

void setupWifi()
{
    if (wifi.Init(WifiUpdateStarting))
        wifi.StartWifi();
}

void setup() {
    enableLoopWDT();
    Serial.begin(115200);
    
    bleLogger.init(triggerFromBle);
    setupWifi();

    button.Init();
    //currentMonitor.Init();
    //motorController.Init(&currentMonitor);
    //orchestrator.Init(&motorController, &currentMonitor);

    Serial.println("\nTriton management starting");
}

unsigned long loopCountSinceSleep = 0; // Stores the last time the message was printed
const long awakeTime = 100;  // loops



void loop() {
    button.Loop();
    bleLogger.loop();
    motorController.Loop();
    loopCountSinceSleep++;
    //unsigned long currentMillis = millis(); // Get the current time
    // if (currentMillis - previousMillis >= interval) {
    //     previousMillis = currentMillis; // Update the last printed time
    //     // Print the message
    //     //setupWifi();
    // }

    if (button.ButtonPressed()) {
        orchestrator.StartMovement(Orch::TOGGLE);
    } else if (button.ButtonLongPressed()) {
        orchestrator.Reset();
    }

    if (orchestrator.isIdle() && !bleLogger.isConnected() && loopCountSinceSleep > awakeTime) {
        Serial.println("Entering light sleep");
        esp_sleep_enable_touchpad_wakeup();
        esp_sleep_enable_timer_wakeup(1000000); // 1 second
        esp_light_sleep_start();
        Serial.println("Woke up from light sleep");
        loopCountSinceSleep = 0;
    }

    vTaskDelay(10);
}


