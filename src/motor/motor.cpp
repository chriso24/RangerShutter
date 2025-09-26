#include "motor.h"
#include "Arduino.h"



volatile bool Motor::alert;
volatile int Motor::alertCount;
Current *Motor::currentMonitor;
byte Motor::inMotion;
volatile int Motor::currentPositionEstimate;
int Motor::CurrentSpeed;


void Motor::AllStop() {
    Motor::inMotion = 0;
    Motor::CurrentSpeed = 0;
    analogWrite(Motor::motor_L, 0);
    analogWrite(Motor::motor_R, 0);
}   

Motor::Motor(ILogger* logger) : logger(logger)
{
    CurrentSpeed = 0;
    CurrentDirection = 0;
    inMotion = 0;
    currentPositionEstimate = 0;
    timeAtLastSpeedChange = 0;
    // Singleton = this;
}

void Motor::Init(Current *current)
{
    currentMonitor = current;
    currentMonitor->AttachInteruptForOverCurrent(Motor::CurrentInterupt);
   
    Stop(true);
}

void Motor::Loop()
{
    if(CurrentSpeed > 0)
    {
        if ((xTaskGetTickCount() - timeAtLastSpeedChange) > 60000)
        {
            Stop(true);
        }
    }
}

void Motor::CurrentInterupt()
{
    //if (alertCount > 1)
    //{
        alertCount++;
        inMotion = 0;
        alert = true;
        CurrentSpeed = 0;
        analogWrite(motor_L, 0);
        analogWrite(motor_R, 0);

        if (abs(currentPositionEstimate) > 95)
        {
            currentPositionEstimate = 0;
        }
    //}
   // currentMonitor->Reset();
}

int Motor::GetCalibratedRunTime(bool force)
{
    currentMonitor->StartMonitor(Motor::CurrentInterupt, true);
    int cycleTime = 0;
    logger->LogEvent("Performing calibration");
    currentMonitor->SetCurrentLimit(force ? Current::CurrentLevel::C_HIGH : Current::CurrentLevel::C_MEDIUM, true);

    TickType_t startTickTime = xTaskGetTickCount();
    TickType_t maxWaitTime = startTickTime + pdMS_TO_TICKS(25 * 1000);

    SetSpeedAndDirection(false, force ? fastSpeed : slowSpeed, 5, false);

    vTaskDelay(700);
    currentMonitor->SetCurrentLimit(Current::CurrentLevel::C_LOW,true);
    logger->LogEvent("Waiting for end of track....");

    while (!alert && (maxWaitTime > xTaskGetTickCount()))
    {
        vTaskDelay(100);
    }

    currentMonitor->Reset();
    Stop(true);
    logger->LogEvent("Found the end.");

    startTickTime = xTaskGetTickCount();
    maxWaitTime = startTickTime + pdMS_TO_TICKS(20 * 1000);
    currentMonitor->SetCurrentLimit(force ? Current::CurrentLevel::C_HIGH : Current::CurrentLevel::C_MEDIUM, false);

    logger->LogEvent("Back the other way.");

    SetSpeedAndDirection(true,  force ? fastSpeed : slowSpeed, 5, false);

    currentMonitor->Reset();
    vTaskDelay(700);
    currentMonitor->SetCurrentLimit(Current::CurrentLevel::C_LOW, false);
    logger->LogEvent("Waiting for end of track....");

    while ((!alert) && (maxWaitTime > xTaskGetTickCount()))
    {
        vTaskDelay(100);
    }

    currentMonitor->Reset();
    Stop(true);

    cycleTime = (xTaskGetTickCount() - startTickTime) / 3;

    if (cycleTime <= minCycleTime)
    {
        cycleTime = 0;
        logger->LogEvent("Cycle too short!");
    }

    logger->LogEvent("Found the end after " + std::string(String(cycleTime).c_str()) + " cycles.");

    Stop(true);

    currentMonitor->Reset();
    currentMonitor->ShutdownIna226();

    return cycleTime;
}

bool Motor::IsRunning()
{
    logger->LogEvent("Current Speed = " + std::string(String(CurrentSpeed).c_str()));
    logger->LogEvent("Reset Count = " + std::string(String(alertCount).c_str()));
    return CurrentSpeed > 0;
}

void Motor::SetSpeedAndDirection(bool closing, int targetSpeed, int transitionTime, bool manageCurrent)
{
    logger->LogEvent("Set speed and direction");
    alert = false;
    alertCount = 0;

    timeAtLastSpeedChange = xTaskGetTickCount() ;

    if (CurrentSpeed < targetSpeed)
    {
        for (int i = CurrentSpeed; i < targetSpeed; i++)
        {
            analogWrite(closing ? motor_R : motor_L, i);
            CurrentSpeed = i;
            vTaskDelay(transitionTime);
            if (alert)
            {
                logger->LogEvent("Abort movement");
                break;
            }

            if (manageCurrent)
                currentMonitor->SetCurrentLimitPercentage(((float)i) / ((float)fastSpeed));
        }
    }
    else if (CurrentSpeed > targetSpeed)
    {
        for (int i = CurrentSpeed; i > targetSpeed; i--)
        {
            analogWrite(closing ? motor_R : motor_L, i);
            CurrentSpeed = i;
            vTaskDelay(transitionTime);
            if (alert)
            {
                logger->LogEvent("Abort movement");
                break;
            }

            if (manageCurrent)
                currentMonitor->SetCurrentLimitPercentage(((float)i) / ((float)fastSpeed));
        }
    }

    logger->LogEvent("Finished setting motor speed. Current speed = " + std::string(String(CurrentSpeed).c_str()));
}



void Motor::Stop(bool emergency)
{
    inMotion = 0;
    logger->LogEvent("Stop triggered at position " + std::string(String(currentPositionEstimate).c_str()));
    logger->LogEvent("CurrentSpeed " + std::string(String(CurrentSpeed).c_str()));
    for (int i = CurrentSpeed; i >= 0; i--)
    {
        analogWrite(CurrentDirection ? motor_R : motor_L, i);
        vTaskDelay(emergency ? 1 : 2);
    }
    analogWrite(CurrentDirection ? motor_R : motor_L, 0);
    CurrentSpeed = 0;

    if (!emergency)
        if (abs(currentPositionEstimate) > 95)
        {
            currentPositionEstimate = 0;
        }
};
