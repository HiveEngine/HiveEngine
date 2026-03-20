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
        std::string m_filter{"*"};
        size_t m_minIterations{10};
        size_t m_warmupRuns{3};
        std::chrono::milliseconds m_minTime{100};
        size_t m_repetitions{5};
        bool m_listOnly = false;
        bool m_failOnSkip = false;
        CapabilityMask m_availableCapabilities{0};
        bool m_capabilitiesOverridden = false;
    };

    class BenchmarkRunner
    {
    public:
        explicit BenchmarkRunner(BenchmarkConfig config = {});

        std::vector<BenchmarkResult> RunAll();
        [[nodiscard]] size_t GetMatchedBenchmarks() const
        {
            return m_matchedBenchmarks;
        }
        [[nodiscard]] size_t GetSkippedBenchmarks() const
        {
            return m_skippedBenchmarks;
        }
        [[nodiscard]] size_t GetRunnableBenchmarks() const
        {
            return m_runnableBenchmarks;
        }

    private:
        [[nodiscard]] std::vector<BenchmarkInfo> CollectMatchingBenchmarks() const;
        [[nodiscard]] CapabilityMask GetMissingCapabilities(const BenchmarkInfo& benchmark_info) const;
        bool MatchesFilter(const std::string& full_name) const;
        void PrintSelection(const std::vector<BenchmarkInfo>& benchmarks) const;
        BenchmarkResult RunSingle(const char* suite_name, const char* benchmark_name,
                                  std::function<void(BenchmarkState&)> benchmark_func);

        size_t DetermineIterations(std::function<void(BenchmarkState&)> benchmark_func);

        BenchmarkConfig m_config;
        size_t m_matchedBenchmarks{0};
        size_t m_skippedBenchmarks{0};
        size_t m_runnableBenchmarks{0};
    };

    void PrintBenchmarkResults(const std::vector<BenchmarkResult>& results);
    BenchmarkConfig ParseBenchmarkCommandLine(int argc, char** argv);
} // namespace larvae
