#pragma once

#include <larvae/benchmark.h>
#include <larvae/capabilities.h>

#include <functional>
#include <string>
#include <vector>

namespace larvae
{
    struct BenchmarkInfo
    {
        const char* m_suiteName;
        const char* m_benchmarkName;
        std::function<void(BenchmarkState&)> m_benchmarkFunc;
        CapabilityMask m_requiredCapabilities{0};
    };

    class BenchmarkRegistry
    {
    public:
        static BenchmarkRegistry& GetInstance();

        void RegisterBenchmark(const char* suite_name, const char* benchmark_name,
                               std::function<void(BenchmarkState&)> benchmark_func,
                               CapabilityMask required_capabilities = 0);

        const std::vector<BenchmarkInfo>& GetBenchmarks() const
        {
            return m_benchmarks;
        }

        void Clear()
        {
            m_benchmarks.clear();
        }

    private:
        BenchmarkRegistry() = default;
        std::vector<BenchmarkInfo> m_benchmarks;
    };

    class BenchmarkRegistrar
    {
    public:
        BenchmarkRegistrar(const char* suite_name, const char* benchmark_name,
                           std::function<void(BenchmarkState&)> benchmark_func, CapabilityMask required_capabilities = 0)
        {
            BenchmarkRegistry::GetInstance().RegisterBenchmark(suite_name, benchmark_name, std::move(benchmark_func),
                                                               required_capabilities);
        }
    };

    inline BenchmarkRegistrar RegisterBenchmark(const char* suite_name, const char* benchmark_name,
                                                std::function<void(BenchmarkState&)> benchmark_func)
    {
        return {suite_name, benchmark_name, std::move(benchmark_func)};
    }

    inline BenchmarkRegistrar RegisterBenchmarkWithCapabilities(const char* suite_name, const char* benchmark_name,
                                                                CapabilityMask required_capabilities,
                                                                std::function<void(BenchmarkState&)> benchmark_func)
    {
        return {suite_name, benchmark_name, std::move(benchmark_func), required_capabilities};
    }
} // namespace larvae
