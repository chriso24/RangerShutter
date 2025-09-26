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
    typedef void(*ShutdownInterup)(void);
    static void CurrentInterupt();

    void Init();
    
    void AttachInteruptForOverCurrent(ShutdownInterup callBack);
    void ContinuousSampling();
    void Reset();
    void CheckCurrent();
    void StartMonitor(ShutdownInterup callBack, bool slowRun);
    void ShutdownIna226();
    void SetCurrentLimit(CurrentLevel level, bool closing);
    void SetCurrentLimitPercentage(float percentage);
    void PrintCurrent();
    static void Loop(void* p_pParam);
    void EndThread();
    void RunMonitor();

private:
    ILogger* logger;
    static const int SHORT_WINDOW_SIZE = 3;
    static const int LONG_WINDOW_SIZE = 20;
    static const int LONG_WINDOW_COUNT = 10;
    static constexpr float ALERT_PERCENTAGE = 0.45;

    const int I2C_SDA = 21;
    const int I2C_SCL = 22;

    bool wasRunning = false;
    CurrentLevel currentLevel;
    static ShutdownInterup callBack;
    static float currentAtLastInterupt;
    static volatile int interuptCount;
    static const int movingAverageSizeSmall = 3;
    static const int movingAverageSizeLarge = 10;
    static constexpr float maxCurrentHigh = 850.0;
    static constexpr float maxCurrentLow = 780.0;
    static constexpr float maxCurrentUltraLow = 490.0;
    static constexpr float maxCurrentUltraUltraLow = 360.0;
    static constexpr float minCurrentRead = 200.0;
    static INA226_WE* ina226;
    void StartupIna226();
    bool CheckInit();
    void SetCurrentValue(CurrentLevel level, bool closing);
    float currentMeasurements[SHORT_WINDOW_SIZE];
    float longCurrentMeasurements[LONG_WINDOW_SIZE];
    float shortAverageCurrent;
    float longAverageCurrent;
    bool isSlowRun;
    long currentMEasurementCounter = 0;
    TickType_t shutdownTime;
    TaskHandle_t Task1;
};

#endif // CURRENT_H




