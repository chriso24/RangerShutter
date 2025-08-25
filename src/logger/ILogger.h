#ifndef ILOGGER_H
#define ILOGGER_H

#include <string>

class ILogger {
public:
    virtual ~ILogger() {}
    virtual void LogEvent(const std::string& message) = 0;
};

#endif // ILOGGER_H
