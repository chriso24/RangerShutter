#ifndef Orch_h
#define Orch_h
#include "Arduino.h" 
#include <motor/motor.h>



class Orch {
public:

    void StartMovement();
    bool AbortMovement();
    void ActionMovement();
    void Stop(bool emergency);
    void Init(Motor* motorController,Current* currentMonitor);
    void PerformCalibration();
    bool IsCalibrated();
    void EndThread();

    void Reset();

    static void Loop(void* p_pParam);
    
private:
    bool directionClose;
    Motor* motorController;
Current* currentMonitor;
int recordedTimeForCycle;

bool finishedSuccessfully;



const int rampingOnRun = 1100; // ticks
    const int slowSpeed = 80;
    const int recoverySpeed = 120;
    const int fastSpeed = 255; // Max 255
    const int maxRunTimeAtEnd = 10000;// seconds
    
    TaskHandle_t Task1;    
    static const int OrchWakeTime = 1000; // seconds
};
#endif



