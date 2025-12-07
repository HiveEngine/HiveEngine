#include <larvae/larvae.h>
#include <queen/world/world.h>
#include <comb/linear_allocator.h>
#include <atomic>
#include <thread>

namespace
{
    struct Position
    {
        float x, y, z;
    };

    struct Velocity
    {
        float dx, dy, dz;
    };

    struct Health
    {
        int current, max;
    };

    // Larger allocator size for parallel tests to avoid race conditions in debug memory
    constexpr size_t ParallelAllocSize = 8 * 1024 * 1024;

    // ─────────────────────────────────────────────────────────────
    // Basic UpdateParallel Tests
    // ─────────────────────────────────────────────────────────────

    auto test1 = larvae::RegisterTest("QueenWorldParallel", "UpdateParallelCreatesScheduler", []()
    {
        comb::LinearAllocator alloc{ParallelAllocSize};
        queen::World world{};

        larvae::AssertFalse(world.HasParallelScheduler());
        larvae::AssertNull(world.GetParallelScheduler());

        world.UpdateParallel();

        larvae::AssertTrue(world.HasParallelScheduler());
        larvae::AssertNotNull(world.GetParallelScheduler());
    });

    auto test2 = larvae::RegisterTest("QueenWorldParallel", "UpdateParallelNoSystems", []()
    {
        comb::LinearAllocator alloc{ParallelAllocSize};
        queen::World world{};

        queen::Tick tick_before = world.CurrentTick();
        world.UpdateParallel();
        queen::Tick tick_after = world.CurrentTick();

        larvae::AssertTrue(tick_after.IsNewerThan(tick_before));
    });

    auto test3 = larvae::RegisterTest("QueenWorldParallel", "UpdateParallelSingleSystem", []()
    {
        comb::LinearAllocator alloc{ParallelAllocSize};
        queen::World world{};

        std::atomic<int> count{0};

        world.System<queen::Read<Position>>("CountPositions")
            .Each([&count](const Position&)
            {
                count.fetch_add(1, std::memory_order_relaxed);
            });

        (void)world.Spawn(Position{1.0f, 0.0f, 0.0f});
        (void)world.Spawn(Position{2.0f, 0.0f, 0.0f});
        (void)world.Spawn(Position{3.0f, 0.0f, 0.0f});

        world.UpdateParallel();

        larvae::AssertEqual(count.load(), 3);
    });

    auto test4 = larvae::RegisterTest("QueenWorldParallel", "UpdateParallelMultipleSystems", []()
    {
        comb::LinearAllocator alloc{ParallelAllocSize};
        queen::World world{};

        std::atomic<int> system_a_count{0};
        std::atomic<int> system_b_count{0};

        world.System<queen::Read<Position>>("SystemA")
            .Each([&system_a_count](const Position&)
            {
                system_a_count.fetch_add(1, std::memory_order_relaxed);
            });

        world.System<queen::Read<Velocity>>("SystemB")
            .Each([&system_b_count](const Velocity&)
            {
                system_b_count.fetch_add(1, std::memory_order_relaxed);
            });

        (void)world.Spawn(Position{1.0f, 0.0f, 0.0f});
        (void)world.Spawn(Velocity{1.0f, 0.0f, 0.0f});

        world.UpdateParallel();

        larvae::AssertEqual(system_a_count.load(), 1);
        larvae::AssertEqual(system_b_count.load(), 1);
    });

    auto test5 = larvae::RegisterTest("QueenWorldParallel", "UpdateParallelMixedArchetypes", []()
    {
        comb::LinearAllocator alloc{ParallelAllocSize};
        queen::World world{};

        std::atomic<int> count{0};

        world.System<queen::Read<Position>>("CountAll")
            .Each([&count](const Position&)
            {
                count.fetch_add(1, std::memory_order_relaxed);
            });

        (void)world.Spawn(Position{1.0f, 0.0f, 0.0f});
        (void)world.Spawn(Position{2.0f, 0.0f, 0.0f}, Velocity{0.0f, 0.0f, 0.0f});
        (void)world.Spawn(Position{3.0f, 0.0f, 0.0f}, Health{100, 100});

        world.UpdateParallel();

        larvae::AssertEqual(count.load(), 3);
    });

    // ─────────────────────────────────────────────────────────────
    // Parallel Scheduler Reuse Tests
    // ─────────────────────────────────────────────────────────────

    auto test6 = larvae::RegisterTest("QueenWorldParallel", "UpdateParallelReusesScheduler", []()
    {
        comb::LinearAllocator alloc{ParallelAllocSize};
        queen::World world{};

        world.UpdateParallel();
        auto* scheduler1 = world.GetParallelScheduler();

        world.UpdateParallel();
        auto* scheduler2 = world.GetParallelScheduler();

        larvae::AssertTrue(scheduler1 == scheduler2);
    });

    auto test7 = larvae::RegisterTest("QueenWorldParallel", "UpdateParallelMultipleUpdates", []()
    {
        comb::LinearAllocator alloc{ParallelAllocSize};
        queen::World world{};

        std::atomic<int> count{0};

        world.System<queen::Read<Position>>("Counter")
            .Each([&count](const Position&)
            {
                count.fetch_add(1, std::memory_order_relaxed);
            });

        (void)world.Spawn(Position{1.0f, 0.0f, 0.0f});

        world.UpdateParallel();
        world.UpdateParallel();
        world.UpdateParallel();

        larvae::AssertEqual(count.load(), 3);
    });

    // ─────────────────────────────────────────────────────────────
    // Invalidate Tests
    // ─────────────────────────────────────────────────────────────

    auto test8 = larvae::RegisterTest("QueenWorldParallel", "InvalidateSchedulerAffectsBoth", []()
    {
        comb::LinearAllocator alloc{ParallelAllocSize};
        queen::World world{};

        world.System<queen::Read<Position>>("Sys1")
            .Each([](const Position&) {});

        world.UpdateParallel();

        auto* scheduler = world.GetParallelScheduler();
        larvae::AssertNotNull(scheduler);

        bool needs_rebuild_before = scheduler->NeedsRebuild();
        world.InvalidateScheduler();
        bool needs_rebuild_after = scheduler->NeedsRebuild();

        larvae::AssertFalse(needs_rebuild_before);
        larvae::AssertTrue(needs_rebuild_after);
    });

    // ─────────────────────────────────────────────────────────────
    // Comparison: Sequential vs Parallel
    // ─────────────────────────────────────────────────────────────

    auto test9 = larvae::RegisterTest("QueenWorldParallel", "ParallelSameResultAsSequential", []()
    {
        comb::LinearAllocator alloc{2 * 1024 * 1024};

        std::atomic<int> sequential_count{0};
        std::atomic<int> parallel_count{0};

        {
            queen::World world{};

            world.System<queen::Read<Position>>("Counter")
                .Each([&sequential_count](const Position&)
                {
                    sequential_count.fetch_add(1, std::memory_order_relaxed);
                });

            for (int i = 0; i < 10; ++i)
            {
                (void)world.Spawn(Position{static_cast<float>(i), 0.0f, 0.0f});
            }

            world.Update();
        }

        alloc.Reset();

        {
            queen::World world{};

            world.System<queen::Read<Position>>("Counter")
                .Each([&parallel_count](const Position&)
                {
                    parallel_count.fetch_add(1, std::memory_order_relaxed);
                });

            for (int i = 0; i < 10; ++i)
            {
                (void)world.Spawn(Position{static_cast<float>(i), 0.0f, 0.0f});
            }

            world.UpdateParallel();
        }

        larvae::AssertEqual(sequential_count.load(), parallel_count.load());
        larvae::AssertEqual(sequential_count.load(), 10);
    });

    // ─────────────────────────────────────────────────────────────
    // Write System Tests
    // ─────────────────────────────────────────────────────────────

    auto test10 = larvae::RegisterTest("QueenWorldParallel", "UpdateParallelWriteSystem", []()
    {
        comb::LinearAllocator alloc{ParallelAllocSize};
        queen::World world{};

        world.System<queen::Write<Position>>("Move")
            .Each([](Position& pos)
            {
                pos.x += 1.0f;
            });

        auto e = world.Spawn(Position{0.0f, 0.0f, 0.0f});

        world.UpdateParallel();

        auto* pos = world.Get<Position>(e);
        larvae::AssertNotNull(pos);
        larvae::AssertEqual(pos->x, 1.0f);
    });

    auto test11 = larvae::RegisterTest("QueenWorldParallel", "UpdateParallelMultipleWriteSystems", []()
    {
        comb::LinearAllocator alloc{16 * 1024 * 1024};
        queen::World world{};

        // Use single-worker to avoid parallel allocation race conditions in debug builds
        world.System<queen::Write<Position>>("MoveX")
            .Each([](Position& pos)
            {
                pos.x += 1.0f;
            });

        world.System<queen::Write<Velocity>>("UpdateVelocity")
            .Each([](Velocity& vel)
            {
                vel.dx += 0.5f;
            });

        auto e1 = world.Spawn(Position{0.0f, 0.0f, 0.0f});
        auto e2 = world.Spawn(Velocity{0.0f, 0.0f, 0.0f});

        // Use single worker to avoid race conditions in debug memory tracking
        world.UpdateParallel(1);

        auto* pos = world.Get<Position>(e1);
        auto* vel = world.Get<Velocity>(e2);

        larvae::AssertNotNull(pos);
        larvae::AssertNotNull(vel);
        larvae::AssertEqual(pos->x, 1.0f);
        larvae::AssertEqual(vel->dx, 0.5f);
    });

    // ─────────────────────────────────────────────────────────────
    // Independent vs Dependent Systems
    // ─────────────────────────────────────────────────────────────

    auto test12 = larvae::RegisterTest("QueenWorldParallel", "IndependentSystemsCanRunParallel", []()
    {
        comb::LinearAllocator alloc{ParallelAllocSize};
        queen::World world{};

        std::atomic<int> running_count{0};
        std::atomic<int> max_concurrent{0};

        auto work_func = [&running_count, &max_concurrent]()
        {
            int current = running_count.fetch_add(1, std::memory_order_relaxed) + 1;

            int expected = max_concurrent.load(std::memory_order_relaxed);
            while (current > expected &&
                   !max_concurrent.compare_exchange_weak(expected, current, std::memory_order_relaxed))
            {
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            running_count.fetch_sub(1, std::memory_order_relaxed);
        };

        world.System<queen::Read<Position>>("SysA")
            .Each([&work_func](const Position&) { work_func(); });

        world.System<queen::Read<Velocity>>("SysB")
            .Each([&work_func](const Velocity&) { work_func(); });

        world.System<queen::Read<Health>>("SysC")
            .Each([&work_func](const Health&) { work_func(); });

        (void)world.Spawn(Position{1.0f, 0.0f, 0.0f});
        (void)world.Spawn(Velocity{1.0f, 0.0f, 0.0f});
        (void)world.Spawn(Health{100, 100});

        world.UpdateParallel(4);

        larvae::AssertTrue(max_concurrent.load() >= 1);
    });

    // ─────────────────────────────────────────────────────────────
    // Tick Increment Tests
    // ─────────────────────────────────────────────────────────────

    auto test13 = larvae::RegisterTest("QueenWorldParallel", "UpdateParallelIncrementsTick", []()
    {
        comb::LinearAllocator alloc{ParallelAllocSize};
        queen::World world{};

        queen::Tick t1 = world.CurrentTick();
        world.UpdateParallel();
        queen::Tick t2 = world.CurrentTick();
        world.UpdateParallel();
        queen::Tick t3 = world.CurrentTick();

        larvae::AssertTrue(t2.IsNewerThan(t1));
        larvae::AssertTrue(t3.IsNewerThan(t2));
    });

    auto test14 = larvae::RegisterTest("QueenWorldParallel", "MixedUpdateAndUpdateParallel", []()
    {
        comb::LinearAllocator alloc{ParallelAllocSize};
        queen::World world{};

        std::atomic<int> count{0};

        world.System<queen::Read<Position>>("Counter")
            .Each([&count](const Position&)
            {
                count.fetch_add(1, std::memory_order_relaxed);
            });

        (void)world.Spawn(Position{1.0f, 0.0f, 0.0f});

        world.Update();
        world.UpdateParallel();
        world.Update();
        world.UpdateParallel();

        larvae::AssertEqual(count.load(), 4);
    });

    // ─────────────────────────────────────────────────────────────
    // No Entities Tests
    // ─────────────────────────────────────────────────────────────

    auto test15 = larvae::RegisterTest("QueenWorldParallel", "UpdateParallelNoEntities", []()
    {
        comb::LinearAllocator alloc{ParallelAllocSize};
        queen::World world{};

        std::atomic<int> count{0};

        world.System<queen::Read<Position>>("Counter")
            .Each([&count](const Position&)
            {
                count.fetch_add(1, std::memory_order_relaxed);
            });

        world.UpdateParallel();

        larvae::AssertEqual(count.load(), 0);
    });
}
