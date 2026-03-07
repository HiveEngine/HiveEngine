#include <larvae/larvae.h>
#include <hive/core/log.h>
#include <comb/debug/global_memory_tracker.h>
#include <iostream>

int main(int argc, char** argv)
{
    hive::LogManager logManager;
    hive::ConsoleLogger logger{hive::LogManager::GetInstance()};

    std::cout << "Larvae Benchmark Runner\n\n";
    int result = larvae::RunAllBenchmarks(argc, argv);
    comb::debug::ReportLiveAllocatorLeaks();
    return result;
}
