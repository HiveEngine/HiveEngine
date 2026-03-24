#include <waggle/app.h>

#include <hive/core/clock.h>
#include <hive/profiling/profiler.h>

#include <waggle/time.h>

#include <algorithm>

namespace waggle
{
    App::App(const AppConfig& config)
        : m_world{config.m_world}
        , m_config{config}
    {
        m_world.InsertResource(Time{hive::Clock::SecondsF(config.m_fixedDtNs), 0.f, config.m_fixedDtNs, 0, 0});

        m_world.InsertResource(FrameInfo{0.f, 0.f, 0, 0, 0, 0.f});
    }

    App::~App() = default;

    int32_t App::Tick()
    {
        HIVE_PROFILE_SCOPE_N("Waggle::Tick");

        // First tick: just reset the clock so the next Tick() has a valid delta
        if (m_firstTick)
        {
            m_frameClock.Reset();
            m_firstTick = false;
            UpdateFrameInfoResource();
            return 0;
        }

        m_frameClock.Tick();
        int64_t frameTime = (std::min)(m_frameClock.m_deltaNs, m_config.m_maxFrameTimeNs);
        m_accumulator += frameTime;

        // Fixed-rate simulation steps
        int32_t steps = 0;
        while (m_accumulator >= m_config.m_fixedDtNs && steps < m_config.m_maxSubsteps)
        {
            UpdateTimeResource();
            if (m_jobs.IsValid())
                m_world.AdvanceParallel(m_jobs);
            else
                m_world.Advance();

            m_accumulator -= m_config.m_fixedDtNs;
            m_simTime += m_config.m_fixedDtNs;
            ++m_simTick;
            ++steps;
        }

        UpdateFrameInfoResource();

        HIVE_PROFILE_FRAME;

        return steps;
    }

    void App::UpdateTimeResource()
    {
        Time* time = m_world.Resource<Time>();
        // dt is constant (set at construction)
        // elapsed/tick reflect the state AFTER this step completes
        time->m_elapsedNs = m_simTime + m_config.m_fixedDtNs;
        time->m_elapsed = hive::Clock::SecondsF(time->m_elapsedNs);
        time->m_tick = m_simTick + 1;
    }

    void App::UpdateFrameInfoResource()
    {
        FrameInfo* fi = m_world.Resource<FrameInfo>();
        fi->m_realDtNs = m_frameClock.m_deltaNs;
        fi->m_realDt = hive::Clock::SecondsF(m_frameClock.m_deltaNs);
        fi->m_realElapsedNs = m_frameClock.m_elapsedNs;
        fi->m_realElapsed = hive::Clock::SecondsF(m_frameClock.m_elapsedNs);
        fi->m_frameCount = m_frameClock.m_frameCount;
        fi->m_alpha =
            m_config.m_fixedDtNs > 0
                ? static_cast<float>(static_cast<double>(m_accumulator) / static_cast<double>(m_config.m_fixedDtNs))
                : 0.f;
    }
} // namespace waggle
