#include <larvae/larvae.h>
#include <wax/vector_types.h>
#include <vector>

namespace
{
    // =============================================================================
    // Wax::Vector Benchmarks
    // =============================================================================

    auto bench1 = larvae::RegisterBenchmark("WaxVector", "PushBack_100", [](larvae::BenchmarkState& state) {
        comb::LinearAllocator alloc{1024 * 1024};

        while (state.KeepRunning())
        {
            alloc.Reset();
            wax::LinearVector<int> vec{alloc};
            for (int i = 0; i < 100; ++i)
            {
                vec.PushBack(i);
            }
            larvae::DoNotOptimize(vec.Data());
        }

        state.SetItemsProcessed(state.iterations() * 100);
    });

    auto bench2 = larvae::RegisterBenchmark("WaxVector", "PushBack_1000", [](larvae::BenchmarkState& state) {
        comb::LinearAllocator alloc{1024 * 1024};

        while (state.KeepRunning())
        {
            alloc.Reset();
            wax::LinearVector<int> vec{alloc};
            for (int i = 0; i < 1000; ++i)
            {
                vec.PushBack(i);
            }
            larvae::DoNotOptimize(vec.Data());
        }

        state.SetItemsProcessed(state.iterations() * 1000);
    });

    auto bench3 = larvae::RegisterBenchmark("WaxVector", "PushBackReserved_1000", [](larvae::BenchmarkState& state) {
        comb::LinearAllocator alloc{1024 * 1024};

        while (state.KeepRunning())
        {
            alloc.Reset();
            wax::LinearVector<int> vec{alloc};
            vec.Reserve(1000);
            for (int i = 0; i < 1000; ++i)
            {
                vec.PushBack(i);
            }
            larvae::DoNotOptimize(vec.Data());
        }

        state.SetItemsProcessed(state.iterations() * 1000);
    });

    auto bench4 = larvae::RegisterBenchmark("WaxVector", "Iteration_1000", [](larvae::BenchmarkState& state) {
        comb::LinearAllocator alloc{1024 * 1024};
        wax::LinearVector<int> vec{alloc};
        for (int i = 0; i < 1000; ++i)
        {
            vec.PushBack(i);
        }

        while (state.KeepRunning())
        {
            int sum = 0;
            for (int val : vec)
            {
                sum += val;
            }
            larvae::DoNotOptimize(sum);
        }

        state.SetItemsProcessed(state.iterations() * 1000);
    });

    auto bench5 = larvae::RegisterBenchmark("WaxVector", "RandomAccess_1000", [](larvae::BenchmarkState& state) {
        comb::LinearAllocator alloc{1024 * 1024};
        wax::LinearVector<int> vec{alloc};
        for (int i = 0; i < 1000; ++i)
        {
            vec.PushBack(i);
        }

        while (state.KeepRunning())
        {
            for (size_t i = 0; i < 1000; ++i)
            {
                int val = vec[i];
                larvae::DoNotOptimize(val);
            }
        }

        state.SetItemsProcessed(state.iterations() * 1000);
    });

    auto bench6 = larvae::RegisterBenchmark("WaxVector", "Modification_1000", [](larvae::BenchmarkState& state) {
        comb::LinearAllocator alloc{1024 * 1024};
        wax::LinearVector<int> vec{alloc};
        vec.Resize(1000);

        while (state.KeepRunning())
        {
            for (int& val : vec)
            {
                val++;
            }
            larvae::DoNotOptimize(vec.Data());
        }

        state.SetItemsProcessed(state.iterations() * 1000);
    });

    auto bench7 = larvae::RegisterBenchmark("WaxVector", "EmplaceBack_1000", [](larvae::BenchmarkState& state) {
        comb::LinearAllocator alloc{1024 * 1024};

        while (state.KeepRunning())
        {
            alloc.Reset();
            wax::LinearVector<int> vec{alloc};
            for (int i = 0; i < 1000; ++i)
            {
                vec.EmplaceBack(i);
            }
            larvae::DoNotOptimize(vec.Data());
        }

        state.SetItemsProcessed(state.iterations() * 1000);
    });

    auto bench8 = larvae::RegisterBenchmark("WaxVector", "PopBack_1000", [](larvae::BenchmarkState& state) {
        comb::LinearAllocator alloc{1024 * 1024};

        while (state.KeepRunning())
        {
            alloc.Reset();
            wax::LinearVector<int> vec{alloc};
            for (int i = 0; i < 1000; ++i)
            {
                vec.PushBack(i);
            }

            // Measure PopBack only (but includes setup cost)
            for (int i = 0; i < 1000; ++i)
            {
                vec.PopBack();
            }
            larvae::DoNotOptimize(vec.Data());
        }

        state.SetItemsProcessed(state.iterations() * 1000);
    });

    auto bench9 = larvae::RegisterBenchmark("WaxVector", "Resize_1000", [](larvae::BenchmarkState& state) {
        comb::LinearAllocator alloc{1024 * 1024};

        while (state.KeepRunning())
        {
            alloc.Reset();
            wax::LinearVector<int> vec{alloc};
            vec.Resize(1000);
            larvae::DoNotOptimize(vec.Data());
        }

        state.SetItemsProcessed(state.iterations() * 1000);
    });

    // =============================================================================
    // std::vector Benchmarks (for comparison)
    // =============================================================================

    auto bench10 = larvae::RegisterBenchmark("StdVector", "PushBack_100", [](larvae::BenchmarkState& state) {
        while (state.KeepRunning())
        {
            std::vector<int> vec;
            for (int i = 0; i < 100; ++i)
            {
                vec.push_back(i);
            }
            larvae::DoNotOptimize(vec.data());
        }

        state.SetItemsProcessed(state.iterations() * 100);
    });

    auto bench11 = larvae::RegisterBenchmark("StdVector", "PushBack_1000", [](larvae::BenchmarkState& state) {
        while (state.KeepRunning())
        {
            std::vector<int> vec;
            for (int i = 0; i < 1000; ++i)
            {
                vec.push_back(i);
            }
            larvae::DoNotOptimize(vec.data());
        }

        state.SetItemsProcessed(state.iterations() * 1000);
    });

    auto bench12 = larvae::RegisterBenchmark("StdVector", "PushBackReserved_1000", [](larvae::BenchmarkState& state) {
        while (state.KeepRunning())
        {
            std::vector<int> vec;
            vec.reserve(1000);
            for (int i = 0; i < 1000; ++i)
            {
                vec.push_back(i);
            }
            larvae::DoNotOptimize(vec.data());
        }

        state.SetItemsProcessed(state.iterations() * 1000);
    });

    auto bench13 = larvae::RegisterBenchmark("StdVector", "Iteration_1000", [](larvae::BenchmarkState& state) {
        std::vector<int> vec;
        for (int i = 0; i < 1000; ++i)
        {
            vec.push_back(i);
        }

        while (state.KeepRunning())
        {
            int sum = 0;
            for (int val : vec)
            {
                sum += val;
            }
            larvae::DoNotOptimize(sum);
        }

        state.SetItemsProcessed(state.iterations() * 1000);
    });

    auto bench14 = larvae::RegisterBenchmark("StdVector", "RandomAccess_1000", [](larvae::BenchmarkState& state) {
        std::vector<int> vec;
        for (int i = 0; i < 1000; ++i)
        {
            vec.push_back(i);
        }

        while (state.KeepRunning())
        {
            for (size_t i = 0; i < 1000; ++i)
            {
                int val = vec[i];
                larvae::DoNotOptimize(val);
            }
        }

        state.SetItemsProcessed(state.iterations() * 1000);
    });

    auto bench15 = larvae::RegisterBenchmark("StdVector", "Modification_1000", [](larvae::BenchmarkState& state) {
        std::vector<int> vec;
        vec.resize(1000);

        while (state.KeepRunning())
        {
            for (int& val : vec)
            {
                val++;
            }
            larvae::DoNotOptimize(vec.data());
        }

        state.SetItemsProcessed(state.iterations() * 1000);
    });

    auto bench16 = larvae::RegisterBenchmark("StdVector", "EmplaceBack_1000", [](larvae::BenchmarkState& state) {
        while (state.KeepRunning())
        {
            std::vector<int> vec;
            for (int i = 0; i < 1000; ++i)
            {
                vec.emplace_back(i);
            }
            larvae::DoNotOptimize(vec.data());
        }

        state.SetItemsProcessed(state.iterations() * 1000);
    });

    auto bench17 = larvae::RegisterBenchmark("StdVector", "PopBack_1000", [](larvae::BenchmarkState& state) {
        while (state.KeepRunning())
        {
            std::vector<int> vec;
            for (int i = 0; i < 1000; ++i)
            {
                vec.push_back(i);
            }

            // Measure PopBack only (but includes setup cost)
            for (int i = 0; i < 1000; ++i)
            {
                vec.pop_back();
            }
            larvae::DoNotOptimize(vec.data());
        }

        state.SetItemsProcessed(state.iterations() * 1000);
    });

    auto bench18 = larvae::RegisterBenchmark("StdVector", "Resize_1000", [](larvae::BenchmarkState& state) {
        while (state.KeepRunning())
        {
            std::vector<int> vec;
            vec.resize(1000);
            larvae::DoNotOptimize(vec.data());
        }

        state.SetItemsProcessed(state.iterations() * 1000);
    });
}
