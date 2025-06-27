#ifndef Motor_h
#define Motor_h
#include "Arduino.h" 
#include "../current/current.h"

// This should just be a static class or singleton


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
    //const int MaxSpeed = 255; //255

    Motor();

	void Move(byte direction);
    void Stop(bool emergency);

    void Init(Current *current);

    bool IsRamping();

    void Reset();

    void Loop();

    bool IsRunning();

    int GetCalibratedRunTime(bool force);
    //void Alert();

private:
    // int currentSpeed;
    // int currentDirection;
    void SetCurrentForSpeed(int speed);
    
    TickType_t timeAtLastSpeedChange;
    static Current* currentMonitor;
    
    int timeForFullCycle = 0;
    int recordedTimeForFullCycle = 0;
    int cycleStartTick = 0;
    int SetMaxSpeed = 255; //255
    

    const int smoothMotorValue = 0; // Increasing makes speed chnages slower
    const int rampingOnRun = 700; // ticks
    static const int motor_R = 19;
    static const int slowSpeed = 110;
    static const int fastSpeed = 255; // Max 255
    static const int motor_L = 18;
    static const int minCycleTime = pdMS_TO_TICKS(2000);
    
    void WriteSpeed(int speed, bool isIncrease);
    
};
#endif



