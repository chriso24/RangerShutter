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

const char *ssid = "Wiffy5G";
const char *password = "29232681";

// const int motor_R = 19;
// const int motor_L = 18;

// const int I2C_SDA = 20;
// const int I2C_SCL = 21;

Current currentMonitor;
Motor motorController;
Button button;
//Wifi wifi;
Orch orchestrator;
// TaskHandle_t Task1;
// TaskHandle_t Task2;
//TaskHandle_t Wifi;


// put function declarations here:
int myFunction(int, int);
void continuousSampling();
void MotorLoopExecute();
void setupWifi();
void MotorLoop(void *);
void WifiUpdateStarting();

// void CurrentMonitorLoop(void *parameter)
// {
//     Serial.println("\nCurrent monitor started");
//     Wire.begin();
//     current.Init();
//     for (;;)
//     {
//         current.Loop();
//         delay(1);
//         // if ((xTaskGetTickCount() % 1000) == 0)
//         //     Serial.print("WaterMark: "); Serial.println(uxTaskGetStackHighWaterMark(Task1));
//     }
// }

void setup()
{
    Serial.begin(115200);
    //wifi.Init(WifiUpdateStarting);
    //wifi.StartWifi();

    //current.Init();

    button.Init();
    currentMonitor.Init();
    motorController.Init(&currentMonitor);
    orchestrator.Init(&motorController, &currentMonitor);
    // Wire.begin(I2C_SDA, I2C_SCL);

    Serial.println("\nTriton management starting");

    // xTaskCreatePinnedToCore(
    //     CurrentMonitorLoop, /* Function to implement the task */
    //     "CurrentMonitor",   /* Name of the task */
    //     10000,              /* Stack size in words */
    //     NULL,               /* Task input parameter */
    //     2,                  /* Priority of the task */
    //     &Task1,             /* Task handle. */
    //     0);                 /* Core where the task should run */

    // xTaskCreatePinnedToCore(
    //     MotorLoop, /* Function to implement the task */
    //     "Motor",   /* Name of the task */
    //     3000,     /* Stack size in words */
    //     NULL,      /* Task input parameter */
    //     1,         /* Priority of the task */
    //     &Task2,    /* Task handle. */
    //     1);        /* Core where the task should run */

    // xTaskCreatePinnedToCore(
    //     WifiLoop, /* Function to implement the task */
    //     "Wifi",   /* Name of the task */
    //     10000,     /* Stack size in words */
    //     NULL,      /* Task input parameter */
    //     1,         /* Priority of the task */
    //     &Wifi,    /* Task handle. */
    //     1);        /* Core where the task should run */
}

void WifiUpdateStarting(){
    // TODO: Shutdown everything
    Serial.println("Stop motor.");
}

void loop()
{
    //motorController.Loop();
    button.Loop();

    if (button.ButtonPressed())
    {
        orchestrator.StartMovement();
    }
    else if (button.ButtonLongPressed())
    {
        orchestrator.Reset();
    }
    
//Serial.printf("Free: %d\tMaxAlloc: %d\t PSFree: %d\n",ESP.getFreeHeap(),ESP.getMaxAllocHeap(),ESP.getFreePsram());

    vTaskDelay(80);
}

// void MotorLoopExecute()
// {
//     motorController.Loop();
//     button.Loop();

//     if (motorController.CurrentSpeed > 0 && current.Stop(motorController.IsRamping()))
//     {
//         motorController.Stop(true);
//         return;
//     }

//     if (button.ButtonPressed())
//     {
//         if (motorController.CurrentSpeed > 0)
//         {
//             motorController.Stop(false);
//             delay(100);
//         }
//         else
//         {
//             current.Reset();
//             if (motorController.CurrentDirection == 0)
//                 motorController.Move(1);
//             else
//                 motorController.Move(0);
//         }
//     }
//     else if (motorController.CurrentSpeed == 0)
//     {
//         vTaskDelay(10);
//     }
    // for(int i=0; i<5; i++){
    //     continuousSampling();
    //     delay(3000);
    //   }

    //  Serial.println("Power down for 1s");
    //   ina226.powerDown();
    //   for(int i=0; i<1; i++){
    //     Serial.print(".");
    //     delay(1000);
    //   }

    //   Serial.println("Power up!");
    //   Serial.println("");
    //   ina226.powerUp();

    // byte error, address;
    //   int nDevices;
    //    Serial.println("Scanning...");
    //   nDevices = 0;
    //   for(address = 1; address < 127; address++ ) {
    //     Wire.beginTransmission(address);
    //     error = Wire.endTransmission();
    //     if (error == 0) {
    //       Serial.print("I2C device found at address 0x");
    //       if (address<16) {
    //         Serial.print("0");
    //       }
    //       Serial.println(address,HEX);
    //       nDevices++;
    //     }
    //     else if (error==4) {
    //       Serial.print("Unknow error at address 0x");
    //       if (address<16) {
    //         Serial.print("0");
    //       }
    //       Serial.println(address,HEX);
    //     }
    //   }
    //   if (nDevices == 0) {
    //     Serial.println("No I2C devices found\n");
    //   }
    //   else {
    //     Serial.println("done\n");
    //   }
    //    delay(1000);
//}

// void moveMotor()
// {
//     analogWrite(motor_L, 0);
// delay(500);

// for(int i=0; i<50; i++){
// analogWrite(motor_R, i);
// delay(30);
// }

// analogWrite(motor_R, 0);
// delay(500);

// for(int i=0; i<50; i++){
// analogWrite(motor_L, i);
// delay(30);
// }
// }

// void loop()
// {
//     current.ContinuousSampling();
//     vTaskDelay(1000);
// }
