#include "LogService.h"

/// init static members
ProgLog* LogService::theLog = nullptr;
NullLog LogService::theNullLog = NullLog();

void LogService::ProvideLog(ProgLog* aLog)
{
    if (aLog == nullptr)
    {
        theLog = &theNullLog;
    }
    else
    {
        theLog = aLog;
    }
}

bool ProgLog::IsLoggingAllowed(const std::string& sender, const ELogLevel& level)
{
    if (level > logLevel || (senderFilter.length() > 0 && sender != senderFilter))
    {
        return false;
    }
    return true;
}

void ConsoleLog::Log(const std::string& message, const std::string& sender, const ELogLevel& level)
{
    if (!IsLoggingAllowed(sender, level))
    {
        return;
    }
    std::ostream* curStream;
    if (level == ELogLevel::Error)
    {
        curStream = &std::cerr;
    }
    else
    {
        curStream = &std::cout;
    }
    *curStream << FormatLogLevel(level) << FormatSender(sender) << message << std::endl;
}

void FileLog::OpenLog(const std::string& logName)
{
    CloseLog();
    logFile.open(logName.c_str());
}

void FileLog::CloseLog()
{
    if (logFile.is_open())
    {
        logFile.close();
    }
}

void FileLog::Log(const std::string& message, const std::string& sender, const ELogLevel& level)
{
    if (!logFile.is_open() || !IsLoggingAllowed(sender, level))
    {
        return;
    }
    logFile << FormatLogLevel(level) << FormatSender(sender) << message << std::endl;
}

/// helpers

std::string FormatLogLevel(const ELogLevel& level)
{
    switch (level)
    {
        case ELogLevel::Error:
            return "(Error) ";
        case ELogLevel::Warn:
            return "(Warning) ";
        case ELogLevel::Debug:
            return "(Debug) ";
        case ELogLevel::Message:
            return "";
        default:
            return "";
    }
}

std::string FormatSender(const std::string& sender)
{
    if (sender.length() > 0)
    {
        return sender + ": ";
    }
    return "";
}
