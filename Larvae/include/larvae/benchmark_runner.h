#pragma once

#include <larvae/benchmark.h>
#include <vector>
#include <string>

namespace larvae
{
    struct BenchmarkConfig
    {
        std::string filter{"*"};
        size_t min_iterations{10};
        size_t warmup_runs{3};
        std::chrono::milliseconds min_time{100};
        size_t repetitions{5};
    };

    class BenchmarkRunner
    {
    public:
        explicit BenchmarkRunner(BenchmarkConfig config = {});

        std::vector<BenchmarkResult> RunAll();

    private:
        bool MatchesFilter(const std::string& full_name) const;
        BenchmarkResult RunSingle(const char* suite_name,
                                  const char* benchmark_name,
                                  std::function<void(BenchmarkState&)> benchmark_func);

        size_t DetermineIterations(std::function<void(BenchmarkState&)> benchmark_func);

        BenchmarkConfig config_;
    };

    void PrintBenchmarkResults(const std::vector<BenchmarkResult>& results);
    BenchmarkConfig ParseBenchmarkCommandLine(int argc, char** argv);
}
