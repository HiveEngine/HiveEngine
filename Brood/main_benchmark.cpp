#include <larvae/larvae.h>
#include <hive/core/log.h>
#include <iostream>

int main(int argc, char** argv)
{
    hive::LogManager logManager;
    hive::ConsoleLogger logger{hive::LogManager::GetInstance()};

    std::cout << "Larvae Benchmark Runner\n\n";
    return larvae::RunAllBenchmarks(argc, argv);
}
