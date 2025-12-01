#include <larvae/larvae.h>
#include <wax/containers/span.h>
#include <wax/containers/array.h>
#include <span>

namespace
{
    // =============================================================================
    // Wax::Span Benchmarks
    // =============================================================================

    auto bench1 = larvae::RegisterBenchmark("WaxSpan", "Iteration_100", [](larvae::BenchmarkState& state) {
        int data[100];
        for (int i = 0; i < 100; ++i) data[i] = i;
        wax::Span<int> span{data};

        while (state.KeepRunning())
        {
            int sum = 0;
            for (int val : span)
            {
                sum += val;
            }
            larvae::DoNotOptimize(sum);
        }

        state.SetItemsProcessed(state.iterations() * 100);
    });

    auto bench2 = larvae::RegisterBenchmark("WaxSpan", "Iteration_1000", [](larvae::BenchmarkState& state) {
        int data[1000];
        for (int i = 0; i < 1000; ++i) data[i] = i;
        wax::Span<int> span{data};

        while (state.KeepRunning())
        {
            int sum = 0;
            for (int val : span)
            {
                sum += val;
            }
            larvae::DoNotOptimize(sum);
        }

        state.SetItemsProcessed(state.iterations() * 1000);
    });

    auto bench3 = larvae::RegisterBenchmark("WaxSpan", "RandomAccess_100", [](larvae::BenchmarkState& state) {
        int data[100];
        for (int i = 0; i < 100; ++i) data[i] = i;
        wax::Span<int> span{data};

        while (state.KeepRunning())
        {
            for (size_t i = 0; i < 100; ++i)
            {
                int val = span[i];
                larvae::DoNotOptimize(val);
            }
        }

        state.SetItemsProcessed(state.iterations() * 100);
    });

    auto bench4 = larvae::RegisterBenchmark("WaxSpan", "RandomAccess_1000", [](larvae::BenchmarkState& state) {
        int data[1000];
        for (int i = 0; i < 1000; ++i) data[i] = i;
        wax::Span<int> span{data};

        while (state.KeepRunning())
        {
            for (size_t i = 0; i < 1000; ++i)
            {
                int val = span[i];
                larvae::DoNotOptimize(val);
            }
        }

        state.SetItemsProcessed(state.iterations() * 1000);
    });

    auto bench5 = larvae::RegisterBenchmark("WaxSpan", "Subspan_First", [](larvae::BenchmarkState& state) {
        int data[1000];
        for (int i = 0; i < 1000; ++i) data[i] = i;
        wax::Span<int> span{data};

        while (state.KeepRunning())
        {
            auto sub = span.First(100);
            int sum = 0;
            for (int val : sub)
            {
                sum += val;
            }
            larvae::DoNotOptimize(sum);
        }

        state.SetItemsProcessed(state.iterations() * 100);
    });

    auto bench6 = larvae::RegisterBenchmark("WaxSpan", "Subspan_Last", [](larvae::BenchmarkState& state) {
        int data[1000];
        for (int i = 0; i < 1000; ++i) data[i] = i;
        wax::Span<int> span{data};

        while (state.KeepRunning())
        {
            auto sub = span.Last(100);
            int sum = 0;
            for (int val : sub)
            {
                sum += val;
            }
            larvae::DoNotOptimize(sum);
        }

        state.SetItemsProcessed(state.iterations() * 100);
    });

    auto bench7 = larvae::RegisterBenchmark("WaxSpan", "Modification_100", [](larvae::BenchmarkState& state) {
        int data[100];
        wax::Span<int> span{data};

        while (state.KeepRunning())
        {
            for (int& val : span)
            {
                val++;
            }
            larvae::DoNotOptimize(span.Data());
        }

        state.SetItemsProcessed(state.iterations() * 100);
    });

    auto bench8 = larvae::RegisterBenchmark("WaxSpan", "Modification_1000", [](larvae::BenchmarkState& state) {
        int data[1000];
        wax::Span<int> span{data};

        while (state.KeepRunning())
        {
            for (int& val : span)
            {
                val++;
            }
            larvae::DoNotOptimize(span.Data());
        }

        state.SetItemsProcessed(state.iterations() * 1000);
    });

    auto bench9 = larvae::RegisterBenchmark("WaxSpan", "Construction", [](larvae::BenchmarkState& state) {
        int data[100];

        while (state.KeepRunning())
        {
            wax::Span<int> span{data};
            larvae::DoNotOptimize(span.Data());
        }

        state.SetItemsProcessed(state.iterations());
    });

    // =============================================================================
    // std::span Benchmarks (for comparison)
    // =============================================================================

    auto bench10 = larvae::RegisterBenchmark("StdSpan", "Iteration_100", [](larvae::BenchmarkState& state) {
        int data[100];
        for (int i = 0; i < 100; ++i) data[i] = i;
        std::span<int> span{data};

        while (state.KeepRunning())
        {
            int sum = 0;
            for (int val : span)
            {
                sum += val;
            }
            larvae::DoNotOptimize(sum);
        }

        state.SetItemsProcessed(state.iterations() * 100);
    });

    auto bench11 = larvae::RegisterBenchmark("StdSpan", "Iteration_1000", [](larvae::BenchmarkState& state) {
        int data[1000];
        for (int i = 0; i < 1000; ++i) data[i] = i;
        std::span<int> span{data};

        while (state.KeepRunning())
        {
            int sum = 0;
            for (int val : span)
            {
                sum += val;
            }
            larvae::DoNotOptimize(sum);
        }

        state.SetItemsProcessed(state.iterations() * 1000);
    });

    auto bench12 = larvae::RegisterBenchmark("StdSpan", "RandomAccess_100", [](larvae::BenchmarkState& state) {
        int data[100];
        for (int i = 0; i < 100; ++i) data[i] = i;
        std::span<int> span{data};

        while (state.KeepRunning())
        {
            for (size_t i = 0; i < 100; ++i)
            {
                int val = span[i];
                larvae::DoNotOptimize(val);
            }
        }

        state.SetItemsProcessed(state.iterations() * 100);
    });

    auto bench13 = larvae::RegisterBenchmark("StdSpan", "RandomAccess_1000", [](larvae::BenchmarkState& state) {
        int data[1000];
        for (int i = 0; i < 1000; ++i) data[i] = i;
        std::span<int> span{data};

        while (state.KeepRunning())
        {
            for (size_t i = 0; i < 1000; ++i)
            {
                int val = span[i];
                larvae::DoNotOptimize(val);
            }
        }

        state.SetItemsProcessed(state.iterations() * 1000);
    });

    auto bench14 = larvae::RegisterBenchmark("StdSpan", "Subspan_First", [](larvae::BenchmarkState& state) {
        int data[1000];
        for (int i = 0; i < 1000; ++i) data[i] = i;
        std::span<int> span{data};

        while (state.KeepRunning())
        {
            auto sub = span.first(100);
            int sum = 0;
            for (int val : sub)
            {
                sum += val;
            }
            larvae::DoNotOptimize(sum);
        }

        state.SetItemsProcessed(state.iterations() * 100);
    });

    auto bench15 = larvae::RegisterBenchmark("StdSpan", "Subspan_Last", [](larvae::BenchmarkState& state) {
        int data[1000];
        for (int i = 0; i < 1000; ++i) data[i] = i;
        std::span<int> span{data};

        while (state.KeepRunning())
        {
            auto sub = span.last(100);
            int sum = 0;
            for (int val : sub)
            {
                sum += val;
            }
            larvae::DoNotOptimize(sum);
        }

        state.SetItemsProcessed(state.iterations() * 100);
    });

    auto bench16 = larvae::RegisterBenchmark("StdSpan", "Modification_100", [](larvae::BenchmarkState& state) {
        int data[100];
        std::span<int> span{data};

        while (state.KeepRunning())
        {
            for (int& val : span)
            {
                val++;
            }
            larvae::DoNotOptimize(span.data());
        }

        state.SetItemsProcessed(state.iterations() * 100);
    });

    auto bench17 = larvae::RegisterBenchmark("StdSpan", "Modification_1000", [](larvae::BenchmarkState& state) {
        int data[1000];
        std::span<int> span{data};

        while (state.KeepRunning())
        {
            for (int& val : span)
            {
                val++;
            }
            larvae::DoNotOptimize(span.data());
        }

        state.SetItemsProcessed(state.iterations() * 1000);
    });

    auto bench18 = larvae::RegisterBenchmark("StdSpan", "Construction", [](larvae::BenchmarkState& state) {
        int data[100];

        while (state.KeepRunning())
        {
            std::span<int> span{data};
            larvae::DoNotOptimize(span.data());
        }

        state.SetItemsProcessed(state.iterations());
    });
}
