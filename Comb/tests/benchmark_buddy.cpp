#include <larvae/larvae.h>
#include <comb/buddy_allocator.h>
#include <cstdlib>
#include <vector>

namespace
{
    constexpr size_t operator""_KB(unsigned long long kb) { return kb * 1024; }
    constexpr size_t operator""_MB(unsigned long long mb) { return mb * 1024 * 1024; }

    auto bench1 = larvae::RegisterBenchmark("BuddyAllocator", "SmallAllocations_64B", [](larvae::BenchmarkState& state) {
        comb::BuddyAllocator allocator{100_MB};

        while (state.KeepRunning())
        {
            void* ptr = allocator.Allocate(64, 8);
            larvae::DoNotOptimize(ptr);

            if (allocator.GetUsedMemory() > 90_MB)
            {
                // Can't reset BuddyAllocator easily, so we'll need to track and free
                // For benchmark, we'll just break and restart
                break;
            }
        }

        state.SetBytesProcessed(state.iterations() * 64);
        state.SetItemsProcessed(state.iterations());
    });

    auto bench2 = larvae::RegisterBenchmark("BuddyAllocator", "MediumAllocations_256B", [](larvae::BenchmarkState& state) {
        comb::BuddyAllocator allocator{100_MB};

        while (state.KeepRunning())
        {
            void* ptr = allocator.Allocate(256, 8);
            larvae::DoNotOptimize(ptr);

            if (allocator.GetUsedMemory() > 90_MB)
            {
                break;
            }
        }

        state.SetBytesProcessed(state.iterations() * 256);
        state.SetItemsProcessed(state.iterations());
    });

    auto bench3 = larvae::RegisterBenchmark("BuddyAllocator", "LargeAllocations_4KB", [](larvae::BenchmarkState& state) {
        comb::BuddyAllocator allocator{500_MB};

        while (state.KeepRunning())
        {
            void* ptr = allocator.Allocate(4_KB, 8);
            larvae::DoNotOptimize(ptr);

            if (allocator.GetUsedMemory() > 450_MB)
            {
                break;
            }
        }

        state.SetBytesProcessed(state.iterations() * 4_KB);
        state.SetItemsProcessed(state.iterations());
    });

    auto bench4 = larvae::RegisterBenchmark("BuddyAllocator", "AllocationAndDeallocation", [](larvae::BenchmarkState& state) {
        comb::BuddyAllocator allocator{100_MB};

        while (state.KeepRunning())
        {
            void* ptr = allocator.Allocate(128, 8);
            larvae::DoNotOptimize(ptr);
            allocator.Deallocate(ptr);
        }

        state.SetBytesProcessed(state.iterations() * 128);
        state.SetItemsProcessed(state.iterations());
    });

    auto bench5 = larvae::RegisterBenchmark("BuddyAllocator", "RapidRecycling", [](larvae::BenchmarkState& state) {
        comb::BuddyAllocator allocator{10_MB};

        while (state.KeepRunning())
        {
            for (int j = 0; j < 10; ++j)
            {
                void* ptr = allocator.Allocate(128, 8);
                larvae::DoNotOptimize(ptr);
                allocator.Deallocate(ptr);
            }
        }

        state.SetItemsProcessed(state.iterations() * 10);
    });

    auto bench6 = larvae::RegisterBenchmark("BuddyAllocator", "MixedSizeAllocations", [](larvae::BenchmarkState& state) {
        comb::BuddyAllocator allocator{100_MB};
        const size_t sizes[] = {32, 64, 128, 256, 512, 1024};
        const int num_sizes = sizeof(sizes) / sizeof(sizes[0]);

        size_t i = 0;
        while (state.KeepRunning())
        {
            size_t size = sizes[i % num_sizes];
            void* ptr = allocator.Allocate(size, 8);
            larvae::DoNotOptimize(ptr);
            ++i;

            if (allocator.GetUsedMemory() > 90_MB)
            {
                break;
            }
        }

        state.SetItemsProcessed(state.iterations());
    });

    auto bench7 = larvae::RegisterBenchmark("BuddyAllocator", "CoalescingPattern", [](larvae::BenchmarkState& state) {
        comb::BuddyAllocator allocator{10_MB};

        while (state.KeepRunning())
        {
            // Allocate two buddies
            void* ptr1 = allocator.Allocate(128, 8);
            void* ptr2 = allocator.Allocate(128, 8);
            larvae::DoNotOptimize(ptr1);
            larvae::DoNotOptimize(ptr2);

            // Free both - triggers coalescing
            allocator.Deallocate(ptr1);
            allocator.Deallocate(ptr2);
        }

        state.SetItemsProcessed(state.iterations() * 2);
    });

    auto bench8 = larvae::RegisterBenchmark("BuddyAllocator", "SplittingOverhead", [](larvae::BenchmarkState& state) {
        comb::BuddyAllocator allocator{100_MB};

        while (state.KeepRunning())
        {
            // Small allocation forces splitting
            void* ptr = allocator.Allocate(64, 8);
            larvae::DoNotOptimize(ptr);

            if (allocator.GetUsedMemory() > 90_MB)
            {
                break;
            }
        }

        state.SetItemsProcessed(state.iterations());
    });

    auto bench9 = larvae::RegisterBenchmark("BuddyAllocator", "WorstCaseFragmentation", [](larvae::BenchmarkState& state) {
        comb::BuddyAllocator allocator{10_MB};

        while (state.KeepRunning())
        {
            // Create worst-case fragmentation
            void* p1 = allocator.Allocate(256, 8);
            void* p2 = allocator.Allocate(256, 8);
            void* p3 = allocator.Allocate(256, 8);
            void* p4 = allocator.Allocate(256, 8);

            larvae::DoNotOptimize(p1);
            larvae::DoNotOptimize(p2);
            larvae::DoNotOptimize(p3);
            larvae::DoNotOptimize(p4);

            // Free alternating
            allocator.Deallocate(p1);
            allocator.Deallocate(p3);

            // Free remaining
            allocator.Deallocate(p2);
            allocator.Deallocate(p4);
        }

        state.SetItemsProcessed(state.iterations() * 4);
    });

    auto bench10 = larvae::RegisterBenchmark("malloc", "MixedSizeAllocations", [](larvae::BenchmarkState& state) {
        const size_t sizes[] = {32, 64, 128, 256, 512, 1024};
        const int num_sizes = sizeof(sizes) / sizeof(sizes[0]);

        std::vector<void*> ptrs;
        ptrs.reserve(10000);

        size_t i = 0;
        while (state.KeepRunning())
        {
            size_t size = sizes[i % num_sizes];
            void* ptr = malloc(size);
            larvae::DoNotOptimize(ptr);
            ptrs.push_back(ptr);
            ++i;

            if (ptrs.size() > 10000)
            {
                for (void* p : ptrs)
                {
                    free(p);
                }
                ptrs.clear();
            }
        }

        for (void* p : ptrs)
        {
            free(p);
        }

        state.SetItemsProcessed(state.iterations());
    });

    auto bench11 = larvae::RegisterBenchmark("malloc", "AllocationAndDeallocation", [](larvae::BenchmarkState& state) {
        while (state.KeepRunning())
        {
            void* ptr = malloc(128);
            larvae::DoNotOptimize(ptr);
            free(ptr);
        }

        state.SetItemsProcessed(state.iterations());
    });

    auto bench12 = larvae::RegisterBenchmark("BuddyAllocator", "PowerOfTwoSizes", [](larvae::BenchmarkState& state) {
        comb::BuddyAllocator allocator{100_MB};

        const size_t sizes[] = {64, 128, 256, 512, 1024, 2048, 4096};
        const int num_sizes = sizeof(sizes) / sizeof(sizes[0]);

        size_t i = 0;
        while (state.KeepRunning())
        {
            size_t size = sizes[i % num_sizes];
            void* ptr = allocator.Allocate(size, 8);
            larvae::DoNotOptimize(ptr);
            ++i;

            if (allocator.GetUsedMemory() > 90_MB)
            {
                break;
            }
        }

        state.SetItemsProcessed(state.iterations());
    });

    auto bench13 = larvae::RegisterBenchmark("BuddyAllocator", "UnalignedSizes", [](larvae::BenchmarkState& state) {
        comb::BuddyAllocator allocator{100_MB};

        const size_t sizes[] = {17, 33, 65, 129, 257, 513, 1025};
        const int num_sizes = sizeof(sizes) / sizeof(sizes[0]);

        size_t i = 0;
        while (state.KeepRunning())
        {
            size_t size = sizes[i % num_sizes];
            void* ptr = allocator.Allocate(size, 8);
            larvae::DoNotOptimize(ptr);
            ++i;

            if (allocator.GetUsedMemory() > 90_MB)
            {
                break;
            }
        }

        state.SetItemsProcessed(state.iterations());
    });
}
