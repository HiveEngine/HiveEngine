#include <larvae/larvae.h>
#include <comb/pool_allocator.h>
#include <cstdlib>

namespace
{
    struct SmallObject
    {
        int data[4]; // 16 bytes
    };

    struct MediumObject
    {
        int data[64]; // 256 bytes
    };

    auto bench1 = larvae::RegisterBenchmark("PoolAllocator", "SmallObjectAllocation", [](larvae::BenchmarkState& state) {
        comb::PoolAllocator<SmallObject> pool{100000};

        while (state.KeepRunning())
        {
            SmallObject* obj = comb::New<SmallObject>(pool);
            larvae::DoNotOptimize(obj);

            // Reset when pool full
            if (pool.GetFreeCount() == 0)
            {
                pool.Reset();
            }
        }

        state.SetBytesProcessed(state.iterations() * sizeof(SmallObject));
        state.SetItemsProcessed(state.iterations());
    });

    auto bench2 = larvae::RegisterBenchmark("PoolAllocator", "MediumObjectAllocation", [](larvae::BenchmarkState& state) {
        comb::PoolAllocator<MediumObject> pool{100000};

        while (state.KeepRunning())
        {
            MediumObject* obj = comb::New<MediumObject>(pool);
            larvae::DoNotOptimize(obj);

            if (pool.GetFreeCount() == 0)
            {
                pool.Reset();
            }
        }

        state.SetBytesProcessed(state.iterations() * sizeof(MediumObject));
        state.SetItemsProcessed(state.iterations());
    });

    auto bench3 = larvae::RegisterBenchmark("PoolAllocator", "AllocationAndDeallocation", [](larvae::BenchmarkState& state) {
        comb::PoolAllocator<SmallObject> pool{100000};

        while (state.KeepRunning())
        {
            SmallObject* obj = comb::New<SmallObject>(pool);
            larvae::DoNotOptimize(obj);
            comb::Delete(pool, obj);
        }

        state.SetBytesProcessed(state.iterations() * sizeof(SmallObject));
        state.SetItemsProcessed(state.iterations());
    });

    auto bench4 = larvae::RegisterBenchmark("PoolAllocator", "RapidRecycling", [](larvae::BenchmarkState& state) {
        comb::PoolAllocator<SmallObject> pool{10};

        while (state.KeepRunning())
        {
            // Allocate all
            SmallObject* objects[10];
            for (int i = 0; i < 10; ++i)
            {
                objects[i] = comb::New<SmallObject>(pool);
                larvae::DoNotOptimize(objects[i]);
            }

            // Deallocate all
            for (int i = 0; i < 10; ++i)
            {
                comb::Delete(pool, objects[i]);
            }
        }

        state.SetItemsProcessed(state.iterations() * 20); // 10 allocs + 10 deallocs
    });

    auto bench5 = larvae::RegisterBenchmark("PoolAllocator", "ResetPerformance", [](larvae::BenchmarkState& state) {
        comb::PoolAllocator<SmallObject> pool{10000};

        // Fill pool
        for (size_t i = 0; i < 10000; ++i)
        {
            (void)comb::New<SmallObject>(pool);
        }

        while (state.KeepRunning())
        {
            pool.Reset();

            // Refill for next iteration
            for (size_t i = 0; i < 10000; ++i)
            {
                (void)comb::New<SmallObject>(pool);
            }
        }

        state.SetItemsProcessed(state.iterations());
    });

    auto bench6 = larvae::RegisterBenchmark("malloc", "SmallObjectAllocation", [](larvae::BenchmarkState& state) {
        while (state.KeepRunning())
        {
            void* ptr = std::malloc(sizeof(SmallObject));
            larvae::DoNotOptimize(ptr);
            std::free(ptr);
        }

        state.SetBytesProcessed(state.iterations() * sizeof(SmallObject));
        state.SetItemsProcessed(state.iterations());
    });
}
