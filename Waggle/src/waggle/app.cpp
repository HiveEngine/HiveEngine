#include <waggle/app.h>
#include <waggle/time.h>
#include <hive/core/clock.h>
#include <hive/profiling/profiler.h>
#include <algorithm>

namespace waggle
{
    App::App(const AppConfig& config)
        : world_{config.world}
        , config_{config}
    {
        world_.InsertResource(Time{
            hive::Clock::SecondsF(config_.fixed_dt_ns),
            0.f,
            config_.fixed_dt_ns,
            0,
            0
        });

        world_.InsertResource(FrameInfo{
            0.f, 0.f,
            0, 0,
            0,
            0.f
        });
    }

    App::~App() = default;

    int32_t App::Tick()
    {
        HIVE_PROFILE_SCOPE_N("Waggle::Tick");

        // First tick: just reset the clock so the next Tick() has a valid delta
        if (first_tick_)
        {
            frame_clock_.Reset();
            first_tick_ = false;
            UpdateFrameInfoResource();
            return 0;
        }

        frame_clock_.Tick();
        int64_t frame_time = (std::min)(frame_clock_.delta_ns, config_.max_frame_time_ns);
        accumulator_ += frame_time;

        // Fixed-rate simulation steps
        int32_t steps = 0;
        while (accumulator_ >= config_.fixed_dt_ns && steps < config_.max_substeps)
        {
            UpdateTimeResource();
            world_.Advance();

            accumulator_ -= config_.fixed_dt_ns;
            sim_time_ += config_.fixed_dt_ns;
            ++sim_tick_;
            ++steps;
        }

        UpdateFrameInfoResource();

        HIVE_PROFILE_FRAME;

        return steps;
    }

    void App::UpdateTimeResource()
    {
        Time* time = world_.Resource<Time>();
        // dt is constant (set at construction)
        // elapsed/tick reflect the state AFTER this step completes
        time->elapsed_ns = sim_time_ + config_.fixed_dt_ns;
        time->elapsed = hive::Clock::SecondsF(time->elapsed_ns);
        time->tick = sim_tick_ + 1;
    }

    void App::UpdateFrameInfoResource()
    {
        FrameInfo* fi = world_.Resource<FrameInfo>();
        fi->real_dt_ns = frame_clock_.delta_ns;
        fi->real_dt = hive::Clock::SecondsF(frame_clock_.delta_ns);
        fi->real_elapsed_ns = frame_clock_.elapsed_ns;
        fi->real_elapsed = hive::Clock::SecondsF(frame_clock_.elapsed_ns);
        fi->frame_count = frame_clock_.frame_count;
        fi->alpha = config_.fixed_dt_ns > 0
            ? static_cast<float>(static_cast<double>(accumulator_) / static_cast<double>(config_.fixed_dt_ns))
            : 0.f;
    }
}
