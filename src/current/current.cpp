#include "current.h"
#include "Arduino.h"
#include <INA226_WE.h>
#include <Wire.h>
#include <cmath>

// I2C address for INA226
#define I2C_ADDRESS 0x40

// Static member definitions
INA226_WE* Current::ina226 = nullptr;
volatile int Current::interuptCount = 0;
float Current::currentAtLastInterupt = 0.0;
Current::ShutdownInterup Current::callBack = nullptr;

float Current::maxCurrentHigh = Current::defaultMaxCurrentHigh;
float Current::maxCurrentLow = Current::defaultMaxCurrentLow;
float Current::maxCurrentUltraLow = Current::defaultMaxCurrentUltraLow;
float Current::maxCurrentUltraUltraLow = Current::defaultMaxCurrentUltraUltraLow;
float Current::currentLimitPercentage = Current::defaultCurrentWatch;


// Constructor
Current::Current(ILogger* logger) : logger(logger)
{
     i2cMutex = xSemaphoreCreateMutex();
     pinMode(2, INPUT_PULLUP);
    attachInterrupt(digitalPinToInterrupt(2), Current::CurrentInterupt, FALLING);
}

Current::~Current()
{
    vSemaphoreDelete(i2cMutex);
}

void Current::Init()
{
    // Initialize I2C and INA226 instance
    Wire.begin(I2C_SDA, I2C_SCL);
    if (ina226 == nullptr)
    {
        ina226 = new INA226_WE(I2C_ADDRESS);
    }
    logger->LogEvent("Current Init");
    StartupIna226();

    // Do not attach external hardware interrupts here; callback mechanism will be used by monitor task
}

void Current::StartupIna226()
{
    if (xSemaphoreTake(i2cMutex, portMAX_DELAY) == pdTRUE)
    {
        if (ina226 == nullptr) {
            xSemaphoreGive(i2cMutex);
            return;
        }
        ina226->powerUp();
        ina226->init();
        // sensible defaults
        ina226->setResistorRange(1, 10.0);
        ina226->setMeasureMode(CONTINUOUS);
        //ina226->setConversionTime(CONV_TIME_140);
        // default alert threshold -- user can override via SetCurrentLimit/Percentage
        ina226->setAlertType(POWER_OVER, maxCurrentLow);
        ina226->setAlertPinActiveHigh();
        xSemaphoreGive(i2cMutex);
    }
}

// Start the monitoring task. Reentrant-safe: if already running, extend shutdown time.
void Current::StartMonitor(ShutdownInterup callBackFn, bool slowRun)
{
    isSlowRun = slowRun;
    callBack = callBackFn;

    // Ensure INA226 is powered and configured
    
    // if (ina226 == nullptr) {
    //     ina226 = new INA226_WE(I2C_ADDRESS);
    //     StartupIna226();
    // }

    // initialize moving averages with a reasonable default so spikes are measured against real data
    float seed = maxCurrentHigh;
    float seedShort = maxCurrentHigh * 0.8f;
    for (int i = 0; i < SHORT_WINDOW_SIZE; ++i) {
        currentMeasurements[i] = seedShort;
    }
    for (int i = 0; i < LONG_WINDOW_SIZE; ++i) {
        longCurrentMeasurements[i] = seed;
    }
    shortAverageCurrent = seed;
    longAverageCurrent = seed;

    shutdownTime = xTaskGetTickCount() + pdMS_TO_TICKS(60000);

    // If a task already exists and is running, extend shutdown and return
    // TODO: Crashes half the time - need to debug why
    if (Task1 != NULL) {
        ShutdownMonitor();
    }

    logger->LogEvent("Start current monitor");

    // Create the monitoring task with a modest priority
    wasRunning = true;
    xTaskCreatePinnedToCore(
        Loop,
        "CurrentMonitor",
        4096,
        this,
        tskIDLE_PRIORITY + 2,
        &Task1,
        1
    );
}

// Static FreeRTOS task entry
void Current::Loop(void* p_pParam)
{
    Current* self = static_cast<Current*>(p_pParam);
    if (!self) {
        vTaskDelete(NULL);
        return;
    }

    // Run the monitor until EndMonitor or timeout
    self->RunMonitor();

    // Ensure the callback is not called unexpectedly here
    vTaskDelete(NULL);
    self->Task1 = NULL;
    //vTaskDelete(NULL);
}

void Current::RunMonitor()
{
    Init();
    const TickType_t pollDelay = 20; // 1ms between measurements (ideal 140 us, but 1ms is the smallest) matches CONV_TIME_140)
    currentMEasurementCounter = 0;

    while (shutdownTime == 0 || shutdownTime > xTaskGetTickCount())
    {
        CheckCurrent();
        currentMEasurementCounter++;
        vTaskDelay(pollDelay);
    }

    logger->LogEvent("Finished current mon loop");

    ShutdownIna226();
}

void Current::ShutdownIna226()
{
    // signal the task to stop
    shutdownTime = 1;
    if (xSemaphoreTake(i2cMutex, portMAX_DELAY) == pdTRUE)
    {
        if (ina226) ina226->powerDown();
        xSemaphoreGive(i2cMutex);
    }

    logger->LogEvent("INA226 powered down");
}

float Current::GetBatteryPercentage()
{
    // Placeholder for battery percentage logic
    if (currentVoltage == 0.0f) return 1.0f; // No reading yet default 100%%
    logger->LogEvent("Battery voltage: " + std::string(String(currentVoltage).c_str()) + "V");
    return (currentVoltage / 14.0f); 
}

void Current::AdjustCurrentLimits( bool increase)
{
    if (increase && (maxCurrentHigh < defaultMaxCurrentHigh * 1.3))
    {
        maxCurrentHigh += 10.0f;
        maxCurrentLow += 10.0f;
        maxCurrentUltraLow += 10.0f;
        currentLimitPercentage += 0.03f;
        logger->LogEvent("Increased current limits");
    }
    else if (!increase && (maxCurrentHigh > defaultMaxCurrentHigh * 0.7))
    {
        maxCurrentHigh -= 0.05f;
        maxCurrentLow -= 0.5f;
        maxCurrentUltraLow -= 0.5f;
        currentLimitPercentage -= 0.001f;
        logger->LogEvent("Decreased current limits");
    }
}

void Current::ShutdownMonitor()
{
    // signal the task to stop
    shutdownTime = 1;
    if (Task1 != NULL)
    {
        eTaskState taskState = eTaskGetState(Task1);
        if (taskState < eDeleted)
        {
            logger->LogEvent("Aborting current monitor");

            TickType_t startTick = xTaskGetTickCount();
            while(Task1 != NULL && eTaskGetState(Task1) < eDeleted && (xTaskGetTickCount() - startTick) < pdMS_TO_TICKS(500)) {
                vTaskDelay(100);
            }
            
            Task1 = NULL; // Set to null before deleting
            logger->LogEvent("Aborted current monitor");
        }
    }
}

void Current::AttachInteruptForOverCurrent(ShutdownInterup callBackFn)
{
    callBack = callBackFn;
}

void Current::CurrentInterupt()
{
 //   Serial.println("Current interupt triggered from pin high");
    interuptCount++;
    if (callBack) callBack();
}

void Current::SetCurrentLimitPercentage(float percentage)
{
    float current = ((maxCurrentHigh - maxCurrentUltraLow) * percentage) + maxCurrentUltraLow;
    //logger->LogEvent("Set current limit to: " + std::string(String(current).c_str()));

    if (xSemaphoreTake(i2cMutex, portMAX_DELAY) == pdTRUE)
    {
        if (!ina226) {
            xSemaphoreGive(i2cMutex);
            return;
        }
        ina226->setAlertType(POWER_OVER, current);
        xSemaphoreGive(i2cMutex);
    }
}

bool Current::CheckInit()
{
    bool result = false;
    if (xSemaphoreTake(i2cMutex, portMAX_DELAY) == pdTRUE)
    {
        if (!ina226) {
            xSemaphoreGive(i2cMutex);
            return false;
        }
        ina226->readAndClearFlags();
        float voltage = ina226->getBusVoltage_V();
        if (voltage <= 0.01) {
            logger->LogEvent("INA226 init: no voltage");
            result = false;
        } else {
            result = true;
        }
        xSemaphoreGive(i2cMutex);
    }
    return result;
}

void Current::Reset()
{
    if (xSemaphoreTake(i2cMutex, portMAX_DELAY) == pdTRUE)
    {
        if (ina226) ina226->readAndClearFlags();
        interuptCount = 0;
        xSemaphoreGive(i2cMutex);
    }
}

void Current::SetCurrentLimit(Current::CurrentLevel level, bool closing)
{
    SetCurrentValue(level, closing);
}

void Current::SetAccelartionActive(bool active)
{
    accelerationActive = active;
}   

void Current::SetCurrentValue(Current::CurrentLevel level, bool closing)
{
    if (xSemaphoreTake(i2cMutex, portMAX_DELAY) == pdTRUE)
    {
        if (!ina226) {
            xSemaphoreGive(i2cMutex);
            return;
        }
        float val = maxCurrentHigh;
        switch (level)
        {
        case CurrentLevel::C_HIGH:
            val = closing ? maxCurrentHigh * 1.05f : maxCurrentHigh;
            logger->LogEvent("SetCurrentValue: HIGH" + std::string(String(val).c_str()));
            break;
        case CurrentLevel::C_MEDIUM:
            val = closing ? maxCurrentLow * 1.1f : maxCurrentLow;
            logger->LogEvent("SetCurrentValue: MEDIUM" + std::string(String(val).c_str()) );
            break;
        case CurrentLevel::C_LOW:
            val = closing ? maxCurrentUltraLow * 1.1f : maxCurrentUltraLow;
            logger->LogEvent("SetCurrentValue: LOW" + std::string(String(val).c_str()));
            break;
        case CurrentLevel::C_VLOW:
            val = maxCurrentUltraUltraLow;
            logger->LogEvent("SetCurrentValue: VLOW" + std::string(String(val).c_str()));
            break;
        }
        logger->LogEvent("SetCurrent: " + std::string(String(val).c_str()));
        ina226->setAlertType(POWER_OVER, val);
        currentLevel = level;
        xSemaphoreGive(i2cMutex);
    }
}

// Core monitoring logic: measure, update moving averages, detect spike >15% and call callback.
void Current::CheckCurrent()
{
    float busPower = 0;
    if (xSemaphoreTake(i2cMutex, portMAX_DELAY) == pdTRUE)
    {
        if (!ina226) {
            xSemaphoreGive(i2cMutex);
            return;
        }

        // Read measurement, be robust to occasional 0 readings
        ina226->readAndClearFlags();
        busPower = ina226->getBusPower();
        currentVoltage = ina226->getBusVoltage_V();
        //logger->LogEvent("Bus V: " + std::string(String(ina226->getBusVoltage_V()).c_str()) + "W, Shunt voltage: " + std::string(String(currentVoltage).c_str()) + "V");
        xSemaphoreGive(i2cMutex);
    }

    if (busPower < minCurrentRead) {
        // ignore low/no-load readings
        return;
    }

    // Update short window circular buffer
    for (int i = 0; i < SHORT_WINDOW_SIZE - 1; ++i)
        currentMeasurements[i] = currentMeasurements[i + 1];
    currentMeasurements[SHORT_WINDOW_SIZE - 1] = busPower;

    // compute short average
    float shortSum = 0.0f;
    for (int i = 0; i < SHORT_WINDOW_SIZE; ++i) shortSum += currentMeasurements[i];
    shortAverageCurrent = shortSum / SHORT_WINDOW_SIZE;

    // every LONG_WINDOW_COUNT measurements update the long average
    //if ((currentMEasurementCounter % LONG_WINDOW_COUNT) == 0) {
        for (int i = 0; i < LONG_WINDOW_SIZE - 1; ++i)
            longCurrentMeasurements[i] = longCurrentMeasurements[i + 1];
        longCurrentMeasurements[LONG_WINDOW_SIZE - 1] = busPower;

        float longSum = 0.0f;
        for (int i = 0; i < LONG_WINDOW_SIZE; ++i) longSum += longCurrentMeasurements[i];
        longAverageCurrent = longSum / LONG_WINDOW_SIZE;
    //}

    // Calculate relative change against shortAverageCurrent
    float change = 0.0f;
    if (shortAverageCurrent > 0.0f)
        change = (shortAverageCurrent - longAverageCurrent) / shortAverageCurrent;

    // Log occasionally for debugging
    if ((currentMEasurementCounter % 20) == 0)
    {
        logger->LogEvent("Current check: bus=" + std::string(String(busPower).c_str()) + ", shortAvg=" + std::string(String(shortAverageCurrent).c_str()) + " longAvg=" + std::string(String(longAverageCurrent).c_str()));
    }

    // Trigger callback on significant positive spike > 15% (0.15) or negative drop of similar magnitude
    const float spikeThreshold = accelerationActive ? 0.5f :  currentLimitPercentage; // 15%
    if (change > spikeThreshold)
    {
        logger->LogEvent("ALERT: current change detected: " + std::string(String(change).c_str()));
        if (callBack) callBack();
    }

    // TODO: Only abort in this way if current change is low for 2 seconds
    // if (abs(change) < 0.01f)
    // {
    //     logger->LogEvent("ALERT: current change too steady: " + std::string(String(change).c_str()));
    //     if (callBack) callBack();
    // }

}

void Current::PrintCurrent()
{
    if (xSemaphoreTake(i2cMutex, portMAX_DELAY) == pdTRUE)
    {
        if (!ina226) {
            xSemaphoreGive(i2cMutex);
            return;
        }
        ina226->readAndClearFlags();
        float currentValue = ina226->getCurrent_mA();
        float bus = ina226->getBusPower();
        logger->LogEvent("Current(mA): " + std::string(String(currentValue).c_str()));
        logger->LogEvent("Power: " + std::string(String(bus).c_str()));
        xSemaphoreGive(i2cMutex);
    }
}

