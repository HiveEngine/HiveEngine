#pragma once

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <atomic>

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

        size_t iterations() const { return iterations_; }

        void SetBytesProcessed(size_t bytes) { bytes_processed_ = bytes; }
        void SetItemsProcessed(size_t items) { items_processed_ = items; }

        size_t GetBytesProcessed() const { return bytes_processed_; }
        size_t GetItemsProcessed() const { return items_processed_; }

        void StartTiming();
        void StopTiming();

        std::chrono::nanoseconds GetElapsed() const { return elapsed_; }

    private:
        size_t iterations_;
        size_t current_iteration_{0};
        size_t bytes_processed_{0};
        size_t items_processed_{0};
        std::chrono::nanoseconds elapsed_{0};
        std::chrono::high_resolution_clock::time_point start_time_;
        bool is_timing_{false};
    };

    struct BenchmarkResult
    {
        const char* suite_name;
        const char* benchmark_name;
        size_t iterations;
        std::chrono::nanoseconds min_time;
        std::chrono::nanoseconds max_time;
        std::chrono::nanoseconds mean_time;
        std::chrono::nanoseconds median_time;
        double bytes_per_second{0.0};
        double items_per_second{0.0};
    };

    template<typename T>
    inline void DoNotOptimize(T const& value)
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

    template<typename T>
    inline void DoNotOptimize(T& value)
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
}
