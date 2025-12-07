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

    auto test1 = larvae::RegisterTest("QueenWorld", "Creation", []() {
        comb::LinearAllocator alloc{65536};

        queen::World world{};

        larvae::AssertEqual(world.EntityCount(), size_t{0});
        larvae::AssertEqual(world.ArchetypeCount(), size_t{1});
    });

    auto test2 = larvae::RegisterTest("QueenWorld", "SpawnEmpty", []() {
        comb::LinearAllocator alloc{65536};

        queen::World world{};

        queen::Entity e = world.Spawn().Build();

        larvae::AssertFalse(e.IsNull());
        larvae::AssertTrue(world.IsAlive(e));
        larvae::AssertEqual(world.EntityCount(), size_t{1});
    });

    auto test3 = larvae::RegisterTest("QueenWorld", "SpawnWithComponent", []() {
        comb::LinearAllocator alloc{131072};

        queen::World world{};

        queen::Entity e = world.Spawn()
            .With(Position{1.0f, 2.0f, 3.0f})
            .Build();

        larvae::AssertTrue(world.Has<Position>(e));

        Position* pos = world.Get<Position>(e);
        larvae::AssertNotNull(pos);
        larvae::AssertEqual(pos->x, 1.0f);
        larvae::AssertEqual(pos->y, 2.0f);
        larvae::AssertEqual(pos->z, 3.0f);
    });

    auto test4 = larvae::RegisterTest("QueenWorld", "SpawnWithMultipleComponents", []() {
        comb::LinearAllocator alloc{131072};

        queen::World world{};

        queen::Entity e = world.Spawn()
            .With(Position{1.0f, 2.0f, 3.0f})
            .With(Velocity{0.1f, 0.2f, 0.3f})
            .Build();

        larvae::AssertTrue(world.Has<Position>(e));
        larvae::AssertTrue(world.Has<Velocity>(e));

        Position* pos = world.Get<Position>(e);
        Velocity* vel = world.Get<Velocity>(e);

        larvae::AssertNotNull(pos);
        larvae::AssertNotNull(vel);
        larvae::AssertEqual(pos->x, 1.0f);
        larvae::AssertEqual(vel->dx, 0.1f);
    });

    auto test5 = larvae::RegisterTest("QueenWorld", "SpawnVariadic", []() {
        comb::LinearAllocator alloc{131072};

        queen::World world{};

        queen::Entity e = world.Spawn(Position{5.0f, 6.0f, 7.0f}, Velocity{1.0f, 0.0f, 0.0f});

        larvae::AssertTrue(world.Has<Position>(e));
        larvae::AssertTrue(world.Has<Velocity>(e));
        larvae::AssertEqual(world.Get<Position>(e)->x, 5.0f);
        larvae::AssertEqual(world.Get<Velocity>(e)->dx, 1.0f);
    });

    auto test6 = larvae::RegisterTest("QueenWorld", "Despawn", []() {
        comb::LinearAllocator alloc{131072};

        queen::World world{};

        queen::Entity e = world.Spawn(Position{1.0f, 2.0f, 3.0f});

        larvae::AssertTrue(world.IsAlive(e));
        larvae::AssertEqual(world.EntityCount(), size_t{1});

        world.Despawn(e);

        larvae::AssertFalse(world.IsAlive(e));
        larvae::AssertEqual(world.EntityCount(), size_t{0});
    });

    auto test7 = larvae::RegisterTest("QueenWorld", "GetNonExistentComponent", []() {
        comb::LinearAllocator alloc{131072};

        queen::World world{};

        queen::Entity e = world.Spawn(Position{1.0f, 2.0f, 3.0f});

        larvae::AssertFalse(world.Has<Velocity>(e));
        larvae::AssertNull(world.Get<Velocity>(e));
    });

    auto test8 = larvae::RegisterTest("QueenWorld", "AddComponent", []() {
        comb::LinearAllocator alloc{262144};

        queen::World world{};

        queen::Entity e = world.Spawn(Position{1.0f, 2.0f, 3.0f});

        larvae::AssertFalse(world.Has<Velocity>(e));

        world.Add<Velocity>(e, Velocity{0.5f, 0.6f, 0.7f});

        larvae::AssertTrue(world.Has<Velocity>(e));
        larvae::AssertEqual(world.Get<Velocity>(e)->dx, 0.5f);

        larvae::AssertTrue(world.Has<Position>(e));
        larvae::AssertEqual(world.Get<Position>(e)->x, 1.0f);
    });

    auto test9 = larvae::RegisterTest("QueenWorld", "RemoveComponent", []() {
        comb::LinearAllocator alloc{262144};

        queen::World world{};

        queen::Entity e = world.Spawn(Position{1.0f, 2.0f, 3.0f}, Velocity{0.1f, 0.2f, 0.3f});

        larvae::AssertTrue(world.Has<Position>(e));
        larvae::AssertTrue(world.Has<Velocity>(e));

        world.Remove<Velocity>(e);

        larvae::AssertTrue(world.Has<Position>(e));
        larvae::AssertFalse(world.Has<Velocity>(e));
        larvae::AssertEqual(world.Get<Position>(e)->x, 1.0f);
    });

    auto test10 = larvae::RegisterTest("QueenWorld", "SetComponent", []() {
        comb::LinearAllocator alloc{262144};

        queen::World world{};

        queen::Entity e = world.Spawn(Position{1.0f, 2.0f, 3.0f});

        world.Set<Position>(e, Position{10.0f, 20.0f, 30.0f});

        larvae::AssertEqual(world.Get<Position>(e)->x, 10.0f);
        larvae::AssertEqual(world.Get<Position>(e)->y, 20.0f);
    });

    auto test11 = larvae::RegisterTest("QueenWorld", "MultipleEntities", []() {
        comb::LinearAllocator alloc{262144};

        queen::World world{};

        queen::Entity e1 = world.Spawn(Position{1.0f, 0.0f, 0.0f});
        queen::Entity e2 = world.Spawn(Position{2.0f, 0.0f, 0.0f});
        queen::Entity e3 = world.Spawn(Position{3.0f, 0.0f, 0.0f});

        larvae::AssertEqual(world.EntityCount(), size_t{3});

        larvae::AssertEqual(world.Get<Position>(e1)->x, 1.0f);
        larvae::AssertEqual(world.Get<Position>(e2)->x, 2.0f);
        larvae::AssertEqual(world.Get<Position>(e3)->x, 3.0f);
    });

    auto test12 = larvae::RegisterTest("QueenWorld", "DespawnMiddleEntity", []() {
        comb::LinearAllocator alloc{262144};

        queen::World world{};

        queen::Entity e1 = world.Spawn(Position{1.0f, 0.0f, 0.0f});
        queen::Entity e2 = world.Spawn(Position{2.0f, 0.0f, 0.0f});
        queen::Entity e3 = world.Spawn(Position{3.0f, 0.0f, 0.0f});

        world.Despawn(e2);

        larvae::AssertEqual(world.EntityCount(), size_t{2});
        larvae::AssertTrue(world.IsAlive(e1));
        larvae::AssertFalse(world.IsAlive(e2));
        larvae::AssertTrue(world.IsAlive(e3));

        larvae::AssertEqual(world.Get<Position>(e1)->x, 1.0f);
        larvae::AssertEqual(world.Get<Position>(e3)->x, 3.0f);
    });

    auto test13 = larvae::RegisterTest("QueenWorld", "DeadEntityOperations", []() {
        comb::LinearAllocator alloc{131072};

        queen::World world{};

        queen::Entity e = world.Spawn(Position{1.0f, 2.0f, 3.0f});
        world.Despawn(e);

        larvae::AssertFalse(world.Has<Position>(e));
        larvae::AssertNull(world.Get<Position>(e));
    });

    auto test14 = larvae::RegisterTest("QueenWorld", "ArchetypeTransitions", []() {
        comb::LinearAllocator alloc{524288};

        queen::World world{};

        queen::Entity e = world.Spawn().Build();

        world.Add<Position>(e, Position{1.0f, 2.0f, 3.0f});
        larvae::AssertTrue(world.Has<Position>(e));

        world.Add<Velocity>(e, Velocity{0.1f, 0.2f, 0.3f});
        larvae::AssertTrue(world.Has<Position>(e));
        larvae::AssertTrue(world.Has<Velocity>(e));

        world.Add<Health>(e, Health{100, 100});
        larvae::AssertTrue(world.Has<Position>(e));
        larvae::AssertTrue(world.Has<Velocity>(e));
        larvae::AssertTrue(world.Has<Health>(e));

        larvae::AssertEqual(world.Get<Position>(e)->x, 1.0f);
        larvae::AssertEqual(world.Get<Velocity>(e)->dx, 0.1f);
        larvae::AssertEqual(world.Get<Health>(e)->current, 100);

        world.Remove<Velocity>(e);
        larvae::AssertTrue(world.Has<Position>(e));
        larvae::AssertFalse(world.Has<Velocity>(e));
        larvae::AssertTrue(world.Has<Health>(e));
    });

    auto test15 = larvae::RegisterTest("QueenWorld", "EntityRecycling", []() {
        comb::LinearAllocator alloc{131072};

        queen::World world{};

        queen::Entity e1 = world.Spawn(Position{1.0f, 0.0f, 0.0f});
        uint32_t index1 = e1.Index();
        uint16_t gen1 = e1.Generation();

        world.Despawn(e1);

        queen::Entity e2 = world.Spawn(Position{2.0f, 0.0f, 0.0f});
        uint32_t index2 = e2.Index();
        uint16_t gen2 = e2.Generation();

        larvae::AssertEqual(index1, index2);
        larvae::AssertEqual(gen2, static_cast<uint16_t>(gen1 + 1));

        larvae::AssertFalse(world.IsAlive(e1));
        larvae::AssertTrue(world.IsAlive(e2));

        larvae::AssertNull(world.Get<Position>(e1));
        larvae::AssertNotNull(world.Get<Position>(e2));
        larvae::AssertEqual(world.Get<Position>(e2)->x, 2.0f);
    });
}
