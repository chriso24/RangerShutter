#include "current.h"
#include "Arduino.h"
#include <INA226_WE.h>
#define I2C_ADDRESS 0x40

INA226_WE *Current::ina226;

volatile int Current::interuptCount;
Current::ShutdownInterup callBackOnOverCurrent;
float Current::currentAtLastInterupt;

void Current::Init()
{
    ina226 = new INA226_WE(I2C_ADDRESS);
    // callBackOnOverCurrent = callBack;
    Serial.println("Current Init");
    Wire.begin(I2C_SDA, I2C_SCL);
    ina226->init();

    // 25/06/2025 uncommenting to see if this helps. Have been running mostly well without
    // ina226->setAverage(AVERAGE_4);

    ina226->setResistorRange(1, 10.0);
    ina226->setMeasureMode(CONTINUOUS);
    ina226->setAlertType(POWER_OVER, maxCurrentLow);
    ina226->setAlertPinActiveHigh();
    // ina226->setConversionTime(CONV_TIME_140);
    // ina226->readAndClearFlags();
    // if (CheckInit())
    pinMode(2, INPUT_PULLUP);
    attachInterrupt(digitalPinToInterrupt(2), Current::CurrentInterupt, FALLING);
    // else
    //    Init();
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

    Serial.println("---------Start current monitor");
    AttachInteruptForOverCurrent(callBack);
    // bool wasRunning = AbortMovement();
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
    Serial.println("---------End current monitor");
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
    // Serial.print("Set current perdcentage: ");
    Serial.println(percentage);
    float current = ((float)(maxCurrentHigh - maxCurrentUltraLow) * percentage) + maxCurrentUltraLow;

    // Serial.print("Set current: ");
    Serial.println(current);

    ina226->setAlertType(POWER_OVER, current * 1.00);

    // ina226->readAndClearFlags();
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
        Serial.println("Init Failed!!!!");
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
    // if (currentLevel == level)
    // return;

    switch (level)
    {
    case CurrentLevel::C_HIGH:
        ina226->setAlertType(POWER_OVER, closing ? maxCurrentHigh * 1.05 : maxCurrentHigh);
        if (currentLevel != level)
            Serial.println("High");
        break;
    case CurrentLevel::C_MEDIUM:
        ina226->setAlertType(POWER_OVER, closing ? maxCurrentLow * 1.1 : maxCurrentLow);
        if (currentLevel != level)
            Serial.println(closing ? "Medium*1.5" : "Medium");
        break;
    case CurrentLevel::C_LOW:
        ina226->setAlertType(POWER_OVER, closing ? maxCurrentUltraLow * 1.1 : maxCurrentUltraLow);
        if (currentLevel != level)
            Serial.println(closing ? "Low*1.5" : "Low");
        break;
    case CurrentLevel::C_VLOW:
        ina226->setAlertType(POWER_OVER, maxCurrentUltraUltraLow);
        if (currentLevel != level)
            Serial.println("VeryLow");
        break;
    default:
        break;

        // ina226->readAndClearFlags();
    }

    currentLevel = level;
}

void Current::CheckCurrent()
{
    //Serial.println("CheckCurrent");
    currentMEasurementCounter++;
    ina226->readAndClearFlags();
    // float currentValue = ina226->getCurrent_mA();
    float bus = ina226->getBusPower();

        //Serial.println("Finish read CheckCurrent");
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
    // Serial.print("Count = ") ; Serial.println(averageTouch );
    shortAverageCurrent = newAvergae;
//Serial.print("shortAverageCurrent = ") ; Serial.println(shortAverageCurrent );
//Serial.print("bus = ") ; Serial.println(bus );

    float currentShift = ((bus - shortAverageCurrent) / shortAverageCurrent);

    if (currentMEasurementCounter % LONG_WINDOW_COUNT == 0)
    {
                Serial.print("Current Average: ");
        Serial.println(shortAverageCurrent);
        Serial.print("Long Average: ");
        Serial.println(longAverageCurrent);
        Serial.print("Current Shift:   ");
        Serial.println(currentShift);



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
             Serial.println("Long average is not changing enough. ABORT");
            callBackOnOverCurrent(); // Trigger the callback for over current
            }
            else
            Serial.println("Long average is not changing enough. But slow run, so ignore.");
        }

        longAverageCurrent = newLongAvergae;
    }

    //float adjustCurrentPercentage = (bus - )



    if (currentShift > ALERT_PERCENTAGE)
    {
        // ALERT
        Serial.println("ALERT: Current shift is too high!");
        callBackOnOverCurrent(); // Trigger the callback for over current
    }
   
}

void Current::PrintCurrent()
{
    ina226->readAndClearFlags();
    float currentValue = ina226->getCurrent_mA();
    float bus = ina226->getBusPower();
    Serial.print("Current: ");
    Serial.println(currentValue);
    Serial.print("Power: ");
    Serial.println(bus);
    Serial.print("Interupt: ");
    Serial.println(currentAtLastInterupt);
    Serial.print("InteruptCount: ");
    Serial.println(interuptCount);
}


void Current::ContinuousSampling()
{
    float shuntVoltage_mV = 0.0;
    float loadVoltage_V = 0.0;
    float busVoltage_V = 0.0;
    float current_mA = 0.0;
    float power_mW = 0.0;
    float res = 0.0;
    float pes = 0.0;

    ina226->readAndClearFlags();
    shuntVoltage_mV = ina226->getShuntVoltage_mV();
    busVoltage_V = ina226->getBusVoltage_V();
    current_mA = ina226->getCurrent_mA();
    power_mW = ina226->getBusPower();
    loadVoltage_V = busVoltage_V + (shuntVoltage_mV / 1000);

    Serial.print("Shunt Voltage [mV]: ");
    Serial.println(shuntVoltage_mV);
    Serial.print("Bus Voltage [V]: ");
    Serial.println(busVoltage_V);

    Serial.print("Load Voltage [V]: ");
    Serial.println(loadVoltage_V);
    Serial.print("Current[mA]: ");
    Serial.println(current_mA);
    res = busVoltage_V / 0.07;
    pes = res * 10;

    Serial.print("Bus Power [mW]: ");
    Serial.println(power_mW);
    if (!ina226->overflow)
    {
        Serial.println("Values OK - no overflow");
    }
    else
    {
        Serial.println("Overflow! Choose higher current range");
    }
    Serial.println();
}