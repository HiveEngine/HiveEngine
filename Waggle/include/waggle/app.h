#pragma once

#include <hive/core/clock.h>

#include <queen/world/world.h>

#include <cstdint>

namespace waggle
{
    struct AppConfig
    {
        int64_t m_fixedDtNs{16'666'667};       // 60 Hz
        int64_t m_maxFrameTimeNs{250'000'000}; // 250ms clamp (spiral of death)
        int32_t m_maxSubsteps{8};
        queen::WorldAllocatorConfig m_world{};
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

        [[nodiscard]] queen::World& GetWorld() noexcept { return m_world; }
        [[nodiscard]] const queen::World& GetWorld() const noexcept { return m_world; }

        // Call once per rendered frame.
        // Advances the FrameClock, accumulates time, runs World::Advance() for each
        // fixed step, updates Time/FrameInfo resources, emits HIVE_PROFILE_FRAME.
        // Returns the number of fixed steps taken this frame.
        int32_t Tick();

        [[nodiscard]] bool IsRunning() const noexcept { return m_running; }
        void RequestStop() noexcept { m_running = false; }

    private:
        void UpdateTimeResource();
        void UpdateFrameInfoResource();

        queen::World m_world;
        hive::FrameClock m_frameClock{};
        AppConfig m_config;

        int64_t m_accumulator{0};
        int64_t m_simTime{0};
        uint64_t m_simTick{0};

        bool m_running{true};
        bool m_firstTick{true};
    };
} // namespace waggle
