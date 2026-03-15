#include <larvae/capabilities.h>
#include <larvae/larvae.h>

namespace
{
    auto t_requires_window =
        larvae::RegisterTestWithCapabilities("LarvaeCapabilitiesSkip", "requires_window",
                                             larvae::ToMask(larvae::Capability::WINDOW), []() {});

    auto t_parse_none = larvae::RegisterTest("LarvaeCapabilities", "parse_none", []() {
        bool ok = false;
        const larvae::CapabilityMask mask = larvae::ParseCapabilityList("none", &ok);
        larvae::AssertTrue(ok);
        larvae::AssertEqual(mask, larvae::CapabilityMask{0});
    });

    auto t_parse_list = larvae::RegisterTest("LarvaeCapabilities", "parse_list", []() {
        bool ok = false;
        const larvae::CapabilityMask mask = larvae::ParseCapabilityList("headless,window", &ok);
        larvae::AssertTrue(ok);
        larvae::AssertTrue(larvae::HasAllCapabilities(mask, larvae::Capability::HEADLESS | larvae::Capability::WINDOW));
        larvae::AssertFalse(larvae::HasAllCapabilities(mask, larvae::ToMask(larvae::Capability::RENDER)));
    });

    auto t_format = larvae::RegisterTest("LarvaeCapabilities", "format_mask", []() {
        const std::string formatted =
            larvae::FormatCapabilities(larvae::Capability::HEADLESS | larvae::Capability::RENDER);
        larvae::AssertTrue(formatted == "headless,render");
    });

    auto t_parse_auto = larvae::RegisterTest("LarvaeCapabilities", "parse_auto", []() {
        bool ok = false;
        const larvae::CapabilityMask parsed = larvae::ParseCapabilityList("auto", &ok);
        larvae::AssertTrue(ok);
        larvae::AssertEqual(parsed, larvae::DetectBuildCapabilities());
    });

    auto t_runner_skips_missing_capabilities =
        larvae::RegisterTest("LarvaeCapabilities", "runner_skips_missing_capabilities", []() {
            larvae::TestRunnerConfig config;
            config.suite_filter = "LarvaeCapabilitiesSkip";
            config.available_capabilities = larvae::ToMask(larvae::Capability::HEADLESS);
            config.capabilities_overridden = true;

            larvae::TestRunner runner{config};
            const int result = runner.Run();

            larvae::AssertEqual(result, 0);
            larvae::AssertEqual(runner.GetTotalTests(), 1);
            larvae::AssertEqual(runner.GetPassedTests(), 0);
            larvae::AssertEqual(runner.GetFailedTests(), 0);
            larvae::AssertEqual(runner.GetSkippedTests(), 1);
        });

    auto t_runner_fail_on_skip_returns_non_zero =
        larvae::RegisterTest("LarvaeCapabilities", "runner_fail_on_skip_returns_non_zero", []() {
            larvae::TestRunnerConfig config;
            config.suite_filter = "LarvaeCapabilitiesSkip";
            config.fail_on_skip = true;
            config.available_capabilities = larvae::ToMask(larvae::Capability::HEADLESS);
            config.capabilities_overridden = true;

            larvae::TestRunner runner{config};
            const int result = runner.Run();

            larvae::AssertEqual(result, 1);
            larvae::AssertEqual(runner.GetSkippedTests(), 1);
        });

    auto t_parse_command_line_none_preserves_override =
        larvae::RegisterTest("LarvaeCapabilities", "parse_command_line_none_preserves_override", []() {
            char arg0[] = "larvae_runner";
            char arg1[] = "--suite=LarvaeCapabilitiesSkip";
            char arg2[] = "--capabilities=none";
            char* argv[] = {arg0, arg1, arg2};

            const larvae::TestRunnerConfig config = larvae::ParseCommandLine(static_cast<int>(std::size(argv)), argv);
            larvae::AssertTrue(config.capabilities_overridden);
            larvae::AssertEqual(config.available_capabilities, larvae::CapabilityMask{0});

            larvae::TestRunner runner{config};
            const int result = runner.Run();

            larvae::AssertEqual(result, 0);
            larvae::AssertEqual(runner.GetSkippedTests(), 1);
        });

    auto t_parse_command_line_list_and_fail_on_skip =
        larvae::RegisterTest("LarvaeCapabilities", "parse_command_line_list_and_fail_on_skip", []() {
            char arg0[] = "larvae_runner";
            char arg1[] = "--list";
            char arg2[] = "--fail-on-skip";
            char* argv[] = {arg0, arg1, arg2};

            const larvae::TestRunnerConfig config = larvae::ParseCommandLine(static_cast<int>(std::size(argv)), argv);
            larvae::AssertTrue(config.list_only);
            larvae::AssertTrue(config.fail_on_skip);
        });

    auto t_benchmark_runner_filters_missing_capabilities =
        larvae::RegisterTest("LarvaeCapabilities", "benchmark_runner_filters_missing_capabilities", []() {
            auto& registry = larvae::BenchmarkRegistry::GetInstance();
            registry.Clear();

            registry.RegisterBenchmark("LarvaeCapabilitiesBenchmark", "headless_only",
                                       [](larvae::BenchmarkState& state) {
                                           while (state.KeepRunning())
                                           {
                                           }
                                       },
                                       larvae::ToMask(larvae::Capability::HEADLESS));
            registry.RegisterBenchmark("LarvaeCapabilitiesBenchmark", "window_only",
                                       [](larvae::BenchmarkState& state) {
                                           while (state.KeepRunning())
                                           {
                                           }
                                       },
                                       larvae::ToMask(larvae::Capability::WINDOW));

            larvae::BenchmarkConfig config;
            config.filter = "LarvaeCapabilitiesBenchmark*";
            config.min_iterations = 1;
            config.warmup_runs = 0;
            config.repetitions = 1;
            config.min_time = std::chrono::milliseconds{0};
            config.available_capabilities = larvae::ToMask(larvae::Capability::HEADLESS);
            config.capabilities_overridden = true;

            const std::vector<larvae::BenchmarkResult> results = larvae::BenchmarkRunner{config}.RunAll();

            larvae::AssertEqual(results.size(), size_t{1});
            larvae::AssertStringEqual(results.front().suite_name, "LarvaeCapabilitiesBenchmark");
            larvae::AssertStringEqual(results.front().benchmark_name, "headless_only");
        });

    auto t_benchmark_list_only_tracks_skips =
        larvae::RegisterTest("LarvaeCapabilities", "benchmark_list_only_tracks_skips", []() {
            auto& registry = larvae::BenchmarkRegistry::GetInstance();
            registry.Clear();

            registry.RegisterBenchmark("LarvaeCapabilitiesBenchmark", "window_only",
                                       [](larvae::BenchmarkState& state) {
                                           while (state.KeepRunning())
                                           {
                                           }
                                       },
                                       larvae::ToMask(larvae::Capability::WINDOW));

            larvae::BenchmarkConfig config;
            config.filter = "LarvaeCapabilitiesBenchmark*";
            config.list_only = true;
            config.fail_on_skip = true;
            config.available_capabilities = larvae::ToMask(larvae::Capability::HEADLESS);
            config.capabilities_overridden = true;

            larvae::BenchmarkRunner runner{config};
            const std::vector<larvae::BenchmarkResult> results = runner.RunAll();

            larvae::AssertTrue(results.empty());
            larvae::AssertEqual(runner.GetMatchedBenchmarks(), size_t{1});
            larvae::AssertEqual(runner.GetRunnableBenchmarks(), size_t{0});
            larvae::AssertEqual(runner.GetSkippedBenchmarks(), size_t{1});
        });

    auto t_parse_benchmark_command_line_none_preserves_override =
        larvae::RegisterTest("LarvaeCapabilities", "parse_benchmark_command_line_none_preserves_override", []() {
            auto& registry = larvae::BenchmarkRegistry::GetInstance();
            registry.Clear();

            registry.RegisterBenchmark("LarvaeCapabilitiesBenchmark", "headless_only",
                                       [](larvae::BenchmarkState& state) {
                                           while (state.KeepRunning())
                                           {
                                           }
                                       },
                                       larvae::ToMask(larvae::Capability::HEADLESS));

            char arg0[] = "larvae_benchmark";
            char arg1[] = "--benchmark-filter=LarvaeCapabilitiesBenchmark*";
            char arg2[] = "--capabilities=none";
            char* argv[] = {arg0, arg1, arg2};

            const larvae::BenchmarkConfig config =
                larvae::ParseBenchmarkCommandLine(static_cast<int>(std::size(argv)), argv);

            larvae::AssertTrue(config.capabilities_overridden);
            larvae::AssertEqual(config.available_capabilities, larvae::CapabilityMask{0});
            larvae::AssertTrue(larvae::BenchmarkRunner{config}.RunAll().empty());
        });

    auto t_parse_benchmark_command_line_list_and_fail_on_skip =
        larvae::RegisterTest("LarvaeCapabilities", "parse_benchmark_command_line_list_and_fail_on_skip", []() {
            char arg0[] = "larvae_benchmark";
            char arg1[] = "--list";
            char arg2[] = "--fail-on-skip";
            char* argv[] = {arg0, arg1, arg2};

            const larvae::BenchmarkConfig config =
                larvae::ParseBenchmarkCommandLine(static_cast<int>(std::size(argv)), argv);

            larvae::AssertTrue(config.list_only);
            larvae::AssertTrue(config.fail_on_skip);
        });
} // namespace
