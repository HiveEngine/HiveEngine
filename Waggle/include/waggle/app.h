#pragma once

#include <queen/world/world.h>
#include <hive/core/clock.h>
#include <cstdint>

namespace waggle
{
    struct AppConfig
    {
        int64_t fixed_dt_ns{16'666'667};        // 60 Hz
        int64_t max_frame_time_ns{250'000'000}; // 250ms clamp (spiral of death)
        int32_t max_substeps{8};
        queen::WorldAllocatorConfig world{};
    };

    class App
    {
    public:
        explicit App(const AppConfig& config = {});
        ~App();

        App(const App&) = delete;
        App& operator=(const App&) = delete;
        App(App&&) = delete;
        App& operator=(App&&) = delete;

        [[nodiscard]] queen::World& GetWorld() noexcept { return world_; }
        [[nodiscard]] const queen::World& GetWorld() const noexcept { return world_; }

        // Call once per rendered frame.
        // Advances the FrameClock, accumulates time, runs World::Advance() for each
        // fixed step, updates Time/FrameInfo resources, emits HIVE_PROFILE_FRAME.
        // Returns the number of fixed steps taken this frame.
        int32_t Tick();

        [[nodiscard]] bool IsRunning() const noexcept { return running_; }
        void RequestStop() noexcept { running_ = false; }

    private:
        void UpdateTimeResource();
        void UpdateFrameInfoResource();

        queen::World world_;
        hive::FrameClock frame_clock_{};
        AppConfig config_;

        int64_t accumulator_{0};
        int64_t sim_time_{0};
        uint64_t sim_tick_{0};

        bool running_{true};
        bool first_tick_{true};
    };
}
