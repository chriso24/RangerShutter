#ifndef ORCH_H
#define ORCH_H

#include "Arduino.h"
#include "../logger/ILogger.h"
#include <motor/motor.h>
#include <Preferences.h>

class Orch {
public:

    enum Command {
        OPEN,
        CLOSE,
        TOGGLE
    };

    Orch(ILogger* logger, Preferences *pref);
    ~Orch();
    void Reset(int timeForCycle);
    void UpdateRunTime(bool increase);
    // void Init(Motor* motorController, Current* currentMonitor);
    void StartMovement(Command direction);
    bool AbortMovement();
    void ActionMovement();
    void Stop(bool emergency);
    void PerformCalibration();
    
    bool IsCalibrated();
    void EndThread();
    void Reset();
    
    bool isIdle();
    static void Loop(void* p_pParam);
    static void CurrentInterupt();
    static volatile bool abortRequested;

private:

void SetupSystem();
bool CheckForAbort();

    ILogger* logger;
    Preferences* preferences;
    bool directionClose;
    Motor* motorController;
    Current* currentMonitor;
    int recordedTimeForCycle;
    bool finishedSuccessfully;
    const int rampingOnRun = 2000;
    const int slowSpeed = 80;
    const int recoverySpeed = 120;
    const int fastSpeed = 255;
    const int maxRunTimeAtEnd = 10000;
    TaskHandle_t Task1 = NULL;
    static const int OrchWakeTime = 1000;
    TickType_t timeAtLastFinish = 0;
    
    bool isRunning = false;
};

#endif // ORCH_H



