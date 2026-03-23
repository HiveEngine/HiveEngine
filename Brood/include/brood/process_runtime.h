#pragma once

#include <hive/core/log.h>
#include <hive/profiling/profiler.h>

#if HIVE_FEATURE_MEM_DEBUG
#include <comb/debug/global_memory_tracker.h>
#endif

#include <cstdio>
#include <cstdlib>

namespace brood
{
    class ProcessRuntime
    {
    public:
        ProcessRuntime()
            : m_logger{m_logManager}
        {
        }

        ~ProcessRuntime()
        {
            Finalize();
        }

        void Finalize()
        {
            if (m_finalized)
            {
                return;
            }

#if COMB_MEM_DEBUG
            comb::debug::ReportLiveAllocatorLeaks();
#endif
            hive::ShutdownProfiler();
            std::fflush(stdout);
            std::fflush(stderr);
            m_finalized = true;
        }

        [[noreturn]] void Exit(int exitCode)
        {
            Finalize();
            std::_Exit(exitCode);
        }

    private:
        hive::LogManager m_logManager;
        hive::ConsoleLogger m_logger;
        bool m_finalized{false};
    };
} // namespace brood
