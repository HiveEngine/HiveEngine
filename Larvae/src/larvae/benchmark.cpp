#include <larvae/benchmark.h>

namespace larvae
{
    BenchmarkState::BenchmarkState(size_t initial_iterations)
        : m_iterations{initial_iterations}
    {
    }

    bool BenchmarkState::KeepRunning()
    {
        if (m_currentIteration == 0)
        {
            StartTiming();
        }

        if (m_currentIteration >= m_iterations)
        {
            StopTiming();
            return false;
        }

        ++m_currentIteration;
        return true;
    }

    void BenchmarkState::StartTiming()
    {
        if (!m_isTiming)
        {
            m_startTime = std::chrono::high_resolution_clock::now();
            m_isTiming = true;
        }
    }

    void BenchmarkState::StopTiming()
    {
        if (m_isTiming)
        {
            auto end_time = std::chrono::high_resolution_clock::now();
            m_elapsed = std::chrono::duration_cast<std::chrono::nanoseconds>(end_time - m_startTime);
            m_isTiming = false;
        }
    }
} // namespace larvae
