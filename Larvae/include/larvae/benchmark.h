#pragma once

#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>

#if defined(_MSC_VER)
#include <intrin.h>
#endif

namespace larvae
{
    class BenchmarkState
    {
    public:
        BenchmarkState(size_t initial_iterations);

        bool KeepRunning();

        size_t Iterations() const
        {
            return m_iterations;
        }

        void SetBytesProcessed(size_t bytes)
        {
            m_bytesProcessed = bytes;
        }
        void SetItemsProcessed(size_t items)
        {
            m_itemsProcessed = items;
        }

        size_t GetBytesProcessed() const
        {
            return m_bytesProcessed;
        }
        size_t GetItemsProcessed() const
        {
            return m_itemsProcessed;
        }

        void StartTiming();
        void StopTiming();

        std::chrono::nanoseconds GetElapsed() const
        {
            return m_elapsed;
        }

    private:
        size_t m_iterations;
        size_t m_currentIteration{0};
        size_t m_bytesProcessed{0};
        size_t m_itemsProcessed{0};
        std::chrono::nanoseconds m_elapsed{0};
        std::chrono::high_resolution_clock::time_point m_startTime;
        bool m_isTiming{false};
    };

    struct BenchmarkResult
    {
        const char* m_suiteName;
        const char* m_benchmarkName;
        size_t m_iterations;
        std::chrono::nanoseconds m_minTime;
        std::chrono::nanoseconds m_maxTime;
        std::chrono::nanoseconds m_meanTime;
        std::chrono::nanoseconds m_medianTime;
        double m_bytesPerSecond{0.0};
        double m_itemsPerSecond{0.0};
    };

    template <typename T> inline void DoNotOptimize(T const& value)
    {
#if defined(_MSC_VER)
        const volatile void* volatile unused = &value;
        (void)unused;
#elif defined(__GNUC__) || defined(__clang__)
        asm volatile("" : : "r,m"(value) : "memory");
#else
        volatile auto unused = &value;
        (void)unused;
#endif
    }

    template <typename T> inline void DoNotOptimize(T& value)
    {
#if defined(_MSC_VER)
        volatile void* volatile unused = &value;
        (void)unused;
#elif defined(__clang__)
        asm volatile("" : "+r,m"(value) : : "memory");
#elif defined(__GNUC__)
        asm volatile("" : "+m,r"(value) : : "memory");
#else
        volatile auto unused = &value;
        (void)unused;
#endif
    }

    inline void ClobberMemory()
    {
#if defined(_MSC_VER)
        _ReadWriteBarrier();
#elif defined(__GNUC__) || defined(__clang__)
        asm volatile("" : : : "memory");
#else
        std::atomic_signal_fence(std::memory_order_acq_rel);
#endif
    }
} // namespace larvae
