#include <larvae/benchmark.h>

namespace larvae
{
    BenchmarkState::BenchmarkState(size_t initial_iterations)
        : iterations_{initial_iterations}
    {
    }

    bool BenchmarkState::KeepRunning()
    {
        if (current_iteration_ == 0)
        {
            StartTiming();
        }

        if (current_iteration_ >= iterations_)
        {
            StopTiming();
            return false;
        }

        ++current_iteration_;
        return true;
    }

    void BenchmarkState::StartTiming()
    {
        if (!is_timing_)
        {
            start_time_ = std::chrono::high_resolution_clock::now();
            is_timing_ = true;
        }
    }

    void BenchmarkState::StopTiming()
    {
        if (is_timing_)
        {
            auto end_time = std::chrono::high_resolution_clock::now();
            elapsed_ = std::chrono::duration_cast<std::chrono::nanoseconds>(end_time - start_time_);
            is_timing_ = false;
        }
    }
}
