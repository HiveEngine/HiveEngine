#include <larvae/larvae.h>
#include <waggle/app.h>
#include <waggle/time.h>
#include <thread>
#include <cmath>

namespace {

    // =========================================================================
    // Construction
    // =========================================================================

    auto t_default_config = larvae::RegisterTest("Waggle", "App_default_config_inserts_resources", []() {
        waggle::App app;
        auto& world = app.GetWorld();

        auto* time = world.Resource<waggle::Time>();
        auto* fi = world.Resource<waggle::FrameInfo>();
        larvae::AssertTrue(time != nullptr);
        larvae::AssertTrue(fi != nullptr);
    });

    auto t_time_dt = larvae::RegisterTest("Waggle", "Time_dt_matches_config", []() {
        waggle::App app;
        auto* time = app.GetWorld().Resource<waggle::Time>();

        // Default: 60Hz = 16'666'667 ns
        larvae::AssertEqual(time->dt_ns, int64_t{16'666'667});
        larvae::AssertTrue(std::abs(time->dt - (1.f / 60.f)) < 1e-4f);
    });

    auto t_custom_config = larvae::RegisterTest("Waggle", "Custom_config_120Hz", []() {
        waggle::AppConfig cfg;
        cfg.fixed_dt_ns = 8'333'333; // 120Hz
        waggle::App app{cfg};

        auto* time = app.GetWorld().Resource<waggle::Time>();
        larvae::AssertEqual(time->dt_ns, int64_t{8'333'333});
    });

    auto t_initial_time_zero = larvae::RegisterTest("Waggle", "Initial_time_is_zero", []() {
        waggle::App app;
        auto* time = app.GetWorld().Resource<waggle::Time>();

        larvae::AssertEqual(time->elapsed_ns, int64_t{0});
        larvae::AssertTrue(time->elapsed == 0.f);
        larvae::AssertEqual(time->tick, uint64_t{0});
    });

    auto t_initial_frame_info = larvae::RegisterTest("Waggle", "Initial_frame_info_is_zero", []() {
        waggle::App app;
        auto* fi = app.GetWorld().Resource<waggle::FrameInfo>();

        larvae::AssertEqual(fi->real_dt_ns, int64_t{0});
        larvae::AssertEqual(fi->frame_count, uint64_t{0});
        larvae::AssertTrue(fi->alpha == 0.f);
    });

    // =========================================================================
    // Tick behavior
    // =========================================================================

    auto t_first_tick_returns_zero = larvae::RegisterTest("Waggle", "First_Tick_returns_zero", []() {
        waggle::App app;
        int32_t steps = app.Tick();
        larvae::AssertEqual(steps, 0);
    });

    auto t_tick_advances_time = larvae::RegisterTest("Waggle", "Tick_advances_simulation_time", []() {
        waggle::App app;
        app.Tick(); // first tick (reset)

        // Sleep enough for at least 1 fixed step (>16.67ms)
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
        int32_t steps = app.Tick();

        larvae::AssertTrue(steps >= 1);

        auto* time = app.GetWorld().Resource<waggle::Time>();
        larvae::AssertTrue(time->tick > 0);
        larvae::AssertTrue(time->elapsed_ns > 0);
        larvae::AssertTrue(time->elapsed > 0.f);
    });

    auto t_frame_count_increments = larvae::RegisterTest("Waggle", "FrameInfo_count_increments", []() {
        waggle::App app;
        app.Tick(); // first tick (reset, frame_count stays 0 in FrameClock)

        std::this_thread::sleep_for(std::chrono::milliseconds(5));
        app.Tick();

        auto* fi = app.GetWorld().Resource<waggle::FrameInfo>();
        larvae::AssertEqual(fi->frame_count, uint64_t{1});
    });

    auto t_multiple_ticks_accumulate = larvae::RegisterTest("Waggle", "Multiple_ticks_accumulate_time", []() {
        waggle::App app;
        app.Tick(); // first tick (reset)

        uint64_t total_steps = 0;
        for (int i = 0; i < 3; ++i)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
            total_steps += static_cast<uint64_t>(app.Tick());
        }

        auto* time = app.GetWorld().Resource<waggle::Time>();
        larvae::AssertEqual(time->tick, total_steps);
    });

    // =========================================================================
    // RequestStop
    // =========================================================================

    auto t_running_default = larvae::RegisterTest("Waggle", "IsRunning_default_true", []() {
        waggle::App app;
        larvae::AssertTrue(app.IsRunning());
    });

    auto t_request_stop = larvae::RegisterTest("Waggle", "RequestStop_sets_not_running", []() {
        waggle::App app;
        app.RequestStop();
        larvae::AssertTrue(!app.IsRunning());
    });

    // =========================================================================
    // Max substeps clamping
    // =========================================================================

    auto t_max_substeps = larvae::RegisterTest("Waggle", "Max_substeps_clamped", []() {
        waggle::AppConfig cfg;
        cfg.max_substeps = 2;
        waggle::App app{cfg};

        app.Tick(); // first tick (reset)

        // Sleep long enough for many steps, but clamp should limit to 2
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        int32_t steps = app.Tick();

        larvae::AssertTrue(steps <= 2);
    });

    // =========================================================================
    // Alpha interpolation
    // =========================================================================

    auto t_alpha_range = larvae::RegisterTest("Waggle", "Alpha_in_0_1_range", []() {
        waggle::App app;
        app.Tick(); // first tick (reset)

        std::this_thread::sleep_for(std::chrono::milliseconds(20));
        app.Tick();

        auto* fi = app.GetWorld().Resource<waggle::FrameInfo>();
        larvae::AssertTrue(fi->alpha >= 0.f);
        larvae::AssertTrue(fi->alpha <= 1.f);
    });

}
