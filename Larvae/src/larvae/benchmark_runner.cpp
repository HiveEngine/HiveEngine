#include <larvae/benchmark_registry.h>
#include <larvae/benchmark_runner.h>

#include <algorithm>
#include <iomanip>
#include <iostream>
#include <numeric>

namespace larvae
{
    BenchmarkRunner::BenchmarkRunner(BenchmarkConfig config)
        : m_config{std::move(config)}
    {
        if (!m_config.m_capabilitiesOverridden)
        {
            m_config.m_availableCapabilities = DetectBuildCapabilities();
        }
    }

    std::vector<BenchmarkInfo> BenchmarkRunner::CollectMatchingBenchmarks() const
    {
        std::vector<BenchmarkInfo> filtered_benchmarks;
        const auto& benchmarks = BenchmarkRegistry::GetInstance().GetBenchmarks();
        filtered_benchmarks.reserve(benchmarks.size());

        for (const auto& info : benchmarks)
        {
            const std::string full_name = std::string{info.m_suiteName} + "." + info.m_benchmarkName;
            if (MatchesFilter(full_name))
            {
                filtered_benchmarks.push_back(info);
            }
        }

        return filtered_benchmarks;
    }

    CapabilityMask BenchmarkRunner::GetMissingCapabilities(const BenchmarkInfo& benchmark_info) const
    {
        return benchmark_info.m_requiredCapabilities & ~m_config.m_availableCapabilities;
    }

    std::vector<BenchmarkResult> BenchmarkRunner::RunAll()
    {
        std::vector<BenchmarkResult> results;
        m_matchedBenchmarks = 0;
        m_skippedBenchmarks = 0;
        m_runnableBenchmarks = 0;

        const std::vector<BenchmarkInfo> benchmarks = CollectMatchingBenchmarks();
        m_matchedBenchmarks = benchmarks.size();

        if (m_config.m_listOnly)
        {
            PrintSelection(benchmarks);
            for (const auto& info : benchmarks)
            {
                if (GetMissingCapabilities(info) != 0)
                {
                    ++m_skippedBenchmarks;
                }
                else
                {
                    ++m_runnableBenchmarks;
                }
            }
            return results;
        }

        for (const auto& info : benchmarks)
        {
            if (!HasAllCapabilities(m_config.m_availableCapabilities, info.m_requiredCapabilities))
            {
                ++m_skippedBenchmarks;
                continue;
            }

            auto result = RunSingle(info.m_suiteName, info.m_benchmarkName, info.m_benchmarkFunc);
            results.push_back(result);
            ++m_runnableBenchmarks;
        }

        return results;
    }

    bool BenchmarkRunner::MatchesFilter(const std::string& full_name) const
    {
        const auto& pattern = m_config.m_filter;

        if (pattern == "*")
        {
            return true;
        }

        if (pattern.front() == '*' && pattern.back() == '*')
        {
            std::string substr = pattern.substr(1, pattern.length() - 2);
            return full_name.find(substr) != std::string::npos;
        }

        if (pattern.back() == '*')
        {
            std::string prefix = pattern.substr(0, pattern.length() - 1);
            return full_name.starts_with(prefix);
        }

        if (pattern.front() == '*')
        {
            std::string suffix = pattern.substr(1);
            return full_name.ends_with(suffix);
        }

        return full_name == pattern;
    }

    void BenchmarkRunner::PrintSelection(const std::vector<BenchmarkInfo>& benchmarks) const
    {
        std::cout << "Listing " << benchmarks.size() << " benchmark(s)\n";
        std::cout << "Available capabilities: " << FormatCapabilities(m_config.m_availableCapabilities) << "\n";

        for (const auto& benchmark : benchmarks)
        {
            const CapabilityMask missing = GetMissingCapabilities(benchmark);
            std::cout << (missing == 0 ? "[ RUNNABLE ] " : "[ SKIPPED  ] ") << benchmark.m_suiteName << "."
                      << benchmark.m_benchmarkName << " (requires: "
                      << FormatCapabilities(benchmark.m_requiredCapabilities) << ")";
            if (missing != 0)
            {
                std::cout << " (missing: " << FormatCapabilities(missing) << ")";
            }
            std::cout << "\n";
        }
    }

    BenchmarkResult BenchmarkRunner::RunSingle(const char* suite_name, const char* benchmark_name,
                                               std::function<void(BenchmarkState&)> benchmark_func)
    {
        for (size_t i = 0; i < m_config.m_warmupRuns; ++i)
        {
            BenchmarkState warmup_state{m_config.m_minIterations};
            benchmark_func(warmup_state);
        }

        size_t iterations = DetermineIterations(benchmark_func);

        std::vector<std::chrono::nanoseconds> times;
        times.reserve(m_config.m_repetitions);

        size_t bytes_processed = 0;
        size_t items_processed = 0;

        for (size_t rep = 0; rep < m_config.m_repetitions; ++rep)
        {
            BenchmarkState state{iterations};
            benchmark_func(state);

            times.push_back(state.GetElapsed());
            bytes_processed = state.GetBytesProcessed();
            items_processed = state.GetItemsProcessed();
        }

        std::sort(times.begin(), times.end());

        auto min_time = times.front();
        auto max_time = times.back();
        auto median_time = times[times.size() / 2];

        auto total = std::accumulate(times.begin(), times.end(), std::chrono::nanoseconds{0});
        auto mean_time = total / times.size();

        BenchmarkResult result{};
        result.m_suiteName = suite_name;
        result.m_benchmarkName = benchmark_name;
        result.m_iterations = iterations;
        result.m_minTime = min_time;
        result.m_maxTime = max_time;
        result.m_meanTime = mean_time;
        result.m_medianTime = median_time;

        if (bytes_processed > 0)
        {
            double seconds = static_cast<double>(median_time.count()) / 1e9;
            result.m_bytesPerSecond = static_cast<double>(bytes_processed) / seconds;
        }

        if (items_processed > 0)
        {
            double seconds = static_cast<double>(median_time.count()) / 1e9;
            result.m_itemsPerSecond = static_cast<double>(items_processed) / seconds;
        }

        return result;
    }

    size_t BenchmarkRunner::DetermineIterations(std::function<void(BenchmarkState&)> benchmark_func)
    {
        size_t iterations = m_config.m_minIterations;

        while (true)
        {
            BenchmarkState state{iterations};
            benchmark_func(state);

            if (state.GetElapsed() >= m_config.m_minTime)
            {
                break;
            }

            iterations *= 10;

            if (iterations > 1'000'000'000)
            {
                break;
            }
        }

        return iterations;
    }

    void PrintBenchmarkResults(const std::vector<BenchmarkResult>& results)
    {
        if (results.empty())
        {
            std::cout << "No benchmarks matched the filter.\n";
            return;
        }

        std::cout << "Running " << results.size() << " benchmark(s)...\n";
        std::cout << std::string(90, '-') << '\n';

        std::cout << std::left << std::setw(40) << "Benchmark" << std::right << std::setw(12) << "Time" << std::setw(12)
                  << "Min" << std::setw(12) << "Max" << std::setw(14) << "Iterations" << '\n';

        std::cout << std::string(90, '-') << '\n';

        for (const auto& result : results)
        {
            std::string full_name = std::string{result.m_suiteName} + "/" + result.m_benchmarkName;

            auto format_time = [](std::chrono::nanoseconds ns) -> std::string {
                double value = static_cast<double>(ns.count());
                const char* unit = "ns";

                if (value >= 1'000'000)
                {
                    value /= 1'000'000;
                    unit = "ms";
                }
                else if (value >= 1'000)
                {
                    value /= 1'000;
                    unit = "us";
                }

                std::ostringstream oss;
                oss << std::fixed << std::setprecision(1) << value << " " << unit;
                return oss.str();
            };

            std::cout << std::left << std::setw(40) << full_name << std::right << std::setw(12)
                      << format_time(result.m_medianTime) << std::setw(12) << format_time(result.m_minTime)
                      << std::setw(12) << format_time(result.m_maxTime) << std::setw(14) << result.m_iterations << '\n';

            if (result.m_bytesPerSecond > 0)
            {
                double mb_per_sec = result.m_bytesPerSecond / (1024 * 1024);
                std::cout << std::setw(40) << "" << std::right << "  Throughput: " << std::fixed << std::setprecision(2)
                          << mb_per_sec << " MB/s\n";
            }

            if (result.m_itemsPerSecond > 0)
            {
                std::cout << std::setw(40) << "" << std::right << "  Items/sec: " << std::fixed << std::setprecision(0)
                          << result.m_itemsPerSecond << '\n';
            }
        }

        std::cout << std::string(90, '-') << '\n';
    }

    BenchmarkConfig ParseBenchmarkCommandLine(int argc, char** argv)
    {
        BenchmarkConfig config{};
        config.m_availableCapabilities = DetectBuildCapabilities();

        for (int i = 1; i < argc; ++i)
        {
            std::string arg{argv[i]};

            if (arg.starts_with("--benchmark-filter="))
            {
                config.m_filter = arg.substr(19);
            }
            else if (arg == "--list")
            {
                config.m_listOnly = true;
            }
            else if (arg == "--fail-on-skip")
            {
                config.m_failOnSkip = true;
            }
            else if (arg.starts_with("--benchmark-min-time="))
            {
                std::string value = arg.substr(21);
                if (value.ends_with("ms"))
                {
                    int ms = std::stoi(value.substr(0, value.size() - 2));
                    config.m_minTime = std::chrono::milliseconds{ms};
                }
                else if (value.ends_with("s"))
                {
                    int s = std::stoi(value.substr(0, value.size() - 1));
                    config.m_minTime = std::chrono::milliseconds{s * 1000};
                }
            }
            else if (arg.starts_with("--benchmark-repetitions="))
            {
                config.m_repetitions = std::stoull(arg.substr(24));
            }
            else if (arg.starts_with("--capabilities="))
            {
                bool ok = false;
                const CapabilityMask parsed = ParseCapabilityList(arg.substr(15), &ok);
                if (ok)
                {
                    config.m_availableCapabilities = parsed;
                    config.m_capabilitiesOverridden = true;
                }
            }
        }

        return config;
    }
} // namespace larvae
