#include <larvae/larvae.h>

namespace
{
    auto bench_headless = larvae::RegisterBenchmarkWithCapabilities(
        "LarvaeCapabilities", "HeadlessOnly", larvae::ToMask(larvae::Capability::HEADLESS),
        [](larvae::BenchmarkState& state) {
            int value = 0;
            while (state.KeepRunning())
            {
                value += 1;
                larvae::DoNotOptimize(value);
            }
            state.SetItemsProcessed(state.Iterations());
        });

    auto bench_window = larvae::RegisterBenchmarkWithCapabilities(
        "LarvaeCapabilities", "WindowOnly", larvae::ToMask(larvae::Capability::WINDOW),
        [](larvae::BenchmarkState& state) {
            int value = 0;
            while (state.KeepRunning())
            {
                value += 2;
                larvae::DoNotOptimize(value);
            }
            state.SetItemsProcessed(state.Iterations());
        });
} // namespace
