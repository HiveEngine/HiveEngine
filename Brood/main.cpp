#include <larvae/larvae.h>

#include <hive/core/assert.h>
#include <hive/core/log.h>

int main(int argc, char** argv)
{
    hive::LogManager logManager;
    hive::ConsoleLogger logger{hive::LogManager::GetInstance()};

    return larvae::RunAllTests(argc, argv);
}
