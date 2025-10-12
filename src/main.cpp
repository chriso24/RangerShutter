#include <Arduino.h>
#include <esp_sleep.h>
#include <esp_task_wdt.h>


#include "motor/motor.h"
#include "button/button.h"
#include "current/current.h"
#include "wifi/wifi.h"
#include "orchestrator/orch.h"
#include "ble/ble.h"
#include "sleep/sleep.h"

BleLogger *bleLogger = new BleLogger();
//Current *currentMonitor = new Current   (bleLogger);
//Motor *motorController = new Motor(bleLogger);
Button *button = new Button();
Preferences preferences;
Orch *orchestrator;
//Wifi wifi(&bleLogger);
Wifi *wifi;


void WifiUpdateStarting() {
    // TODO: Shutdown everything
    Serial.println("Wifi update starting, shutting down system."); 
    Serial.println("Stop motor.");
    Serial.println("Shutdown current monitor.");
    
    
    Serial.println("Done.");
}



void setupWifi()
{
    if (wifi != NULL)
    {
        delete wifi;
    }

    wifi = new Wifi(bleLogger);

    if (wifi->Init(WifiUpdateStarting))
        wifi->StartWifi();
}

void triggerFromBle(BleLogger::Command command) {
    bleLogger->LogEvent("Toggle shutter from BLE.");

    if (command == BleLogger::Command::OPEN) {
        orchestrator->StartMovement(Orch::TOGGLE);
    } else if (command == BleLogger::Command::CLOSE) {
        orchestrator->StartMovement(Orch::TOGGLE);
    } else if (command == BleLogger::Command::RESET) {
        orchestrator->Reset();
    }   else if (command == BleLogger::Command::WIFI) {
        setupWifi();
    }
}


void setup() {
    enableLoopWDT();
    //esp_task_wdt_init(10, true);
    
    //disableCore1WDT();

    // Just incase we crashed during an operation
    Motor::AllStop();
    preferences.begin("Triton", false);

    Serial.begin(115200);

    orchestrator = new Orch(bleLogger, &preferences);
    
    bleLogger->init(triggerFromBle);
    //setupWifi();

    // button.Init();
    // currentMonitor.Init();
    // motorController.Init(&currentMonitor);
    //orchestrator->Init(&motorController, &currentMonitor);

    Serial.println("\nTriton management starting");
}

unsigned long loopCountSinceSleep = 0; // Stores the last time the message was printed
const long awakeTime = 100;  // loops
TickType_t startWifiAtTime = 0;  // loops
TickType_t startBlueToothAtTime = 0;  // loops
static unsigned long lastLogTime = 0;


bool startup;

void loop() {

// if(!startup){
// esp_task_wdt_add(NULL); 
// startup = true;
// }

//     esp_task_wdt_reset();
    button->Loop();
    bleLogger->loop();
    //motorController.Loop();
    loopCountSinceSleep++;
    //unsigned long currentMillis = millis(); // Get the current time
    // if (currentMillis - previousMillis >= interval) {
    //     previousMillis = currentMillis; // Update the last printed time
    //     // Print the message
    //     //setupWifi();
    // }



    if (button->ButtonPressed()) {
    {
        orchestrator->StartMovement(Orch::TOGGLE);
        startWifiAtTime = xTaskGetTickCount() + pdMS_TO_TICKS(60000); // 60 seconds from now
        //setupWifi();
    }
    } 
    // else if (button.ButtonLongPressed()) {
    //     orchestrator->Reset();
    // }

    if (xTaskGetTickCount() > startWifiAtTime && startWifiAtTime != 0) {
        //setupWifi();
        startWifiAtTime = 0;
    }

    if (orchestrator->isIdle() && !bleLogger->isConnected() && loopCountSinceSleep > awakeTime) {
        //Serial.println("Entering light sleep");
        
        //GoToSleep(1, &bleLogger);
        // esp_sleep_enable_timer_wakeup(5000000); // 5 second
        // esp_light_sleep_start();

        // if (esp_sleep_get_touchpad_wakeup_status() != TOUCH_PAD_MAX)
        // {
        //     loopCountSinceSleep -= 10000;
        // }

        // Serial.println("Woke up from light sleep");
        loopCountSinceSleep = 0;
    }

    
    // unsigned long currentMillis = millis();

    // if (currentMillis - lastLogTime >= 1000) {
    //     lastLogTime = currentMillis;
    // bleLogger->LogEvent("Looping" + std::string(String(currentMillis).c_str()) + " ms");
    // }

    
    vTaskDelay(20);
}


