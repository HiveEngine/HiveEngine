#include <core/Logger.h>
#include <core/Memory.h>
#include <core/Application.h>




int main()
{
    hive::Application app({.log_level = hive::Logger::LogLevel::INFO});


    LOG_INFO("Hello world %d", 10);

    hive::Memory::printMemoryUsage();

}
