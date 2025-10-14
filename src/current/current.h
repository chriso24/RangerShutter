#ifndef CURRENT_H
#define CURRENT_H

#include "Arduino.h"
#include <INA226_WE.h>
#include "../logger/ILogger.h"

class Current {
public:
    enum CurrentLevel {
        C_VLOW,
        C_LOW,
        C_MEDIUM,
        C_HIGH
    };

    Current(ILogger* logger);
    ~Current();
    typedef void(*ShutdownInterup)(void);
    static void CurrentInterupt();

    void Init();
    
    void AttachInteruptForOverCurrent(ShutdownInterup callBack);
    void ContinuousSampling();
    void Reset();
    void CheckCurrent();
    void SetAccelartionActive(bool active);
    void StartMonitor(ShutdownInterup callBack, bool slowRun);
    void ShutdownMonitor();
    void SetCurrentLimit(CurrentLevel level, bool closing);
    void SetCurrentLimitPercentage(float percentage);
    void PrintCurrent();
    static void Loop(void* p_pParam);
    void RunMonitor();
    float GetBatteryPercentage();

    void AdjustCurrentLimits(bool increase);

private:
    ILogger* logger;
    static const int SHORT_WINDOW_SIZE = 3;
    static const int LONG_WINDOW_SIZE = 20;
    static const int LONG_WINDOW_COUNT = 10;
    static constexpr float ALERT_PERCENTAGE = 0.45;

    const int I2C_SDA = 21;
    const int I2C_SCL = 22;

    bool accelerationActive = false;

    bool wasRunning = false;
    CurrentLevel currentLevel;
    static ShutdownInterup callBack;
    static float currentAtLastInterupt;
    static volatile int interuptCount;
    static const int movingAverageSizeSmall = 4;
    static const int movingAverageSizeLarge = 10;
    static  float maxCurrentHigh;
    static  float maxCurrentLow ;
    static  float maxCurrentUltraLow;
    static  float maxCurrentUltraUltraLow ;
    static float currentLimitPercentage;
    static constexpr float minCurrentRead = 200.0;
    static INA226_WE* ina226;
    void ShutdownIna226();
    
    void StartupIna226();
    bool CheckInit();
    void SetCurrentValue(CurrentLevel level, bool closing);
    float currentMeasurements[SHORT_WINDOW_SIZE];
    float longCurrentMeasurements[LONG_WINDOW_SIZE];
    float shortAverageCurrent;
    float longAverageCurrent;
    bool isSlowRun;
    long currentMEasurementCounter = 0;
    float currentVoltage = 0.0f;
    TickType_t shutdownTime;
    TaskHandle_t Task1 = NULL;
    SemaphoreHandle_t i2cMutex;

    static constexpr float defaultMaxCurrentHigh = 850.0;
    static constexpr float defaultMaxCurrentLow = 780.0;
    static constexpr float defaultMaxCurrentUltraLow = 520.0;
    static constexpr float defaultMaxCurrentUltraUltraLow = 340.0; // This is fixed, the lowest current the motor draws while stalled
    static constexpr float defaultCurrentWatch = 0.25f;
};

#endif // CURRENT_H




