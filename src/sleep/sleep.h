// sleep.h
#ifndef SLEEP_H
#define SLEEP_H

#ifndef __cplusplus
// If compiling as C, declare C linkage for callers
extern void GoToSleep(int sleepTimeInSeconds);
#else
// C++ declaration
void GoToSleep(int sleepTimeInSeconds,ILogger* logger);
#endif

#endif // SLEEP_H
