#ifndef MOTOR_H
#define MOTOR_H

#include "Arduino.h"
#include "../current/current.h"
#include "../logger/ILogger.h"

class Motor {
public:
    static volatile bool alert;
    static volatile int alertCount;
    static byte inMotion;
    static volatile int currentPositionEstimate;
    static void CurrentInterupt();

    void SetSpeedAndDirection(bool closing, int targetSpeed, int transitionTime, bool manageCurrent);
    static int CurrentSpeed;
    byte CurrentDirection;

    Motor(ILogger* logger);
    void Move(byte direction);
    void Stop(bool emergency);
    void Init(Current* current);
    bool IsRamping();
    void Reset();
    void Loop();
    bool IsRunning();
    int GetCalibratedRunTime(bool force);
    static void AllStop();

private:
    ILogger* logger;
    TickType_t timeAtLastSpeedChange;
    static Current* currentMonitor;
    int timeForFullCycle = 0;
    int recordedTimeForFullCycle = 0;
    int cycleStartTick = 0;
    int SetMaxSpeed = 255;
    const int smoothMotorValue = 0;
    const int rampingOnRun = 700;
    static const int motor_R = 19;
    static const int slowSpeed = 110;
    static const int fastSpeed = 255;
    static const int motor_L = 18;
    static const int minCycleTime = pdMS_TO_TICKS(2000);
    void WriteSpeed(int speed, bool isIncrease);
};

#endif // MOTOR_H



