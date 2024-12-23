#pragma once

#include <hvpch.h>
namespace hive
{
    //The Logger's job is to hold all the sync we could think of (File Sync, Console Sync, etc) and call
    //them according to the level they registered
    class Logger
    {
    public:
        enum class LogLevel
        {
            DEBUG, INFO, WARN, ERROR
        };

        Logger();
        ~Logger();

        static void LogOutput(LogLevel level, const char* msg, ...);

        using SyncFunction = void(*)(LogLevel level, const char *msg);

        static void AddSync(LogLevel level, SyncFunction function);

    private:
        struct SyncFunctionData
        {
            SyncFunction function;
            LogLevel level;
        } ;

        SyncFunctionData sync_functions_[8]{};
        u8 sync_function_count_ = 0;
    };



}

#define LOG_DEBUG(msg, ...) hive::Logger::LogOutput(hive::Logger::LogLevel::DEBUG, msg, ##__VA_ARGS__);
#define LOG_INFO(msg, ...) hive::Logger::LogOutput(hive::Logger::LogLevel::INFO, msg, ##__VA_ARGS__);
#define LOG_WARN(msg, ...) hive::Logger::LogOutput(hive::Logger::LogLevel::WARN, msg, ##__VA_ARGS__);
#define LOG_ERROR(msg, ...) hive::Logger::LogOutput(hive::Logger::LogLevel::ERROR, msg, ##__VA_ARGS__);
