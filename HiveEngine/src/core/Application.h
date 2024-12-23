#pragma once
#include "core/Memory.h"
#include "core/Logger.h"

namespace hive
{

    struct ApplicationConfig
    {
        Logger::LogLevel log_level;
    };


    class Application
    {
    public:
        explicit Application(ApplicationConfig config);


    private:
        Memory memory_;
        Logger logger_;
    };
}
