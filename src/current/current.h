#ifndef Current_h
#define Current_h
#include "Arduino.h" 
#include <INA226_WE.h>




class Current {
public:

enum CurrentLevel {
    C_VLOW ,
    C_LOW,
    C_MEDIUM,
    C_HIGH
};

typedef void(*ShutdownInterup)(void);
static void CurrentInterupt();

    void Init();

    void AttachInteruptForOverCurrent(ShutdownInterup callBack);
    
    void ContinuousSampling();
    // void Loop();
void Reset();
    // byte Stop(bool isRamping);
    // void Reset();

void CheckCurrent();
    void StartMonitor(ShutdownInterup callBack);
    void EndMonitor();

    void SetCurrentLimit(CurrentLevel level, bool closing);

    void SetCurrentLimitPercentage(float percentage);

void PrintCurrent();

static void Loop(void* p_pParam);

void EndThread();

    // const float MaxCurrentDuringRamp = 2000;
    // const float MaxCurrent = 500;
    // const float MinCurrentForStop = 50;

private:

static const int SHORT_WINDOW_SIZE = 3;
static const int LONG_WINDOW_COUNT = 10;
static constexpr float ALERT_PERCENTAGE = 0.10;

const int I2C_SDA = 21;
const int I2C_SCL = 22;

bool wasRunning = false;

Current::CurrentLevel currentLevel;
static ShutdownInterup callBack;
static float currentAtLastInterupt;
static volatile int interuptCount;

static const int movingAverageSizeSmall = 3;
static const int movingAverageSizeLarge = 10;
static constexpr float overCurrentPercentage = 0.35;

// static constexpr float maxCurrentHigh = 780.0;
// // 560 is the max value the motor will draw at the low speed when stalled.
// static constexpr float maxCurrentLow = 500.0;
// static constexpr float maxCurrentUltraLow = 440.0;
// static constexpr float maxCurrentUltraUltraLow = 340.0;


static constexpr float maxCurrentHigh = 780.0;
// 560 is the max value the motor will draw at the low speed when stalled.
static constexpr float maxCurrentLow = 780.0;
static constexpr float maxCurrentUltraLow = 780.0;
static constexpr float maxCurrentUltraUltraLow = 780.0;

    static INA226_WE* ina226;
    
    bool CheckInit();
    void SetCurrentValue(CurrentLevel level, bool closing);

    float currentMeasurements[SHORT_WINDOW_SIZE] ;
    float longCurrentMeasurements[SHORT_WINDOW_SIZE] ;
    float shortAverageCurrent;
    float longAverageCurrent;

    long currentMEasurementCounter = 0;


TaskHandle_t Task1;    

    // float lastReadings[movingAverageSizeLarge];
    // float lastValue;
    // int maxCurrentCount;
    // int readRateDuringRamp = 10;
    // int readRateSteady = 1;
    // TickType_t lastReadTime;
    
    // float movingAverageShort;
    // float movingAverageLong;
    // SemaphoreHandle_t xMutex = xSemaphoreCreateMutex();

    // void ContinuousSampling();
    // void RecordReading(float currentValue);
    // float GetLargeMovingAvergae();
    // float GetSmallMovingAvergae();
    // void  CheckCurrent();
    // // float GetShortMovingAverage();
    // // float GetLongMovingAverage();
    // void GetCurrentValues(float&,float&,float&);
};
#endif




