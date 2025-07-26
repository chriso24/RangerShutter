
#include <Arduino.h>
#include <Wire.h>
#include <WiFi.h>
#include <ESPmDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>

#include "motor/motor.h"
#include "button/button.h"
#include "current/current.h"
#include "wifi/wifi.h"
#include "orchestrator/orch.h"

const char* ssid = "Wiffy5G";
const char* password = "29232681";

Current currentMonitor;
Motor motorController;
Button button;
Orch orchestrator;

void WifiUpdateStarting() {
    // TODO: Shutdown everything
    Serial.println("Stop motor.");
}

void setup() {
    Serial.begin(115200);

    button.Init();
    currentMonitor.Init();
    motorController.Init(&currentMonitor);
    orchestrator.Init(&motorController, &currentMonitor);

    Serial.println("\nTriton management starting");
}

void loop() {
    button.Loop();
    motorController.Loop();

    if (button.ButtonPressed()) {
        orchestrator.StartMovement();
    } else if (button.ButtonLongPressed()) {
        orchestrator.Reset();
    }

    vTaskDelay(80);
}


