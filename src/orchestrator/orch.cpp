#include "Orch.h"

void Orch::Init(Motor *moto, Current *current)
{
    recordedTimeForCycle = 0;
    motorController = moto;
    currentMonitor = current;
}

void Orch::Reset()
{
    AbortMovement();
    recordedTimeForCycle = 0;

    StartMovement();
}

void Orch::StartMovement()
{

    bool wasRunning = AbortMovement();

    if (!wasRunning)
    {
        directionClose = !directionClose;
        xTaskCreate(
            Loop,   /* Function to implement the task */
            "Orch", /* Name of the task */
            2048,  /* Stack size in words */
            this,   /* Task input parameter */
            1,      /* Priority of the task */
            &Task1  /* Task handle. */
                    // 1
        );          /* Core where the task should run */
    }
}

bool Orch::AbortMovement()
{
    if (Task1 != NULL)
    {
        eTaskState taskState = eTaskGetState(Task1);
        if (taskState != eDeleted)
        {
            vTaskDelete(Task1);
            motorController->Stop(false);
            return true;
        }
    }

    motorController->Stop(false);
    return false;
}

void Orch::Loop(void *pvParameters)
{
    Orch *p_pThis = static_cast<Orch *>(pvParameters);

    if (p_pThis->IsCalibrated())
        p_pThis->ActionMovement();
    else
        p_pThis->PerformCalibration();

    p_pThis->EndThread();
}

void Orch::EndThread()
{
    vTaskDelete(Task1);
}

bool Orch::IsCalibrated()
{
    return recordedTimeForCycle > 0;
}

void Orch::PerformCalibration()
{
    recordedTimeForCycle = motorController->GetCalibratedRunTime(false);

    if (recordedTimeForCycle == 0)
        recordedTimeForCycle = motorController->GetCalibratedRunTime(true);

    finishedSuccessfully = true;
}

void Orch::ActionMovement()
{
    currentMonitor->StartMonitor(Motor::CurrentInterupt);
    int speed = fastSpeed;
    int timeForCycle = recordedTimeForCycle;
    bool isRecoveryRun = !finishedSuccessfully;
    if (!finishedSuccessfully)
    {
        speed = recoverySpeed;
        timeForCycle = timeForCycle*1.5;
    }
    
    finishedSuccessfully = false;
    Serial.println("\nOrch start");

    

    if (!directionClose)
    {
        Serial.println("Close");
        timeForCycle = timeForCycle * 1.05;
    }

    Serial.print("Cycle time = ");
    Serial.println(timeForCycle);

    TickType_t startTick = xTaskGetTickCount();

    currentMonitor->Reset();
    currentMonitor->SetCurrentLimit(Current::CurrentLevel::C_HIGH, !directionClose);
    vTaskDelay(200);
    Serial.println("\nCurrent limit max");

    motorController->SetSpeedAndDirection(directionClose, speed, 3, false);

    vTaskDelay(rampingOnRun);

    Serial.println("\nCurrent limit normal");

    currentMonitor->SetCurrentLimit(isRecoveryRun ? Current::CurrentLevel::C_LOW : Current::CurrentLevel::C_HIGH, !directionClose);

    while ((xTaskGetTickCount() - startTick) < timeForCycle)
    {
        currentMonitor->PrintCurrent();
        vTaskDelay(100);
    }

    Serial.println("Slowing things down for end of cycle");

    if (!isRecoveryRun)
    if (motorController->IsRunning())
    {
        Serial.println("Success run");
        finishedSuccessfully = true;
        motorController->SetSpeedAndDirection(directionClose, slowSpeed, 5, true);
        vTaskDelay(1500);
        currentMonitor->SetCurrentLimit(Current::CurrentLevel::C_VLOW,!directionClose);
    }
    else
    {
        Serial.println("Fail run");
    }

    if (isRecoveryRun)
        finishedSuccessfully = true;

    if (motorController->CurrentSpeed != 0)
    {
        startTick = xTaskGetTickCount();
        while ((xTaskGetTickCount() - startTick) < maxRunTimeAtEnd)
        {
            vTaskDelay(100);
        }
        motorController->Stop(false);
    }

    currentMonitor->EndMonitor();
    Serial.println("\nOrch shutdown");
}