#include <larvae/larvae.h>
#include <queen/world/world.h>
#include <queen/scheduler/parallel_scheduler.h>
#include <comb/linear_allocator.h>
#include <atomic>

namespace
{
    // Test components
    struct Position { float x, y, z; };
    struct Velocity { float dx, dy, dz; };
    struct Health { int value; };

    // ============================================================================
    // ParallelScheduler Basic Tests
    // ============================================================================

    auto test1 = larvae::RegisterTest("QueenParallelScheduler", "Creation", []() {
        comb::LinearAllocator alloc{8 * 1024 * 1024};
        queen::ParallelScheduler<comb::LinearAllocator> scheduler{alloc, 4};

        larvae::AssertTrue(scheduler.NeedsRebuild());
        larvae::AssertFalse(scheduler.HasCycle());
    });

    auto test2 = larvae::RegisterTest("QueenParallelScheduler", "CreationWithExternalPool", []() {
        comb::LinearAllocator alloc{8 * 1024 * 1024};
        queen::ThreadPool<comb::LinearAllocator> pool{alloc, 4};
        pool.Start();

        queen::ParallelScheduler<comb::LinearAllocator> scheduler{alloc, pool};

        larvae::AssertTrue(scheduler.NeedsRebuild());
        larvae::AssertEqual(scheduler.Pool(), &pool);

        pool.Stop();
    });

    auto test3 = larvae::RegisterTest("QueenParallelScheduler", "BuildEmptyStorage", []() {
        comb::LinearAllocator alloc{8 * 1024 * 1024};
        queen::ParallelScheduler<comb::LinearAllocator> scheduler{alloc, 4};
        queen::SystemStorage<comb::LinearAllocator> storage{alloc};

        scheduler.Build(storage);

        larvae::AssertFalse(scheduler.NeedsRebuild());
        larvae::AssertFalse(scheduler.HasCycle());
        larvae::AssertEqual(scheduler.ExecutionOrder().Size(), size_t{0});
    });

    auto test4 = larvae::RegisterTest("QueenParallelScheduler", "RunEmptyWorld", []() {
        comb::LinearAllocator alloc{8 * 1024 * 1024};
        queen::ParallelScheduler<comb::LinearAllocator> scheduler{alloc, 4};
        queen::World world{};
        queen::SystemStorage<comb::LinearAllocator> storage{alloc};

        scheduler.Build(storage);
        scheduler.RunAll(world, storage);  // Should not crash
    });

    auto test5 = larvae::RegisterTest("QueenParallelScheduler", "SingleSystem", []() {
        comb::LinearAllocator alloc{8 * 1024 * 1024};
        queen::ParallelScheduler<comb::LinearAllocator> scheduler{alloc, 4};
        queen::World world{};
        queen::SystemStorage<comb::LinearAllocator> storage{alloc};

        std::atomic<int> counter{0};

        storage.Register(
            "IncrementSystem",
            [&counter](queen::World&) {
                counter.fetch_add(1);
            },
            queen::AccessDescriptor<comb::LinearAllocator>{alloc}
        );

        scheduler.Build(storage);
        scheduler.RunAll(world, storage);

        larvae::AssertEqual(counter.load(), 1);
    });

    auto test6 = larvae::RegisterTest("QueenParallelScheduler", "IndependentSystemsRunParallel", []() {
        comb::LinearAllocator alloc{16 * 1024 * 1024};
        queen::ParallelScheduler<comb::LinearAllocator> scheduler{alloc, 4};
        queen::World world{};
        queen::SystemStorage<comb::LinearAllocator> storage{alloc};

        std::atomic<int> counter{0};

        // Register multiple independent systems (no component conflicts)
        for (int i = 0; i < 10; ++i)
        {
            storage.Register(
                ("System" + std::to_string(i)).c_str(),
                [&counter](queen::World&) {
                    counter.fetch_add(1);
                    // Small delay to allow parallelism
                    volatile int sum = 0;
                    for (int j = 0; j < 100; ++j) sum += j;
                    (void)sum;
                },
                queen::AccessDescriptor<comb::LinearAllocator>{alloc}  // No component access = no conflicts
            );
        }

        scheduler.Build(storage);
        scheduler.RunAll(world, storage);

        larvae::AssertEqual(counter.load(), 10);
    });

    auto test7 = larvae::RegisterTest("QueenParallelScheduler", "DependentSystemsRunInOrder", []() {
        comb::LinearAllocator alloc{16 * 1024 * 1024};
        queen::ParallelScheduler<comb::LinearAllocator> scheduler{alloc, 4};
        queen::World world{};
        queen::SystemStorage<comb::LinearAllocator> storage{alloc};

        std::atomic<int> order{0};
        std::atomic<int> system1_order{-1};
        std::atomic<int> system2_order{-1};

        // System 1: writes Position
        {
            queen::AccessDescriptor<comb::LinearAllocator> access1{alloc};
            access1.AddComponentWrite<Position>();
            storage.Register(
                "WritePosition",
                [&order, &system1_order](queen::World&) {
                    system1_order.store(order.fetch_add(1));
                },
                std::move(access1)
            );
        }

        // System 2: reads Position (depends on System 1)
        {
            queen::AccessDescriptor<comb::LinearAllocator> access2{alloc};
            access2.AddComponentRead<Position>();
            storage.Register(
                "ReadPosition",
                [&order, &system2_order](queen::World&) {
                    system2_order.store(order.fetch_add(1));
                },
                std::move(access2)
            );
        }

        scheduler.Build(storage);
        scheduler.RunAll(world, storage);

        // System 1 should run before System 2
        larvae::AssertTrue(system1_order.load() < system2_order.load());
    });

    auto test8 = larvae::RegisterTest("QueenParallelScheduler", "MultipleRunAllCalls", []() {
        comb::LinearAllocator alloc{8 * 1024 * 1024};
        queen::ParallelScheduler<comb::LinearAllocator> scheduler{alloc, 4};
        queen::World world{};
        queen::SystemStorage<comb::LinearAllocator> storage{alloc};

        std::atomic<int> counter{0};

        storage.Register(
            "CounterSystem",
            [&counter](queen::World&) {
                counter.fetch_add(1);
            },
            queen::AccessDescriptor<comb::LinearAllocator>{alloc}
        );

        scheduler.Build(storage);

        // Run multiple times
        for (int i = 0; i < 5; ++i)
        {
            scheduler.RunAll(world, storage);
        }

        larvae::AssertEqual(counter.load(), 5);
    });

    auto test9 = larvae::RegisterTest("QueenParallelScheduler", "InvalidateAndRebuild", []() {
        comb::LinearAllocator alloc{8 * 1024 * 1024};
        queen::ParallelScheduler<comb::LinearAllocator> scheduler{alloc, 4};
        queen::SystemStorage<comb::LinearAllocator> storage{alloc};

        storage.Register(
            "System1",
            [](queen::World&) {},
            queen::AccessDescriptor<comb::LinearAllocator>{alloc}
        );

        scheduler.Build(storage);
        larvae::AssertFalse(scheduler.NeedsRebuild());

        scheduler.Invalidate();
        larvae::AssertTrue(scheduler.NeedsRebuild());
    });

    auto test10 = larvae::RegisterTest("QueenParallelScheduler", "StressTest", []() {
        comb::LinearAllocator alloc{32 * 1024 * 1024};
        queen::ParallelScheduler<comb::LinearAllocator> scheduler{alloc, 8};
        queen::World world{};
        queen::SystemStorage<comb::LinearAllocator> storage{alloc};

        constexpr int kNumSystems = 50;
        std::atomic<int> counter{0};

        // Register many independent systems
        for (int i = 0; i < kNumSystems; ++i)
        {
            storage.Register(
                ("System" + std::to_string(i)).c_str(),
                [&counter](queen::World&) {
                    counter.fetch_add(1);
                },
                queen::AccessDescriptor<comb::LinearAllocator>{alloc}
            );
        }

        scheduler.Build(storage);
        scheduler.RunAll(world, storage);

        larvae::AssertEqual(counter.load(), kNumSystems);
    });

    auto test11 = larvae::RegisterTest("QueenParallelScheduler", "DiamondDependency", []() {
        comb::LinearAllocator alloc{16 * 1024 * 1024};
        queen::ParallelScheduler<comb::LinearAllocator> scheduler{alloc, 4};
        queen::World world{};
        queen::SystemStorage<comb::LinearAllocator> storage{alloc};

        std::atomic<int> order{0};
        std::atomic<int> root_order{-1};
        std::atomic<int> left_order{-1};
        std::atomic<int> right_order{-1};
        std::atomic<int> bottom_order{-1};

        // Root system: writes Position
        {
            queen::AccessDescriptor<comb::LinearAllocator> root_access{alloc};
            root_access.AddComponentWrite<Position>();
            storage.Register(
                "Root",
                [&order, &root_order](queen::World&) {
                    root_order.store(order.fetch_add(1));
                },
                std::move(root_access)
            );
        }

        // Left system: reads Position, writes Velocity
        {
            queen::AccessDescriptor<comb::LinearAllocator> left_access{alloc};
            left_access.AddComponentRead<Position>();
            left_access.AddComponentWrite<Velocity>();
            storage.Register(
                "Left",
                [&order, &left_order](queen::World&) {
                    left_order.store(order.fetch_add(1));
                },
                std::move(left_access)
            );
        }

        // Right system: reads Position, writes Health
        {
            queen::AccessDescriptor<comb::LinearAllocator> right_access{alloc};
            right_access.AddComponentRead<Position>();
            right_access.AddComponentWrite<Health>();
            storage.Register(
                "Right",
                [&order, &right_order](queen::World&) {
                    right_order.store(order.fetch_add(1));
                },
                std::move(right_access)
            );
        }

        // Bottom system: reads Velocity and Health
        {
            queen::AccessDescriptor<comb::LinearAllocator> bottom_access{alloc};
            bottom_access.AddComponentRead<Velocity>();
            bottom_access.AddComponentRead<Health>();
            storage.Register(
                "Bottom",
                [&order, &bottom_order](queen::World&) {
                    bottom_order.store(order.fetch_add(1));
                },
                std::move(bottom_access)
            );
        }

        scheduler.Build(storage);
        scheduler.RunAll(world, storage);

        // Verify ordering constraints:
        // - Root must run before Left and Right
        // - Left and Right must run before Bottom
        larvae::AssertTrue(root_order.load() < left_order.load());
        larvae::AssertTrue(root_order.load() < right_order.load());
        larvae::AssertTrue(left_order.load() < bottom_order.load());
        larvae::AssertTrue(right_order.load() < bottom_order.load());
    });

    auto test12 = larvae::RegisterTest("QueenParallelScheduler", "GraphAccessor", []() {
        comb::LinearAllocator alloc{8 * 1024 * 1024};
        queen::ParallelScheduler<comb::LinearAllocator> scheduler{alloc, 4};
        queen::SystemStorage<comb::LinearAllocator> storage{alloc};

        storage.Register(
            "System1",
            [](queen::World&) {},
            queen::AccessDescriptor<comb::LinearAllocator>{alloc}
        );

        storage.Register(
            "System2",
            [](queen::World&) {},
            queen::AccessDescriptor<comb::LinearAllocator>{alloc}
        );

        scheduler.Build(storage);

        const auto& graph = scheduler.Graph();
        larvae::AssertEqual(graph.NodeCount(), size_t{2});
    });
}
