#include <larvae/larvae.h>
#include <wax/containers/array.h>
#include <array>

namespace
{
    // =============================================================================
    // Wax::Array Benchmarks
    // =============================================================================

    auto bench1 = larvae::RegisterBenchmark("WaxArray", "Iteration_100", [](larvae::BenchmarkState& state) {
        wax::Array<int, 100> arr = {};
        arr.Fill(42);

        while (state.KeepRunning())
        {
            int sum = 0;
            for (int val : arr)
            {
                sum += val;
            }
            larvae::DoNotOptimize(sum);
        }

        state.SetItemsProcessed(state.iterations() * 100);
    });

    auto bench2 = larvae::RegisterBenchmark("WaxArray", "Iteration_1000", [](larvae::BenchmarkState& state) {
        wax::Array<int, 1000> arr = {};
        arr.Fill(42);

        while (state.KeepRunning())
        {
            int sum = 0;
            for (int val : arr)
            {
                sum += val;
            }
            larvae::DoNotOptimize(sum);
        }

        state.SetItemsProcessed(state.iterations() * 1000);
    });

    auto bench3 = larvae::RegisterBenchmark("WaxArray", "RandomAccess_100", [](larvae::BenchmarkState& state) {
        wax::Array<int, 100> arr = {};
        arr.Fill(42);

        while (state.KeepRunning())
        {
            for (size_t i = 0; i < 100; ++i)
            {
                int val = arr[i];
                larvae::DoNotOptimize(val);
            }
        }

        state.SetItemsProcessed(state.iterations() * 100);
    });

    auto bench4 = larvae::RegisterBenchmark("WaxArray", "RandomAccess_1000", [](larvae::BenchmarkState& state) {
        wax::Array<int, 1000> arr = {};
        arr.Fill(42);

        while (state.KeepRunning())
        {
            for (size_t i = 0; i < 1000; ++i)
            {
                int val = arr[i];
                larvae::DoNotOptimize(val);
            }
        }

        state.SetItemsProcessed(state.iterations() * 1000);
    });

    auto bench5 = larvae::RegisterBenchmark("WaxArray", "Fill_100", [](larvae::BenchmarkState& state) {
        wax::Array<int, 100> arr = {};

        while (state.KeepRunning())
        {
            arr.Fill(42);
            larvae::DoNotOptimize(arr.Data());
        }

        state.SetItemsProcessed(state.iterations() * 100);
    });

    auto bench6 = larvae::RegisterBenchmark("WaxArray", "Fill_1000", [](larvae::BenchmarkState& state) {
        wax::Array<int, 1000> arr = {};

        while (state.KeepRunning())
        {
            arr.Fill(42);
            larvae::DoNotOptimize(arr.Data());
        }

        state.SetItemsProcessed(state.iterations() * 1000);
    });

    auto bench7 = larvae::RegisterBenchmark("WaxArray", "Modification_100", [](larvae::BenchmarkState& state) {
        wax::Array<int, 100> arr = {};

        while (state.KeepRunning())
        {
            for (int& val : arr)
            {
                val++;
            }
            larvae::DoNotOptimize(arr.Data());
        }

        state.SetItemsProcessed(state.iterations() * 100);
    });

    auto bench8 = larvae::RegisterBenchmark("WaxArray", "Modification_1000", [](larvae::BenchmarkState& state) {
        wax::Array<int, 1000> arr = {};

        while (state.KeepRunning())
        {
            for (int& val : arr)
            {
                val++;
            }
            larvae::DoNotOptimize(arr.Data());
        }

        state.SetItemsProcessed(state.iterations() * 1000);
    });

    auto bench9 = larvae::RegisterBenchmark("WaxArray", "FrontBackAccess_100", [](larvae::BenchmarkState& state) {
        wax::Array<int, 100> arr = {};
        arr.Fill(42);

        while (state.KeepRunning())
        {
            int front = arr.Front();
            int back = arr.Back();
            larvae::DoNotOptimize(front);
            larvae::DoNotOptimize(back);
        }

        state.SetItemsProcessed(state.iterations() * 2);
    });

    // =============================================================================
    // std::array Benchmarks (for comparison)
    // =============================================================================

    auto bench10 = larvae::RegisterBenchmark("StdArray", "Iteration_100", [](larvae::BenchmarkState& state) {
        std::array<int, 100> arr = {};
        arr.fill(42);

        while (state.KeepRunning())
        {
            int sum = 0;
            for (int val : arr)
            {
                sum += val;
            }
            larvae::DoNotOptimize(sum);
        }

        state.SetItemsProcessed(state.iterations() * 100);
    });

    auto bench11 = larvae::RegisterBenchmark("StdArray", "Iteration_1000", [](larvae::BenchmarkState& state) {
        std::array<int, 1000> arr = {};
        arr.fill(42);

        while (state.KeepRunning())
        {
            int sum = 0;
            for (int val : arr)
            {
                sum += val;
            }
            larvae::DoNotOptimize(sum);
        }

        state.SetItemsProcessed(state.iterations() * 1000);
    });

    auto bench12 = larvae::RegisterBenchmark("StdArray", "RandomAccess_100", [](larvae::BenchmarkState& state) {
        std::array<int, 100> arr = {};
        arr.fill(42);

        while (state.KeepRunning())
        {
            for (size_t i = 0; i < 100; ++i)
            {
                int val = arr[i];
                larvae::DoNotOptimize(val);
            }
        }

        state.SetItemsProcessed(state.iterations() * 100);
    });

    auto bench13 = larvae::RegisterBenchmark("StdArray", "RandomAccess_1000", [](larvae::BenchmarkState& state) {
        std::array<int, 1000> arr = {};
        arr.fill(42);

        while (state.KeepRunning())
        {
            for (size_t i = 0; i < 1000; ++i)
            {
                int val = arr[i];
                larvae::DoNotOptimize(val);
            }
        }

        state.SetItemsProcessed(state.iterations() * 1000);
    });

    auto bench14 = larvae::RegisterBenchmark("StdArray", "Fill_100", [](larvae::BenchmarkState& state) {
        std::array<int, 100> arr = {};

        while (state.KeepRunning())
        {
            arr.fill(42);
            larvae::DoNotOptimize(arr.data());
        }

        state.SetItemsProcessed(state.iterations() * 100);
    });

    auto bench15 = larvae::RegisterBenchmark("StdArray", "Fill_1000", [](larvae::BenchmarkState& state) {
        std::array<int, 1000> arr = {};

        while (state.KeepRunning())
        {
            arr.fill(42);
            larvae::DoNotOptimize(arr.data());
        }

        state.SetItemsProcessed(state.iterations() * 1000);
    });

    auto bench16 = larvae::RegisterBenchmark("StdArray", "Modification_100", [](larvae::BenchmarkState& state) {
        std::array<int, 100> arr = {};

        while (state.KeepRunning())
        {
            for (int& val : arr)
            {
                val++;
            }
            larvae::DoNotOptimize(arr.data());
        }

        state.SetItemsProcessed(state.iterations() * 100);
    });

    auto bench17 = larvae::RegisterBenchmark("StdArray", "Modification_1000", [](larvae::BenchmarkState& state) {
        std::array<int, 1000> arr = {};

        while (state.KeepRunning())
        {
            for (int& val : arr)
            {
                val++;
            }
            larvae::DoNotOptimize(arr.data());
        }

        state.SetItemsProcessed(state.iterations() * 1000);
    });

    auto bench18 = larvae::RegisterBenchmark("StdArray", "FrontBackAccess_100", [](larvae::BenchmarkState& state) {
        std::array<int, 100> arr = {};
        arr.fill(42);

        while (state.KeepRunning())
        {
            int front = arr.front();
            int back = arr.back();
            larvae::DoNotOptimize(front);
            larvae::DoNotOptimize(back);
        }

        state.SetItemsProcessed(state.iterations() * 2);
    });
}
