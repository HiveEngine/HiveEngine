#pragma once

#include <larvae/benchmark.h>
#include <larvae/benchmark_registry.h>
#include <larvae/capabilities.h>

#include <string>
#include <vector>

namespace larvae
{
    struct BenchmarkConfig
    {
        std::string filter{"*"};
        size_t min_iterations{10};
        size_t warmup_runs{3};
        std::chrono::milliseconds min_time{100};
        size_t repetitions{5};
        bool list_only = false;
        bool fail_on_skip = false;
        CapabilityMask available_capabilities{0};
        bool capabilities_overridden = false;
    };

    class BenchmarkRunner
    {
    public:
        explicit BenchmarkRunner(BenchmarkConfig config = {});

        std::vector<BenchmarkResult> RunAll();
        [[nodiscard]] size_t GetMatchedBenchmarks() const
        {
            return matched_benchmarks_;
        }
        [[nodiscard]] size_t GetSkippedBenchmarks() const
        {
            return skipped_benchmarks_;
        }
        [[nodiscard]] size_t GetRunnableBenchmarks() const
        {
            return runnable_benchmarks_;
        }

    private:
        [[nodiscard]] std::vector<BenchmarkInfo> CollectMatchingBenchmarks() const;
        [[nodiscard]] CapabilityMask GetMissingCapabilities(const BenchmarkInfo& benchmark_info) const;
        bool MatchesFilter(const std::string& full_name) const;
        void PrintSelection(const std::vector<BenchmarkInfo>& benchmarks) const;
        BenchmarkResult RunSingle(const char* suite_name, const char* benchmark_name,
                                  std::function<void(BenchmarkState&)> benchmark_func);

        size_t DetermineIterations(std::function<void(BenchmarkState&)> benchmark_func);

        BenchmarkConfig config_;
        size_t matched_benchmarks_{0};
        size_t skipped_benchmarks_{0};
        size_t runnable_benchmarks_{0};
    };

    void PrintBenchmarkResults(const std::vector<BenchmarkResult>& results);
    BenchmarkConfig ParseBenchmarkCommandLine(int argc, char** argv);
} // namespace larvae
