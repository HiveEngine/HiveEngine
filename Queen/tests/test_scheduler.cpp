#include <larvae/larvae.h>
#include <queen/world/world.h>
#include <comb/linear_allocator.h>

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

    struct Tag {};

    // ─────────────────────────────────────────────────────────────
    // SystemNode Tests
    // ─────────────────────────────────────────────────────────────

    auto test1 = larvae::RegisterTest("QueenScheduler", "SystemNodeDefault", []() {
        queen::SystemNode node{};

        larvae::AssertFalse(node.Id().IsValid());
        larvae::AssertTrue(node.State() == queen::SystemState::Pending);
        larvae::AssertEqual(node.DependencyCount(), uint16_t{0});
        larvae::AssertEqual(node.UnfinishedDeps(), uint16_t{0});
    });

    auto test2 = larvae::RegisterTest("QueenScheduler", "SystemNodeWithId", []() {
        queen::SystemId id{42};
        queen::SystemNode node{id};

        larvae::AssertTrue(node.Id().IsValid());
        larvae::AssertEqual(node.Id().Index(), uint32_t{42});
    });

    auto test3 = larvae::RegisterTest("QueenScheduler", "SystemNodeDependencyCount", []() {
        queen::SystemNode node{queen::SystemId{0}};

        node.SetDependencyCount(3);

        larvae::AssertEqual(node.DependencyCount(), uint16_t{3});
        larvae::AssertEqual(node.UnfinishedDeps(), uint16_t{3});
    });

    auto test4 = larvae::RegisterTest("QueenScheduler", "SystemNodeDecrementDeps", []() {
        queen::SystemNode node{queen::SystemId{0}};
        node.SetDependencyCount(2);

        larvae::AssertFalse(node.DecrementDeps());
        larvae::AssertEqual(node.UnfinishedDeps(), uint16_t{1});

        larvae::AssertTrue(node.DecrementDeps());
        larvae::AssertEqual(node.UnfinishedDeps(), uint16_t{0});
    });

    auto test5 = larvae::RegisterTest("QueenScheduler", "SystemNodeReset", []() {
        queen::SystemNode node{queen::SystemId{0}};
        node.SetDependencyCount(3);

        node.DecrementDeps();
        node.DecrementDeps();
        node.SetState(queen::SystemState::Complete);

        node.Reset();

        larvae::AssertTrue(node.State() == queen::SystemState::Pending);
        larvae::AssertEqual(node.UnfinishedDeps(), uint16_t{3});
    });

    auto test6 = larvae::RegisterTest("QueenScheduler", "SystemNodeIsReady", []() {
        queen::SystemNode node{queen::SystemId{0}};

        larvae::AssertTrue(node.IsReady());

        node.SetDependencyCount(1);
        larvae::AssertFalse(node.IsReady());

        node.DecrementDeps();
        larvae::AssertTrue(node.IsReady());

        node.SetState(queen::SystemState::Running);
        larvae::AssertFalse(node.IsReady());
    });

    // ─────────────────────────────────────────────────────────────
    // DependencyGraph Tests
    // ─────────────────────────────────────────────────────────────

    auto test7 = larvae::RegisterTest("QueenScheduler", "DependencyGraphEmpty", []() {
        comb::LinearAllocator alloc{262144};
        queen::World world{};

        auto& scheduler = world.GetScheduler();
        scheduler.Build(world.GetSystemStorage());

        larvae::AssertEqual(scheduler.Graph().NodeCount(), size_t{0});
        larvae::AssertFalse(scheduler.HasCycle());
    });

    auto test8 = larvae::RegisterTest("QueenScheduler", "DependencyGraphSingleSystem", []() {
        comb::LinearAllocator alloc{262144};
        queen::World world{};

        world.System<queen::Read<Position>>("TestSystem")
            .Each([](const Position&) {});

        auto& scheduler = world.GetScheduler();
        scheduler.Build(world.GetSystemStorage());

        larvae::AssertEqual(scheduler.Graph().NodeCount(), size_t{1});
        larvae::AssertEqual(scheduler.ExecutionOrder().Size(), size_t{1});
        larvae::AssertFalse(scheduler.HasCycle());
    });

    auto test9 = larvae::RegisterTest("QueenScheduler", "DependencyGraphNoConflict", []() {
        comb::LinearAllocator alloc{262144};
        queen::World world{};

        world.System<queen::Read<Position>>("System1")
            .Each([](const Position&) {});

        world.System<queen::Read<Velocity>>("System2")
            .Each([](const Velocity&) {});

        auto& scheduler = world.GetScheduler();
        scheduler.Build(world.GetSystemStorage());

        larvae::AssertEqual(scheduler.Graph().NodeCount(), size_t{2});
        larvae::AssertEqual(scheduler.ExecutionOrder().Size(), size_t{2});
        larvae::AssertEqual(scheduler.Graph().Roots().Size(), size_t{2});
    });

    auto test10 = larvae::RegisterTest("QueenScheduler", "DependencyGraphConflict", []() {
        comb::LinearAllocator alloc{262144};
        queen::World world{};

        world.System<queen::Write<Position>>("Writer")
            .Each([](Position&) {});

        world.System<queen::Read<Position>>("Reader")
            .Each([](const Position&) {});

        auto& scheduler = world.GetScheduler();
        scheduler.Build(world.GetSystemStorage());

        larvae::AssertEqual(scheduler.Graph().NodeCount(), size_t{2});
        larvae::AssertEqual(scheduler.Graph().Roots().Size(), size_t{1});

        const auto& order = scheduler.ExecutionOrder();
        larvae::AssertEqual(order.Size(), size_t{2});
        larvae::AssertEqual(order[0], uint32_t{0});
        larvae::AssertEqual(order[1], uint32_t{1});
    });

    auto test11 = larvae::RegisterTest("QueenScheduler", "DependencyGraphChain", []() {
        comb::LinearAllocator alloc{262144};
        queen::World world{};

        world.System<queen::Write<Position>>("A")
            .Each([](Position&) {});

        world.System<queen::Read<Position>, queen::Write<Velocity>>("B")
            .Each([](const Position&, Velocity&) {});

        world.System<queen::Read<Velocity>>("C")
            .Each([](const Velocity&) {});

        auto& scheduler = world.GetScheduler();
        scheduler.Build(world.GetSystemStorage());

        const auto& order = scheduler.ExecutionOrder();
        larvae::AssertEqual(order.Size(), size_t{3});
        larvae::AssertEqual(order[0], uint32_t{0});
        larvae::AssertEqual(order[1], uint32_t{1});
        larvae::AssertEqual(order[2], uint32_t{2});
    });

    // ─────────────────────────────────────────────────────────────
    // Scheduler Execution Tests
    // ─────────────────────────────────────────────────────────────

    auto test12 = larvae::RegisterTest("QueenScheduler", "SchedulerRunAll", []() {
        comb::LinearAllocator alloc{262144};
        queen::World world{};

        world.Spawn(Position{1.0f, 0.0f, 0.0f});

        int count = 0;

        world.System<queen::Read<Position>>("Counter")
            .Each([&](const Position&) {
                ++count;
            });

        world.Update();

        larvae::AssertEqual(count, 1);
    });

    auto test13 = larvae::RegisterTest("QueenScheduler", "SchedulerExecutionOrder", []() {
        comb::LinearAllocator alloc{262144};
        queen::World world{};

        queen::Entity e = world.Spawn(Position{1.0f, 0.0f, 0.0f});

        world.System<queen::Write<Position>>("Add")
            .Each([](Position& pos) {
                pos.x += 1.0f;
            });

        world.System<queen::Write<Position>>("Multiply")
            .Each([](Position& pos) {
                pos.x *= 2.0f;
            });

        world.Update();

        Position* pos = world.Get<Position>(e);
        larvae::AssertEqual(pos->x, 4.0f);
    });

    auto test14 = larvae::RegisterTest("QueenScheduler", "SchedulerWithDependencies", []() {
        comb::LinearAllocator alloc{262144};
        queen::World world{};

        queen::Entity e = world.Spawn(Position{0.0f, 0.0f, 0.0f}, Velocity{1.0f, 2.0f, 3.0f});

        world.System<queen::Read<Velocity>, queen::Write<Position>>("ApplyVelocity")
            .Each([](const Velocity& vel, Position& pos) {
                pos.x += vel.dx;
                pos.y += vel.dy;
                pos.z += vel.dz;
            });

        world.System<queen::Read<Position>>("CheckPosition")
            .EachWithEntity([&](queen::Entity, const Position& pos) {
                larvae::AssertEqual(pos.x, 1.0f);
                larvae::AssertEqual(pos.y, 2.0f);
                larvae::AssertEqual(pos.z, 3.0f);
            });

        world.Update();

        Position* pos = world.Get<Position>(e);
        larvae::AssertEqual(pos->x, 1.0f);
    });

    auto test15 = larvae::RegisterTest("QueenScheduler", "SchedulerMultipleUpdates", []() {
        comb::LinearAllocator alloc{262144};
        queen::World world{};

        queen::Entity e = world.Spawn(Position{0.0f, 0.0f, 0.0f});

        world.System<queen::Write<Position>>("Increment")
            .Each([](Position& pos) {
                pos.x += 1.0f;
            });

        world.Update();
        world.Update();
        world.Update();

        Position* pos = world.Get<Position>(e);
        larvae::AssertEqual(pos->x, 3.0f);
    });

    auto test16 = larvae::RegisterTest("QueenScheduler", "SchedulerDisabledSystem", []() {
        comb::LinearAllocator alloc{262144};
        queen::World world{};

        world.Spawn(Position{1.0f, 0.0f, 0.0f});

        int count = 0;

        queen::SystemId id = world.System<queen::Read<Position>>("Disabled")
            .Each([&](const Position&) {
                ++count;
            });

        world.SetSystemEnabled(id, false);
        world.Update();

        larvae::AssertEqual(count, 0);
    });

    auto test17 = larvae::RegisterTest("QueenScheduler", "SchedulerInvalidateRebuild", []() {
        comb::LinearAllocator alloc{262144};
        queen::World world{};

        world.System<queen::Read<Position>>("First")
            .Each([](const Position&) {});

        world.Update();

        larvae::AssertFalse(world.GetScheduler().NeedsRebuild());

        world.InvalidateScheduler();
        larvae::AssertTrue(world.GetScheduler().NeedsRebuild());

        world.Update();
        larvae::AssertFalse(world.GetScheduler().NeedsRebuild());
    });

    auto test18 = larvae::RegisterTest("QueenScheduler", "SchedulerComplexGraph", []() {
        comb::LinearAllocator alloc{262144};
        queen::World world{};

        queen::Entity e = world.Spawn(Position{0.0f, 0.0f, 0.0f}, Velocity{0.0f, 0.0f, 0.0f}, Health{100, 100});

        world.System<queen::Write<Position>>("SetPosition")
            .Each([](Position& pos) {
                pos.x = 1.0f;
            });

        world.System<queen::Write<Velocity>>("SetVelocity")
            .Each([](Velocity& vel) {
                vel.dx = 2.0f;
            });

        world.System<queen::Read<Position>, queen::Read<Velocity>, queen::Write<Health>>("CombineIntoHealth")
            .Each([](const Position& pos, const Velocity& vel, Health& hp) {
                hp.current = static_cast<int>(pos.x + vel.dx);
            });

        world.Update();

        Health* hp = world.Get<Health>(e);
        larvae::AssertEqual(hp->current, 3);
    });

    auto test19 = larvae::RegisterTest("QueenScheduler", "SchedulerParallelBranches", []() {
        comb::LinearAllocator alloc{262144};
        queen::World world{};

        int order_a = -1;
        int order_b = -1;
        int counter = 0;

        world.System<queen::Read<Position>>("BranchA")
            .Each([&](const Position&) {
                order_a = counter++;
            });

        world.System<queen::Read<Velocity>>("BranchB")
            .Each([&](const Velocity&) {
                order_b = counter++;
            });

        world.Spawn(Position{1.0f, 0.0f, 0.0f});
        world.Spawn(Velocity{1.0f, 0.0f, 0.0f});

        world.Update();

        larvae::AssertTrue(order_a >= 0);
        larvae::AssertTrue(order_b >= 0);
    });

    auto test20 = larvae::RegisterTest("QueenScheduler", "UpdateVsRunAllSystems", []() {
        comb::LinearAllocator alloc{262144};
        queen::World world{};

        queen::Entity e = world.Spawn(Position{0.0f, 0.0f, 0.0f});

        world.System<queen::Write<Position>>("Increment")
            .Each([](Position& pos) {
                pos.x += 1.0f;
            });

        world.Update();
        Position* pos1 = world.Get<Position>(e);
        float after_update = pos1->x;

        world.RunAllSystems();
        Position* pos2 = world.Get<Position>(e);
        float after_run_all = pos2->x;

        larvae::AssertEqual(after_update, 1.0f);
        larvae::AssertEqual(after_run_all, 2.0f);
    });
}
