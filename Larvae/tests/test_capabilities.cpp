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
            config.m_suiteFilter = "LarvaeCapabilitiesSkip";
            config.m_availableCapabilities = larvae::ToMask(larvae::Capability::HEADLESS);
            config.m_capabilitiesOverridden = true;

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
            config.m_suiteFilter = "LarvaeCapabilitiesSkip";
            config.m_failOnSkip = true;
            config.m_availableCapabilities = larvae::ToMask(larvae::Capability::HEADLESS);
            config.m_capabilitiesOverridden = true;

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
            larvae::AssertTrue(config.m_capabilitiesOverridden);
            larvae::AssertEqual(config.m_availableCapabilities, larvae::CapabilityMask{0});

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
            larvae::AssertTrue(config.m_listOnly);
            larvae::AssertTrue(config.m_failOnSkip);
        });

    auto t_parse_command_line_exclusions =
        larvae::RegisterTest("LarvaeCapabilities", "parse_command_line_exclusions", []() {
            char arg0[] = "larvae_runner";
            char arg1[] = "--exclude-suite=LarvaeCapabilitiesSkip";
            char arg2[] = "--exclude-filter=*benchmark*";
            char* argv[] = {arg0, arg1, arg2};

            const larvae::TestRunnerConfig config = larvae::ParseCommandLine(static_cast<int>(std::size(argv)), argv);
            larvae::AssertStringEqual(config.m_excludeSuiteFilter, "LarvaeCapabilitiesSkip");
            larvae::AssertStringEqual(config.m_excludeFilterPattern, "*benchmark*");
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
            config.m_filter = "LarvaeCapabilitiesBenchmark*";
            config.m_minIterations = 1;
            config.m_warmupRuns = 0;
            config.m_repetitions = 1;
            config.m_minTime = std::chrono::milliseconds{0};
            config.m_availableCapabilities = larvae::ToMask(larvae::Capability::HEADLESS);
            config.m_capabilitiesOverridden = true;

            const std::vector<larvae::BenchmarkResult> results = larvae::BenchmarkRunner{config}.RunAll();

            larvae::AssertEqual(results.size(), size_t{1});
            larvae::AssertStringEqual(results.front().m_suiteName, "LarvaeCapabilitiesBenchmark");
            larvae::AssertStringEqual(results.front().m_benchmarkName, "headless_only");
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
            config.m_filter = "LarvaeCapabilitiesBenchmark*";
            config.m_listOnly = true;
            config.m_failOnSkip = true;
            config.m_availableCapabilities = larvae::ToMask(larvae::Capability::HEADLESS);
            config.m_capabilitiesOverridden = true;

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

            larvae::AssertTrue(config.m_capabilitiesOverridden);
            larvae::AssertEqual(config.m_availableCapabilities, larvae::CapabilityMask{0});
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

            larvae::AssertTrue(config.m_listOnly);
            larvae::AssertTrue(config.m_failOnSkip);
        });
} // namespace
