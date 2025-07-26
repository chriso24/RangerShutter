#include "motor.h"
#include "Arduino.h"



volatile bool Motor::alert;
volatile int Motor::alertCount;
Current *Motor::currentMonitor;
byte Motor::inMotion;
volatile int Motor::currentPositionEstimate;
int Motor::CurrentSpeed;


Motor::Motor()
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
    //pinMode(2, INPUT_PULLUP);
    //attachInterrupt(digitalPinToInterrupt(2), Motor::CurrentInterupt, FALLING);
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
    Serial.println("Perfomring calibration");
    currentMonitor->SetCurrentLimit(force ? Current::CurrentLevel::C_HIGH : Current::CurrentLevel::C_MEDIUM, true);
    // We need to calibrate
    // Move all the way in 1 direction until over current

    TickType_t startTickTime = xTaskGetTickCount();
    TickType_t maxWaitTime = startTickTime + pdMS_TO_TICKS(25 * 1000);

    SetSpeedAndDirection(false, force ? fastSpeed : slowSpeed, 5, false);

    vTaskDelay(700);
    currentMonitor->SetCurrentLimit(Current::CurrentLevel::C_LOW,true);
    Serial.println("Waiting for end of track....");

    while (!alert && (maxWaitTime > xTaskGetTickCount()))
    {
        //currentMonitor->PrintCurrent();
        vTaskDelay(100);
    }

    currentMonitor->Reset();
    Stop(true);
    Serial.println("Found the end.");

    // Back the other way
    startTickTime = xTaskGetTickCount();
    maxWaitTime = startTickTime + pdMS_TO_TICKS(20 * 1000);
    currentMonitor->SetCurrentLimit(force ? Current::CurrentLevel::C_HIGH : Current::CurrentLevel::C_MEDIUM, false);

    Serial.println("Back the other way.");

    SetSpeedAndDirection(true, slowSpeed, 5, false);

    currentMonitor->Reset();
    vTaskDelay(700);
    currentMonitor->SetCurrentLimit(Current::CurrentLevel::C_LOW, false);
    Serial.println("Waiting for end of track....");

    while ((!alert) && (maxWaitTime > xTaskGetTickCount()))
    {
        vTaskDelay(100);
        //currentMonitor->PrintCurrent();
    }

    currentMonitor->Reset();
    Stop(true);

    cycleTime = (xTaskGetTickCount() - startTickTime) / 3;

    if (cycleTime <= minCycleTime)
    {
        cycleTime = 0;
        Serial.println("Cycle too short!");
    }

    Serial.print("Found the end after ");
    Serial.print(cycleTime) +
        Serial.println(" cycles.");

    // Perform stop here just incase we times out waiting for ends
    Stop(true);

    currentMonitor->Reset();
    currentMonitor->EndMonitor();

    return cycleTime;
}

bool Motor::IsRunning()
{
    Serial.print("Current Speed = ");
    Serial.println(CurrentSpeed);
    Serial.print("Reset Count = ");
    Serial.println(alertCount);
    return CurrentSpeed > 0;
}

void Motor::SetSpeedAndDirection(bool closing, int targetSpeed, int transitionTime, bool manageCurrent)
{
    Serial.println("Set speed and direction");
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
                Serial.println("Abort movement");
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
                Serial.println("Abort movement");
                break;
            }

            if (manageCurrent)
                currentMonitor->SetCurrentLimitPercentage(((float)i) / ((float)fastSpeed));
        }
    }

    Serial.print("Finished setting motor speed. Current speed = ");
    Serial.println(CurrentSpeed);
}



void Motor::Stop(bool emergency)
{
    inMotion = 0;
    Serial.print("Stop triggered at position ");
    Serial.println(currentPositionEstimate);
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
