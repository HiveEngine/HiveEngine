#include <larvae/benchmark_registry.h>

namespace larvae
{
    BenchmarkRegistry& BenchmarkRegistry::GetInstance()
    {
        static BenchmarkRegistry instance;
        return instance;
    }

    void BenchmarkRegistry::RegisterBenchmark(const char* suite_name, const char* benchmark_name,
                                              std::function<void(BenchmarkState&)> benchmark_func,
                                              CapabilityMask required_capabilities)
    {
        m_benchmarks.push_back({suite_name, benchmark_name, std::move(benchmark_func), required_capabilities});
    }
} // namespace larvae
