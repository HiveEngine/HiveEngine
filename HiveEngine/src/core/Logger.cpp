#include "Logger.h"
#include <cassert>
#include <cstdarg>
#include <cstdio>

static hive::Logger *s_logger = nullptr;
hive::Logger::Logger()
{
    assert(s_logger == nullptr);

    s_logger = this;
}

hive::Logger::~Logger()
{
    s_logger = nullptr;
}

void hive::Logger::LogOutput(LogLevel level, const char *msg, ...)
{
    assert(s_logger != nullptr);

    const char* levelStr = "";
    switch (level) {
        case LogLevel::DEBUG: levelStr = "DEBUG"; break;
        case LogLevel::INFO:  levelStr = "INFO";  break;
        case LogLevel::WARN:  levelStr = "WARN";  break;
        case LogLevel::ERROR: levelStr = "ERROR"; break;
        default: levelStr = "UNKNOWN"; break;
    }

    va_list args;
    va_start(args, msg);

    char buffer[1024 * 32];
    vsnprintf(buffer, sizeof(buffer), msg, args);
    va_end(args);

    char finalBuffer[1024 * 32];
    snprintf(finalBuffer, sizeof(finalBuffer), "[%s] %s", levelStr, buffer);

    for(int i = 0; i < s_logger->sync_function_count_; i++)
    {
        if(level >= s_logger->sync_functions_[i].level)
        {
            s_logger->sync_functions_[i].function(level, finalBuffer);

        }
    }
}

void hive::Logger::AddSync(LogLevel level, SyncFunction function)
{
    assert(s_logger != nullptr);
    s_logger->sync_functions_[s_logger->sync_function_count_++] = {function, level};
}

