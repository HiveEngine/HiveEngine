#pragma once

#include <larvae/benchmark.h>
#include <functional>
#include <vector>
#include <string>

namespace larvae
{
    struct BenchmarkInfo
    {
        const char* suite_name;
        const char* benchmark_name;
        std::function<void(BenchmarkState&)> benchmark_func;
    };

    class BenchmarkRegistry
    {
    public:
        static BenchmarkRegistry& GetInstance();

        void RegisterBenchmark(const char* suite_name,
                              const char* benchmark_name,
                              std::function<void(BenchmarkState&)> benchmark_func);

        const std::vector<BenchmarkInfo>& GetBenchmarks() const { return benchmarks_; }

        void Clear() { benchmarks_.clear(); }

    private:
        BenchmarkRegistry() = default;
        std::vector<BenchmarkInfo> benchmarks_;
    };

    class BenchmarkRegistrar
    {
    public:
        BenchmarkRegistrar(const char* suite_name,
                          const char* benchmark_name,
                          std::function<void(BenchmarkState&)> benchmark_func)
        {
            BenchmarkRegistry::GetInstance().RegisterBenchmark(
                suite_name, benchmark_name, std::move(benchmark_func));
        }
    };

    inline BenchmarkRegistrar RegisterBenchmark(
        const char* suite_name,
        const char* benchmark_name,
        std::function<void(BenchmarkState&)> benchmark_func)
    {
        return {suite_name, benchmark_name, std::move(benchmark_func)};
    }
}
