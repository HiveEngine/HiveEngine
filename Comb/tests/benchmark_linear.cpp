#include <larvae/larvae.h>
#include <comb/linear_allocator.h>
#include <cstdlib>
#include <vector>

namespace
{
    constexpr size_t operator""_KB(unsigned long long kb) { return kb * 1024; }
    constexpr size_t operator""_MB(unsigned long long mb) { return mb * 1024 * 1024; }

    auto bench1 = larvae::RegisterBenchmark("LinearAllocator", "SmallAllocations_64B", [](larvae::BenchmarkState& state) {
        comb::LinearAllocator allocator{10_MB};

        while (state.KeepRunning())
        {
            void* ptr = allocator.Allocate(64, 8);
            larvae::DoNotOptimize(ptr);

            if (allocator.GetUsedMemory() > 9_MB)
            {
                allocator.Reset();
            }
        }

        state.SetBytesProcessed(state.iterations() * 64);
        state.SetItemsProcessed(state.iterations());
    });

    auto bench2 = larvae::RegisterBenchmark("LinearAllocator", "MediumAllocations_1KB", [](larvae::BenchmarkState& state) {
        comb::LinearAllocator allocator{10_MB};

        while (state.KeepRunning())
        {
            void* ptr = allocator.Allocate(1_KB, 16);
            larvae::DoNotOptimize(ptr);

            if (allocator.GetUsedMemory() > 9_MB)
            {
                allocator.Reset();
            }
        }

        state.SetBytesProcessed(state.iterations() * 1_KB);
        state.SetItemsProcessed(state.iterations());
    });

    auto bench3 = larvae::RegisterBenchmark("LinearAllocator", "WithReset", [](larvae::BenchmarkState& state) {
        comb::LinearAllocator allocator{1_MB};

        while (state.KeepRunning())
        {
            for (int i = 0; i < 100; ++i)
            {
                void* ptr = allocator.Allocate(256, 8);
                larvae::DoNotOptimize(ptr);
            }
            allocator.Reset();
        }

        state.SetBytesProcessed(state.iterations() * 100 * 256);
        state.SetItemsProcessed(state.iterations() * 100);
    });

    auto bench4 = larvae::RegisterBenchmark("LinearAllocator", "AlignedAllocations_64B", [](larvae::BenchmarkState& state) {
        comb::LinearAllocator allocator{10_MB};

        while (state.KeepRunning())
        {
            void* ptr = allocator.Allocate(64, 64);
            larvae::DoNotOptimize(ptr);

            if (allocator.GetUsedMemory() > 9_MB)
            {
                allocator.Reset();
            }
        }

        state.SetBytesProcessed(state.iterations() * 64);
    });

    auto bench5 = larvae::RegisterBenchmark("malloc", "SmallAllocations_64B", [](larvae::BenchmarkState& state) {
        std::vector<void*> ptrs;
        ptrs.reserve(10000);

        while (state.KeepRunning())
        {
            void* ptr = std::malloc(64);
            larvae::DoNotOptimize(ptr);
            ptrs.push_back(ptr);

            if (ptrs.size() >= 10000)
            {
                for (void* p : ptrs)
                {
                    std::free(p);
                }
                ptrs.clear();
            }
        }

        for (void* p : ptrs)
        {
            std::free(p);
        }

        state.SetBytesProcessed(state.iterations() * 64);
        state.SetItemsProcessed(state.iterations());
    });

    auto bench6 = larvae::RegisterBenchmark("LinearAllocator", "Markers", [](larvae::BenchmarkState& state) {
        comb::LinearAllocator allocator{10_MB};

        while (state.KeepRunning())
        {
            void* marker = allocator.GetMarker();

            for (int i = 0; i < 50; ++i)
            {
                void* ptr = allocator.Allocate(128, 8);
                larvae::DoNotOptimize(ptr);
            }

            allocator.ResetToMarker(marker);
        }

        state.SetItemsProcessed(state.iterations() * 50);
    });
}
