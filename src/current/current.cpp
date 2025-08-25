#include "current.h"
#include "Arduino.h"
#include <INA226_WE.h>
#define I2C_ADDRESS 0x40

INA226_WE *Current::ina226;

volatile int Current::interuptCount;
Current::ShutdownInterup callBackOnOverCurrent;
float Current::currentAtLastInterupt;

Current::Current(ILogger* logger) : logger(logger) {}

void Current::Init()
{
    ina226 = new INA226_WE(I2C_ADDRESS);
    logger->LogEvent("Current Init");
    Wire.begin(I2C_SDA, I2C_SCL);
    ina226->init();

    ina226->setResistorRange(1, 10.0);
    ina226->setMeasureMode(CONTINUOUS);
    ina226->setAlertType(POWER_OVER, maxCurrentLow);
    ina226->setAlertPinActiveHigh();
    pinMode(2, INPUT_PULLUP);
    attachInterrupt(digitalPinToInterrupt(2), Current::CurrentInterupt, FALLING);
}

void Current::StartMonitor(ShutdownInterup callBack, bool slowRun)
{
    isSlowRun = slowRun;
    if (Task1 != NULL)
    {
        eTaskState taskState = eTaskGetState(Task1);
        if (taskState != eDeleted)
        {
            shutdownTime += 60000;
            return;
        }
    }

    logger->LogEvent("---------Start current monitor");
    AttachInteruptForOverCurrent(callBack);
    for(int i = 0; i < SHORT_WINDOW_SIZE; i++)
    {
        currentMeasurements[i] = maxCurrentHigh/2;
        longCurrentMeasurements[i] = maxCurrentHigh/2;
    }
    shortAverageCurrent = maxCurrentHigh/2;
    longAverageCurrent = maxCurrentHigh/2;



    if (!wasRunning)
    {
        xTaskCreate(
            Loop,                     /* Function to implement the task */
            "Current",                   /* Name of the task */
            2048,                     /* Stack size in words */
            this,                     /* Task input parameter */
            configMAX_PRIORITIES - 1, /* Priority of the task */
            &Task1                    /* Task handle. */
                                      // 1
        );                            /* Core where the task should run */
    }
}

void Current::Loop(void *pvParameters)
{
    Current *p_pThis = static_cast<Current *>(pvParameters);
    
        p_pThis->RunMonitor();
    
    callBackOnOverCurrent();
    p_pThis->EndThread();
}

void Current::RunMonitor()
{
    shutdownTime = xTaskGetTickCount() + 60000;

    while(shutdownTime > xTaskGetTickCount())
    {
        CheckCurrent();
        vTaskDelay(10);
    }
}

void Current::EndMonitor()
{
    EndThread();
}

void Current::EndThread()
{
    vTaskDelete(Task1);
    logger->LogEvent("---------End current monitor");
}

void Current::AttachInteruptForOverCurrent(ShutdownInterup callBack)
{
    callBackOnOverCurrent = callBack;
}

void Current::CurrentInterupt()
{
    int startInteruptCount = interuptCount;
    interuptCount++;

    // ina226->
    // ina226->readAndClearFlags();
    // currentAtLastInterupt = ina226->getBusPower();

    // delay(100);
callBackOnOverCurrent();
    //if (((interuptCount > 0) || startInteruptCount == interuptCount) && callBackOnOverCurrent != NULL)
    //    callBackOnOverCurrent();
}

void Current::SetCurrentLimitPercentage(float percentage)
{
    logger->LogEvent("Set current perdcentage: " + std::string(String(percentage).c_str()));
    float current = ((float)(maxCurrentHigh - maxCurrentUltraLow) * percentage) + maxCurrentUltraLow;

    logger->LogEvent("Set current: " + std::string(String(current).c_str()));

    ina226->setAlertType(POWER_OVER, current * 1.00);
}

bool Current::CheckInit()
{
    ina226->readAndClearFlags();
    float voltage = ina226->getBusVoltage_V();

    for (size_t i = 0; i < 5000; i++)
    {
        if (voltage > 0)
            break;

        delay(100);
        ina226->readAndClearFlags();
        voltage = ina226->getBusVoltage_V();
    }

    if (voltage == 0)
    {
        logger->LogEvent("Init Failed!!!!");
        ina226->reset_INA226();
        return false;
    }
    return true;
}

void Current::Reset()
{
    ina226->readAndClearFlags();
    interuptCount = 0;
}



void Current::SetCurrentLimit(Current::CurrentLevel level, bool closing)
{
    // if (currentLevel == level)
    // return;

    SetCurrentValue(level, closing);

    // ina226->readAndClearFlags();
    // float currentValue = ina226->getCurrent_mA();
    // float bus = ina226->getBusPower();

    // SetCurrentValue(level);

    // Serial.print("Current: "); Serial.println(currentValue);
    // Serial.print("Power: "); Serial.println(bus);
}

void Current::SetCurrentValue(Current::CurrentLevel level, bool closing)
{
    switch (level)
    {
    case CurrentLevel::C_HIGH:
        ina226->setAlertType(POWER_OVER, closing ? maxCurrentHigh * 1.05 : maxCurrentHigh);
        if (currentLevel != level)
            logger->LogEvent("High");
        break;
    case CurrentLevel::C_MEDIUM:
        ina226->setAlertType(POWER_OVER, closing ? maxCurrentLow * 1.1 : maxCurrentLow);
        if (currentLevel != level)
            logger->LogEvent(closing ? "Medium*1.5" : "Medium");
        break;
    case CurrentLevel::C_LOW:
        ina226->setAlertType(POWER_OVER, closing ? maxCurrentUltraLow * 1.1 : maxCurrentUltraLow);
        if (currentLevel != level)
            logger->LogEvent(closing ? "Low*1.5" : "Low");
        break;
    case CurrentLevel::C_VLOW:
        ina226->setAlertType(POWER_OVER, maxCurrentUltraUltraLow);
        if (currentLevel != level)
            logger->LogEvent("VeryLow");
        break;
    default:
        break;
    }

    currentLevel = level;
}

void Current::CheckCurrent()
{
    currentMEasurementCounter++;
    ina226->readAndClearFlags();
    float bus = ina226->getBusPower();

    if (bus < minCurrentRead || bus == currentMeasurements[SHORT_WINDOW_SIZE - 1])
        return;

    float newAvergae = currentMeasurements[0];
    for (int i = 0; i < SHORT_WINDOW_SIZE - 1; i++)
    {
        currentMeasurements[i] = currentMeasurements[i + 1];
        newAvergae += currentMeasurements[i];
    }
    currentMeasurements[SHORT_WINDOW_SIZE - 1] = bus;
    newAvergae = (newAvergae + bus) / 4.0;
    shortAverageCurrent = newAvergae;

    float currentShift = ((bus - shortAverageCurrent) / shortAverageCurrent);

    if (currentMEasurementCounter % LONG_WINDOW_COUNT == 0)
    {
        logger->LogEvent("Current Average: " + std::string(String(shortAverageCurrent).c_str()));
        logger->LogEvent("Long Average: " + std::string(String(longAverageCurrent).c_str()));
        logger->LogEvent("Current Shift:   " + std::string(String(currentShift).c_str()));

        currentMEasurementCounter = 0;
        float newLongAvergae = longCurrentMeasurements[0];
        for (int i = 0; i < SHORT_WINDOW_SIZE - 1; i++)
        {
            longCurrentMeasurements[i] = longCurrentMeasurements[i + 1];
            newLongAvergae += longCurrentMeasurements[i];
        }

        newLongAvergae = (newLongAvergae + newAvergae) / 4.0;

        if ( abs((longAverageCurrent - newLongAvergae)/longAverageCurrent) <0.002 && longAverageCurrent < maxCurrentUltraUltraLow)
        {
            if (!isSlowRun)
            {
                logger->LogEvent("Long average is not changing enough. ABORT");
                callBackOnOverCurrent(); // Trigger the callback for over current
            }
            else
                logger->LogEvent("Long average is not changing enough. But slow run, so ignore.");
        }

        longAverageCurrent = newLongAvergae;
    }

    if (currentShift > ALERT_PERCENTAGE)
    {
        logger->LogEvent("ALERT: Current shift is too high!");
        callBackOnOverCurrent(); // Trigger the callback for over current
    }
   
}

void Current::PrintCurrent()
{
    ina226->readAndClearFlags();
    float currentValue = ina226->getCurrent_mA();
    float bus = ina226->getBusPower();
    logger->LogEvent("Current: " + std::string(String(currentValue).c_str()));
    logger->LogEvent("Power: " + std::string(String(bus).c_str()));
    logger->LogEvent("Interupt: " + std::string(String(currentAtLastInterupt).c_str()));
    logger->LogEvent("InteruptCount: " + std::string(String(interuptCount).c_str()));
}

