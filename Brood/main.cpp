#include <hive/core/assert.h>
#include <hive/core/log.h>

#include <comb/debug/global_memory_tracker.h>

#include <larvae/larvae.h>

int main(int argc, char** argv) {
    hive::LogManager logManager;
    hive::ConsoleLogger logger{hive::LogManager::GetInstance()};

    int result = larvae::RunAllTests(argc, argv);
    comb::debug::ReportLiveAllocatorLeaks();
    return result;
}
