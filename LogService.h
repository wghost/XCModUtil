#ifndef LOGSERVICE_H
#define LOGSERVICE_H

#include <string>
#include <iostream>
#include <fstream>

#define DEFAULT_SENDER      ""
#define DEFAULT_LEVEL       ELogLevel::Message

#define _Log(x)             LogService::GetLog().Log(std::string(x))
#define _LogError(x,y)      LogService::GetLog().Log(std::string(x),std::string(y),ELogLevel::Error)
#define _LogWarn(x,y)       LogService::GetLog().Log(std::string(x),std::string(y),ELogLevel::Warn)
#define _LogDebug(x,y)      LogService::GetLog().Log(std::string(x),std::string(y),ELogLevel::Debug)
#define _SetLogLevel(x)     LogService::GetLog().LogLevel(ELogLevel(x))
#define _SetFilter(x)       LogService::GetLog().Filter(std::string(x))
#define _InitProgLog()      LogService::Init()
#define _InitConsoleLog()   ConsoleLog _ConLog; LogService::ProvideLog(&_ConLog)

/// require MySenderName() method
#define LogError(x)         LogService::GetLog().Log(std::string(x),MySenderName(),ELogLevel::Error)
#define LogWarn(x)          LogService::GetLog().Log(std::string(x),MySenderName(),ELogLevel::Warn)
#define LogDebug(x)         LogService::GetLog().Log(std::string(x),MySenderName(),ELogLevel::Debug)

enum class ELogLevel
{
    Error = 0,
    Warn,
    Message,
    Debug,
    All
};

/// base virtual interface class
class ProgLog
{
public:
    virtual ~ProgLog() {}
    virtual void Log(const std::string& message, const std::string& sender = DEFAULT_SENDER, const ELogLevel& level = DEFAULT_LEVEL) = 0;
    const ELogLevel& LogLevel() { return logLevel; }
    void LogLevel(const ELogLevel& aLevel) { logLevel = aLevel; }
    void Filter(const std::string& sender) { senderFilter = sender; }
protected:
    bool IsLoggingAllowed(const std::string& sender, const ELogLevel& level);
    ELogLevel logLevel = ELogLevel::All;
    std::string senderFilter = "";
};

/// null log
class NullLog: public ProgLog
{
public:
    virtual void Log(const std::string& message, const std::string& sender = DEFAULT_SENDER, const ELogLevel& level = DEFAULT_LEVEL)
    { /* do nothing */ }
};

/// console log
class ConsoleLog: public ProgLog
{
public:
    virtual void Log(const std::string& message, const std::string& sender = DEFAULT_SENDER, const ELogLevel& level = DEFAULT_LEVEL);
};

/// file log
class FileLog: public ProgLog
{
public:
    FileLog(const std::string& logName): logFile(logName) {}
    void OpenLog(const std::string& logName);
    void CloseLog();
    virtual void Log(const std::string& message, const std::string& sender = DEFAULT_SENDER, const ELogLevel& level = DEFAULT_LEVEL);
private:
    std::ofstream logFile;
};

class LogService
{
public:
    static void Init() { theLog = &theNullLog; }
    static ProgLog& GetLog() { return *theLog; }
    static void ProvideLog(ProgLog* aLog);
private:
    static ProgLog* theLog;
    static NullLog theNullLog;
};

/// helpers

std::string FormatLogLevel(const ELogLevel& level);
std::string FormatSender(const std::string& sender);

#endif // LOGSERVICE_H
