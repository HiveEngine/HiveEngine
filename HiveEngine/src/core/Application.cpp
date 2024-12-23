#include "Application.h"

#include <iostream>

void RegisterDefaultLoggerSync(hive::Logger::LogLevel level)
{
    hive::Logger::AddSync(level, [](hive::Logger::LogLevel level, const char* msg)
    {
       std::cout << msg << std::endl;
    });
}

hive::Application::Application(ApplicationConfig config) : memory_(), logger_()
{
    RegisterDefaultLoggerSync(config.log_level);
}
