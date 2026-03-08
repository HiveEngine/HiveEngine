#pragma once

#include <hive/core/log.h>
#include <hive/profiling/profiler.h>

#if HIVE_FEATURE_MEM_DEBUG
#include <comb/debug/global_memory_tracker.h>
#endif

#include <cstdio>
#include <cstdlib>
#include <iostream>

namespace brood
{
    class ProcessRuntime
    {
    public:
        ProcessRuntime()
            : m_logger_{m_logManager_}
        {
        }

        ~ProcessRuntime()
        {
            Finalize();
        }

        void Finalize()
        {
            if (m_finalized_)
            {
                return;
            }

#if HIVE_FEATURE_MEM_DEBUG
            comb::debug::ReportLiveAllocatorLeaks();
#endif
            hive::ShutdownProfiler();
            std::cout.flush();
            std::cerr.flush();
            std::fflush(stdout);
            std::fflush(stderr);
            m_finalized_ = true;
        }

        [[noreturn]] void Exit(int exitCode)
        {
            Finalize();
            std::_Exit(exitCode);
        }

    private:
        hive::LogManager m_logManager_;
        hive::ConsoleLogger m_logger_;
        bool m_finalized_{false};
    };
} // namespace brood
