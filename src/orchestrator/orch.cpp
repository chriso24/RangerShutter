#include "Orch.h"
#include <Preferences.h>

volatile bool Orch::abortRequested = false;

Orch::Orch(ILogger *logger) : logger(logger), directionClose(false), motorController(nullptr), currentMonitor(nullptr), recordedTimeForCycle(0), finishedSuccessfully(false), Task1(NULL) {

    if (preferences.begin("Triton", false))
    {
    recordedTimeForCycle = preferences.getInt("rtfc", 0);
    directionClose = preferences.getBool("directionClose", true);

     logger->LogEvent("Load time for cycle: " + std::string(String(recordedTimeForCycle).c_str()));
    logger->LogEvent("Load direction close: " + std::string(String(directionClose ? "true" : "false").c_str()));

    
}
else{
    logger->LogEvent("Failed to open preferences");
    recordedTimeForCycle = 5589;
    directionClose = true;
}
preferences.end();
}

Orch::~Orch()
{
    AbortMovement();
}


// void Orch::Init(Motor *moto, Current *current)
// {
//     preferences.begin("Triton", false);
//     recordedTimeForCycle = preferences.getInt("rtfc", 0);
//     directionClose = preferences.getBool("directionClose", true);

//     logger->LogEvent("Load time for cycle: " + std::string(String(recordedTimeForCycle).c_str()));
//     logger->LogEvent("Load direction close: " + std::string(String(directionClose ? "true" : "false").c_str()));

//     motorController = moto;
//     currentMonitor = current;
// }

void Orch::Reset()
{
    AbortMovement();
    recordedTimeForCycle = 0;
    
    preferences.begin("Triton", false);
    preferences.clear();
    preferences.end();
    // StartMovement(TOGGLE);
}

void Orch::StartMovement(Command direction)
{
    bool wasRunning = AbortMovement();

    if (direction == TOGGLE)
    {
        directionClose = !directionClose;
    }
    else
    {
        directionClose = direction == CLOSE;
    }

    xTaskCreatePinnedToCore(
        Loop,                 /* Function to implement the task */
        "Orch",               /* Name of the task */
        4096,                 /* Stack size in words */
        this,                 /* Task input parameter */
        tskIDLE_PRIORITY + 2, /* Priority of the task */
        &Task1,               /* Task handle. */
        0                     // 1
    );                        /* Core where the task should run */
}

bool Orch::AbortMovement()
{
    bool threadRunning = false;
    abortRequested = true;
    try
    {
        if (Task1 != NULL)
        {
            eTaskState taskState = eTaskGetState(Task1);
            if (taskState < eDeleted)
            {
                threadRunning = true;
                logger->LogEvent("Abort current movement");

                TickType_t startTick = xTaskGetTickCount();
                while(isRunning && eTaskGetState(Task1) != eDeleted && (xTaskGetTickCount() - startTick) < pdMS_TO_TICKS(500)) {
                    vTaskDelay(100);
                }
            }
        }
    }
    catch (const std::exception &e)
    {
        logger->LogEvent("AbortMovement failed: " + std::string(e.what()));
    }

    if (motorController != nullptr)
        motorController->Stop(false);

    if (currentMonitor != nullptr)    
        currentMonitor->ShutdownIna226();

    return threadRunning;
}

void Orch::Loop(void *pvParameters)
{
    Orch *p_pThis = static_cast<Orch *>(pvParameters);

    p_pThis->isRunning = true;
    try
    {
        p_pThis->SetupSystem();

        if (p_pThis->IsCalibrated())
            p_pThis->ActionMovement();
        else
            p_pThis->PerformCalibration();
    }
    catch(const std::runtime_error& e)
    {
        p_pThis->logger->LogEvent("Thread movement aborted: " + std::string(e.what()));
    }

    p_pThis->currentMonitor->ShutdownIna226();
    p_pThis->motorController->Stop(false);
    p_pThis->logger->LogEvent("Finish:" + std::string(p_pThis->directionClose ? "Close" : "Open"));
    

    UBaseType_t uxHighWaterMark = uxTaskGetStackHighWaterMark(NULL);
    p_pThis->logger->LogEvent("Orch stack high water mark: " + std::to_string(uxHighWaterMark) + " words");

    p_pThis->EndThread();
}

void Orch::EndThread()
{
    isRunning = false;
    Task1 = NULL;
    vTaskDelete(NULL);
}


void Orch::SetupSystem()
{
    abortRequested = false;
    currentMonitor = new Current(logger);
    currentMonitor->Init(); 
    motorController = new Motor(logger);
    motorController->Init(currentMonitor);
   
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
    directionClose = false;
    recordedTimeForCycle = motorController->GetCalibratedRunTime(false);

    if (recordedTimeForCycle == 0)
        recordedTimeForCycle = motorController->GetCalibratedRunTime(true);

    logger->LogEvent("Calibrated time for cycle: " + std::string(String(recordedTimeForCycle).c_str()));

    if (recordedTimeForCycle > 0)
    {
        finishedSuccessfully = true;

        preferences.begin("Triton", false);
        preferences.putInt("rtfc", recordedTimeForCycle);
        preferences.end();
    }
}

bool Orch::CheckForAbort()
{
    if (abortRequested)
    {
        abortRequested = false;
        logger->LogEvent("Abort requested");
        return true;
    }
    return false;
}


void Orch::CurrentInterupt()
{
   abortRequested = true;
   Motor::CurrentInterupt();
}

void Orch::ActionMovement()
{
    currentMonitor->Reset();

    // pass the local function as the callback
    currentMonitor->StartMonitor(CurrentInterupt, false);
    
    int speed = fastSpeed;
    int timeForCycle = recordedTimeForCycle;
    bool isRecoveryRun = !finishedSuccessfully;
    if (!finishedSuccessfully)
    {
        speed = recoverySpeed;
        timeForCycle = timeForCycle * 1.5;
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
    abortRequested = false;

    vTaskDelay(200);
    logger->LogEvent("\nCurrent limit max");

    motorController->SetSpeedAndDirection(directionClose, speed, 3, false);

    vTaskDelay(rampingOnRun);

    logger->LogEvent("\nCurrent limit normal");

    if (CheckForAbort()) return;
    currentMonitor->SetCurrentLimit(isRecoveryRun ? Current::CurrentLevel::C_LOW : Current::CurrentLevel::C_HIGH, !directionClose);

    while ((xTaskGetTickCount() - startTick) < timeForCycle && !abortRequested)
    {
        // currentMonitor->PrintCurrent();
        vTaskDelay(100);
    }

    logger->LogEvent("Slowing things down for end of cycle");

    if (CheckForAbort()) return;

    if (!isRecoveryRun)
        if (motorController->IsRunning())
        {
            logger->LogEvent("Success run");
            finishedSuccessfully = true;
            if (CheckForAbort()) return;
            motorController->SetSpeedAndDirection(directionClose, slowSpeed, 5, true);
            vTaskDelay(100);

            if (CheckForAbort()) return;
            currentMonitor->SetCurrentLimit(Current::CurrentLevel::C_VLOW, !directionClose);
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
        while ((xTaskGetTickCount() - startTick) < maxRunTimeAtEnd && !abortRequested)
        {
            vTaskDelay(100);
        }
        motorController->Stop(false);
    }

    if (CheckForAbort()) return;
    motorController->Stop(false);

    preferences.begin("Triton", false);
    preferences.putBool("directionClose", directionClose);
    preferences.end();

    
}