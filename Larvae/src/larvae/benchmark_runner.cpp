#include <larvae/benchmark_runner.h>
#include <larvae/benchmark_registry.h>
#include <algorithm>
#include <iostream>
#include <iomanip>
#include <numeric>

namespace larvae
{
    BenchmarkRunner::BenchmarkRunner(BenchmarkConfig config)
        : config_{std::move(config)}
    {
    }

    std::vector<BenchmarkResult> BenchmarkRunner::RunAll()
    {
        std::vector<BenchmarkResult> results;
        const auto& benchmarks = BenchmarkRegistry::GetInstance().GetBenchmarks();

        for (const auto& info : benchmarks)
        {
            std::string full_name = std::string{info.suite_name} + "." + info.benchmark_name;

            if (!MatchesFilter(full_name))
            {
                continue;
            }

            auto result = RunSingle(info.suite_name, info.benchmark_name, info.benchmark_func);
            results.push_back(result);
        }

        return results;
    }

    bool BenchmarkRunner::MatchesFilter(const std::string& full_name) const
    {
        const auto& pattern = config_.filter;

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

    BenchmarkResult BenchmarkRunner::RunSingle(const char* suite_name,
                                               const char* benchmark_name,
                                               std::function<void(BenchmarkState&)> benchmark_func)
    {
        for (size_t i = 0; i < config_.warmup_runs; ++i)
        {
            BenchmarkState warmup_state{config_.min_iterations};
            benchmark_func(warmup_state);
        }

        size_t iterations = DetermineIterations(benchmark_func);

        std::vector<std::chrono::nanoseconds> times;
        times.reserve(config_.repetitions);

        size_t bytes_processed = 0;
        size_t items_processed = 0;

        for (size_t rep = 0; rep < config_.repetitions; ++rep)
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
        result.suite_name = suite_name;
        result.benchmark_name = benchmark_name;
        result.iterations = iterations;
        result.min_time = min_time;
        result.max_time = max_time;
        result.mean_time = mean_time;
        result.median_time = median_time;

        if (bytes_processed > 0)
        {
            double seconds = static_cast<double>(median_time.count()) / 1e9;
            result.bytes_per_second = static_cast<double>(bytes_processed) / seconds;
        }

        if (items_processed > 0)
        {
            double seconds = static_cast<double>(median_time.count()) / 1e9;
            result.items_per_second = static_cast<double>(items_processed) / seconds;
        }

        return result;
    }

    size_t BenchmarkRunner::DetermineIterations(std::function<void(BenchmarkState&)> benchmark_func)
    {
        size_t iterations = config_.min_iterations;

        while (true)
        {
            BenchmarkState state{iterations};
            benchmark_func(state);

            if (state.GetElapsed() >= config_.min_time)
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

        std::cout << std::left
                  << std::setw(40) << "Benchmark"
                  << std::right
                  << std::setw(12) << "Time"
                  << std::setw(12) << "Min"
                  << std::setw(12) << "Max"
                  << std::setw(14) << "Iterations"
                  << '\n';

        std::cout << std::string(90, '-') << '\n';

        for (const auto& result : results)
        {
            std::string full_name = std::string{result.suite_name} + "/" + result.benchmark_name;

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

            std::cout << std::left
                      << std::setw(40) << full_name
                      << std::right
                      << std::setw(12) << format_time(result.median_time)
                      << std::setw(12) << format_time(result.min_time)
                      << std::setw(12) << format_time(result.max_time)
                      << std::setw(14) << result.iterations
                      << '\n';

            if (result.bytes_per_second > 0)
            {
                double mb_per_sec = result.bytes_per_second / (1024 * 1024);
                std::cout << std::setw(40) << ""
                          << std::right
                          << "  Throughput: " << std::fixed << std::setprecision(2)
                          << mb_per_sec << " MB/s\n";
            }

            if (result.items_per_second > 0)
            {
                std::cout << std::setw(40) << ""
                          << std::right
                          << "  Items/sec: " << std::fixed << std::setprecision(0)
                          << result.items_per_second << '\n';
            }
        }

        std::cout << std::string(90, '-') << '\n';
    }

    BenchmarkConfig ParseBenchmarkCommandLine(int argc, char** argv)
    {
        BenchmarkConfig config{};

        for (int i = 1; i < argc; ++i)
        {
            std::string arg{argv[i]};

            if (arg.starts_with("--benchmark-filter="))
            {
                config.filter = arg.substr(19);
            }
            else if (arg.starts_with("--benchmark-min-time="))
            {
                std::string value = arg.substr(21);
                if (value.ends_with("ms"))
                {
                    int ms = std::stoi(value.substr(0, value.size() - 2));
                    config.min_time = std::chrono::milliseconds{ms};
                }
                else if (value.ends_with("s"))
                {
                    int s = std::stoi(value.substr(0, value.size() - 1));
                    config.min_time = std::chrono::milliseconds{s * 1000};
                }
            }
            else if (arg.starts_with("--benchmark-repetitions="))
            {
                config.repetitions = std::stoull(arg.substr(24));
            }
        }

        return config;
    }
}
