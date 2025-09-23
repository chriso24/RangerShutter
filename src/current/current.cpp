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

// Constructor
Current::Current(ILogger* logger) : logger(logger)
{
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
    if (ina226 == nullptr) return;
    ina226->init();
    // sensible defaults
    ina226->setResistorRange(1, 10.0);
    ina226->setMeasureMode(CONTINUOUS);
    //ina226->setConversionTime(CONV_TIME_140);
    // default alert threshold -- user can override via SetCurrentLimit/Percentage
    ina226->setAlertType(POWER_OVER, maxCurrentLow);
    ina226->setAlertPinActiveHigh();
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
    if (Task1 != NULL) {
        eTaskState state = eTaskGetState(Task1);
        if (state != eDeleted) {
            shutdownTime = xTaskGetTickCount() + pdMS_TO_TICKS(60000);
            logger->LogEvent("Monitor already running, extended shutdown");
            return;
        }
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
    //vTaskDelete(NULL);
}

void Current::RunMonitor()
{
    Init();
    const TickType_t pollDelay = 2; // 1ms between measurements (ideal 140 us, but 1ms is the smallest) matches CONV_TIME_140)
    currentMEasurementCounter = 0;

    while (shutdownTime == 0 || shutdownTime > xTaskGetTickCount())
    {
        CheckCurrent();
        currentMEasurementCounter++;
        vTaskDelay(pollDelay);
    }

    ShutdownIna226();
}

void Current::ShutdownIna226()
{
    // signal the task to stop
    shutdownTime = 1;
    if (ina226) ina226->powerDown();
}

void Current::EndThread()
{
    ShutdownIna226();
    if (Task1 != NULL) {
        vTaskDelete(Task1);
        Task1 = NULL;
    }
    wasRunning = false;
    logger->LogEvent("End current monitor");
}

void Current::AttachInteruptForOverCurrent(ShutdownInterup callBackFn)
{
    callBack = callBackFn;
}

void Current::CurrentInterupt()
{
    interuptCount++;
    if (callBack) callBack();
}

void Current::SetCurrentLimitPercentage(float percentage)
{
    if (!ina226) return;
    float current = ((maxCurrentHigh - maxCurrentUltraLow) * percentage) + maxCurrentUltraLow;
    ina226->setAlertType(POWER_OVER, current);
    logger->LogEvent("Set current limit to: " + std::string(String(current).c_str()));
}

bool Current::CheckInit()
{
    if (!ina226) return false;
    ina226->readAndClearFlags();
    float voltage = ina226->getBusVoltage_V();
    if (voltage <= 0.01) {
        logger->LogEvent("INA226 init: no voltage");
        return false;
    }
    return true;
}

void Current::Reset()
{
    if (ina226) ina226->readAndClearFlags();
    interuptCount = 0;
}

void Current::SetCurrentLimit(Current::CurrentLevel level, bool closing)
{
    SetCurrentValue(level, closing);
}

void Current::SetCurrentValue(Current::CurrentLevel level, bool closing)
{
    if (!ina226) return;
    float val = maxCurrentHigh;
    switch (level)
    {
    case CurrentLevel::C_HIGH:
        val = closing ? maxCurrentHigh * 1.05f : maxCurrentHigh;
        logger->LogEvent("SetCurrentValue: HIGH");
        break;
    case CurrentLevel::C_MEDIUM:
        val = closing ? maxCurrentLow * 1.1f : maxCurrentLow;
        logger->LogEvent("SetCurrentValue: MEDIUM");
        break;
    case CurrentLevel::C_LOW:
        val = closing ? maxCurrentUltraLow * 1.1f : maxCurrentUltraLow;
        logger->LogEvent("SetCurrentValue: LOW");
        break;
    case CurrentLevel::C_VLOW:
        val = maxCurrentUltraUltraLow;
        logger->LogEvent("SetCurrentValue: VLOW");
        break;
    }
    ina226->setAlertType(POWER_OVER, val);
    currentLevel = level;
}

// Core monitoring logic: measure, update moving averages, detect spike >15% and call callback.
void Current::CheckCurrent()
{
    if (!ina226) return;

    // Read measurement, be robust to occasional 0 readings
    ina226->readAndClearFlags();
    float busPower = ina226->getBusPower();
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
    const float spikeThreshold = 0.2f; // 15%
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
    if (!ina226) return;
    ina226->readAndClearFlags();
    float currentValue = ina226->getCurrent_mA();
    float bus = ina226->getBusPower();
    logger->LogEvent("Current(mA): " + std::string(String(currentValue).c_str()));
     logger->LogEvent("Power: " + std::string(String(bus).c_str()));
    // logger->LogEvent("Interupt: " + std::string(String(currentAtLastInterupt).c_str()));
    // logger->LogEvent("InteruptCount: " + std::string(String(interuptCount).c_str()));
}

