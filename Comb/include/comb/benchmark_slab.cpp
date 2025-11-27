#include <larvae/larvae.h>
#include <comb/slab_allocator.h>
#include <vector>

namespace
{
    auto bench1 = larvae::RegisterBenchmark("SlabAllocator", "SmallAllocations_32B", [](larvae::BenchmarkState& state) {
        comb::SlabAllocator<100000, 16, 32, 64, 128> slabs;
        std::vector<void*> ptrs;
        ptrs.reserve(100000);

        while (state.KeepRunning())
        {
            void* ptr = slabs.Allocate(32, 8);
            larvae::DoNotOptimize(ptr);
            if (ptr)
            {
                ptrs.push_back(ptr);
            }

            if (slabs.GetSlabFreeCount(1) == 0)
            {
                for (void* p : ptrs)
                {
                    slabs.Deallocate(p);
                }
                ptrs.clear();
            }
        }

        for (void* ptr : ptrs)
        {
            slabs.Deallocate(ptr);
        }
    });

    auto bench2 = larvae::RegisterBenchmark("SlabAllocator", "MediumAllocations_128B", [](larvae::BenchmarkState& state) {
        comb::SlabAllocator<100000, 32, 64, 128, 256> slabs;
        std::vector<void*> ptrs;
        ptrs.reserve(100000);

        while (state.KeepRunning())
        {
            void* ptr = slabs.Allocate(128, 8);
            larvae::DoNotOptimize(ptr);
            if (ptr)
            {
                ptrs.push_back(ptr);
            }

            if (slabs.GetSlabFreeCount(2) == 0)
            {
                for (void* p : ptrs)
                {
                    slabs.Deallocate(p);
                }
                ptrs.clear();
            }
        }

        for (void* ptr : ptrs)
        {
            slabs.Deallocate(ptr);
        }
    });

    auto bench3 = larvae::RegisterBenchmark("SlabAllocator", "MixedSizeAllocations", [](larvae::BenchmarkState& state) {
        comb::SlabAllocator<100000, 16, 32, 64, 128, 256, 512> slabs;
        std::vector<void*> ptrs;
        ptrs.reserve(100000);
        size_t counter = 0;

        while (state.KeepRunning())
        {
            size_t size = 0;
            switch (counter % 6)
            {
                case 0: size = 16; break;
                case 1: size = 32; break;
                case 2: size = 64; break;
                case 3: size = 128; break;
                case 4: size = 256; break;
                case 5: size = 512; break;
            }

            void* ptr = slabs.Allocate(size, 8);
            larvae::DoNotOptimize(ptr);
            if (ptr)
            {
                ptrs.push_back(ptr);
            }
            ++counter;

            if (ptrs.size() >= 50000)
            {
                for (void* p : ptrs)
                {
                    slabs.Deallocate(p);
                }
                ptrs.clear();
            }
        }

        for (void* ptr : ptrs)
        {
            slabs.Deallocate(ptr);
        }
    });

    auto bench4 = larvae::RegisterBenchmark("SlabAllocator", "AllocationAndDeallocation", [](larvae::BenchmarkState& state) {
        comb::SlabAllocator<100000, 64> slabs;

        while (state.KeepRunning())
        {
            void* ptr = slabs.Allocate(64, 8);
            larvae::DoNotOptimize(ptr);
            if (ptr)
            {
                slabs.Deallocate(ptr);
            }
        }
    });

    auto bench5 = larvae::RegisterBenchmark("SlabAllocator", "RapidRecycling", [](larvae::BenchmarkState& state) {
        comb::SlabAllocator<10, 64> slabs;

        while (state.KeepRunning())
        {
            void* ptrs[10];

            for (int i = 0; i < 10; ++i)
            {
                ptrs[i] = slabs.Allocate(64, 8);
                larvae::DoNotOptimize(ptrs[i]);
            }

            for (int i = 0; i < 10; ++i)
            {
                slabs.Deallocate(ptrs[i]);
            }
        }
    });

    auto bench6 = larvae::RegisterBenchmark("SlabAllocator", "ResetPerformance", [](larvae::BenchmarkState& state) {
        comb::SlabAllocator<10000, 64> slabs;
        std::vector<void*> ptrs;
        ptrs.reserve(10000);

        while (state.KeepRunning())
        {
            ptrs.clear();

            for (int i = 0; i < 10000; ++i)
            {
                void* ptr = slabs.Allocate(64, 8);
                if (ptr)
                {
                    ptrs.push_back(ptr);
                }
            }

            slabs.Reset();
            larvae::DoNotOptimize(ptrs.data());
        }
    });

    auto bench7 = larvae::RegisterBenchmark("SlabAllocator", "SlabSelectionOverhead", [](larvae::BenchmarkState& state) {
        comb::SlabAllocator<100000, 16, 32, 64, 128, 256, 512, 1024, 2048> slabs;
        std::vector<void*> ptrs;
        ptrs.reserve(100000);
        size_t counter = 0;

        while (state.KeepRunning())
        {
            size_t size = size_t{16} << (counter % 9);
            void* ptr = slabs.Allocate(size, 8);
            larvae::DoNotOptimize(ptr);
            if (ptr)
            {
                ptrs.push_back(ptr);
            }
            ++counter;

            if (ptrs.size() >= 50000)
            {
                for (void* p : ptrs)
                {
                    slabs.Deallocate(p);
                }
                ptrs.clear();
            }
        }

        for (void* ptr : ptrs)
        {
            slabs.Deallocate(ptr);
        }
    });

    auto bench8 = larvae::RegisterBenchmark("SlabAllocator", "UnalignedSizes", [](larvae::BenchmarkState& state) {
        comb::SlabAllocator<100000, 32, 64, 128, 256> slabs;
        std::vector<void*> ptrs;
        ptrs.reserve(100000);
        size_t counter = 0;

        while (state.KeepRunning())
        {
            size_t size = 0;
            switch (counter % 4)
            {
                case 0: size = 17; break;
                case 1: size = 50; break;
                case 2: size = 100; break;
                case 3: size = 200; break;
            }

            void* ptr = slabs.Allocate(size, 8);
            larvae::DoNotOptimize(ptr);
            if (ptr)
            {
                ptrs.push_back(ptr);
            }
            ++counter;

            if (ptrs.size() >= 50000)
            {
                for (void* p : ptrs)
                {
                    slabs.Deallocate(p);
                }
                ptrs.clear();
            }
        }

        for (void* ptr : ptrs)
        {
            slabs.Deallocate(ptr);
        }
    });

    auto bench9 = larvae::RegisterBenchmark("malloc", "MixedSizeAllocations", [](larvae::BenchmarkState& state) {
        std::vector<void*> ptrs;
        ptrs.reserve(10000);
        size_t counter = 0;

        while (state.KeepRunning())
        {
            size_t size = 0;
            switch (counter % 6)
            {
                case 0: size = 16; break;
                case 1: size = 32; break;
                case 2: size = 64; break;
                case 3: size = 128; break;
                case 4: size = 256; break;
                case 5: size = 512; break;
            }

            void* ptr = malloc(size);
            larvae::DoNotOptimize(ptr);
            if (ptr)
            {
                ptrs.push_back(ptr);
            }
            ++counter;

            if (ptrs.size() >= 10000)
            {
                for (void* p : ptrs)
                {
                    free(p);
                }
                ptrs.clear();
            }
        }

        for (void* ptr : ptrs)
        {
            free(ptr);
        }
    });

    auto bench10 = larvae::RegisterBenchmark("malloc", "AllocationAndDeallocation", [](larvae::BenchmarkState& state) {
        while (state.KeepRunning())
        {
            void* ptr = malloc(64);
            larvae::DoNotOptimize(ptr);
            if (ptr)
            {
                free(ptr);
            }
        }
    });
}
