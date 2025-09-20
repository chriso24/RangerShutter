#include "Orch.h"
#include <Preferences.h>

Orch::Orch(ILogger* logger) : logger(logger), directionClose(false), motorController(nullptr), currentMonitor(nullptr), recordedTimeForCycle(0), finishedSuccessfully(false) {}


void Orch::Init(Motor *moto, Current *current)
{
    preferences.begin("Triton", false);
    recordedTimeForCycle = preferences.getInt("recordedTimeForCycle", 0);
    directionClose = preferences.getBool("directionClose", false);

 
    logger->LogEvent("Load time for cycle: " + std::string(String(recordedTimeForCycle).c_str()));
    logger->LogEvent("Load direction close: " + std::string(String(directionClose ? "true" : "false").c_str()));


    motorController = moto;
    currentMonitor = current;
}


void Orch::Reset()
{
    AbortMovement();
    recordedTimeForCycle = 0;
    preferences.clear();
    StartMovement(TOGGLE);
}

void Orch::StartMovement(Command direction)
{
    bool wasRunning = AbortMovement();

    if (!wasRunning)
    {
        if (direction == TOGGLE)
        {
            directionClose = !directionClose;
        }
        else
        {
            directionClose = direction == CLOSE;
        }

        xTaskCreate(
            Loop,   /* Function to implement the task */
            "Orch", /* Name of the task */
            2048,   /* Stack size in words */
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

bool Orch::isIdle()
{
    if (Task1 == NULL)
    {
        return true;
    }
    eTaskState taskState = eTaskGetState(Task1);
    return taskState == eDeleted || taskState == eInvalid;
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

    if (recordedTimeForCycle > 0)
    {
        finishedSuccessfully = true;

        preferences.putInt("recordedTimeForCycle", recordedTimeForCycle);
    }
}

void Orch::ActionMovement()
{
    currentMonitor->Reset();
    currentMonitor->StartMonitor(Motor::CurrentInterupt, false);
    int speed = fastSpeed;
    int timeForCycle = recordedTimeForCycle;
    bool isRecoveryRun = !finishedSuccessfully;
    if (!finishedSuccessfully)
    {
        speed = recoverySpeed;
        timeForCycle = timeForCycle*1.5;
    }
    
    finishedSuccessfully = false;
    

    if (!directionClose)
    {
        logger->LogEvent("Start:Close");
        timeForCycle = timeForCycle * 1.05;
    }
    else
    {
        logger->LogEvent("Start:Open");
    }

    logger->LogEvent("Cycle time = " + std::string(String(timeForCycle).c_str()));

    TickType_t startTick = xTaskGetTickCount();

    currentMonitor->Reset();
    currentMonitor->SetCurrentLimit(Current::CurrentLevel::C_HIGH, !directionClose);
    vTaskDelay(200);
    logger->LogEvent("\nCurrent limit max");

    motorController->SetSpeedAndDirection(directionClose, speed, 3, false);

    vTaskDelay(rampingOnRun);

    logger->LogEvent("\nCurrent limit normal");

    currentMonitor->SetCurrentLimit(isRecoveryRun ? Current::CurrentLevel::C_LOW : Current::CurrentLevel::C_HIGH, !directionClose);

    while ((xTaskGetTickCount() - startTick) < timeForCycle)
    {
        currentMonitor->PrintCurrent();
        vTaskDelay(100);
    }

    logger->LogEvent("Slowing things down for end of cycle");

    if (!isRecoveryRun)
    if (motorController->IsRunning())
    {
        logger->LogEvent("Success run");
        finishedSuccessfully = true;
        motorController->SetSpeedAndDirection(directionClose, slowSpeed, 5, true);
        vTaskDelay(1500);
        currentMonitor->SetCurrentLimit(Current::CurrentLevel::C_VLOW,!directionClose);
    }
    else
    {
        logger->LogEvent("Fail run");
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

    preferences.putBool("directionClose", directionClose);

    currentMonitor->EndMonitor();
    logger->LogEvent("Finish:" + std::string(directionClose ? "Close" : "Open"));
}