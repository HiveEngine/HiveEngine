#include <larvae/larvae.h>
#include <queen/command/command_buffer.h>
#include <queen/world/world.h>
#include <queen/query/query.h>
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

    // ─────────────────────────────────────────────────────────────────────────
    // Basic CommandBuffer Tests
    // ─────────────────────────────────────────────────────────────────────────

    auto test1 = larvae::RegisterTest("QueenCommandBuffer", "Creation", []() {
        comb::LinearAllocator alloc{65536};

        queen::CommandBuffer<comb::LinearAllocator> cmd{alloc};

        larvae::AssertEqual(cmd.CommandCount(), size_t{0});
        larvae::AssertTrue(cmd.IsEmpty());
    });

    auto test2 = larvae::RegisterTest("QueenCommandBuffer", "SpawnCommand", []() {
        comb::LinearAllocator alloc{65536};

        queen::CommandBuffer<comb::LinearAllocator> cmd{alloc};

        auto builder = cmd.Spawn();

        larvae::AssertEqual(cmd.CommandCount(), size_t{1});
        larvae::AssertFalse(cmd.IsEmpty());
        larvae::AssertEqual(builder.GetSpawnIndex(), uint32_t{0});
    });

    auto test3 = larvae::RegisterTest("QueenCommandBuffer", "SpawnWithComponents", []() {
        comb::LinearAllocator alloc{65536};

        queen::CommandBuffer<comb::LinearAllocator> cmd{alloc};

        cmd.Spawn()
            .With(Position{1.0f, 2.0f, 3.0f})
            .With(Velocity{0.1f, 0.2f, 0.3f});

        larvae::AssertEqual(cmd.CommandCount(), size_t{3});
    });

    auto test4 = larvae::RegisterTest("QueenCommandBuffer", "DespawnCommand", []() {
        comb::LinearAllocator alloc{65536};

        queen::CommandBuffer<comb::LinearAllocator> cmd{alloc};

        queen::Entity e{42, 1, queen::Entity::Flags::kAlive};
        cmd.Despawn(e);

        larvae::AssertEqual(cmd.CommandCount(), size_t{1});
    });

    auto test5 = larvae::RegisterTest("QueenCommandBuffer", "AddCommand", []() {
        comb::LinearAllocator alloc{65536};

        queen::CommandBuffer<comb::LinearAllocator> cmd{alloc};

        queen::Entity e{42, 1, queen::Entity::Flags::kAlive};
        cmd.Add(e, Position{1.0f, 2.0f, 3.0f});

        larvae::AssertEqual(cmd.CommandCount(), size_t{1});
    });

    auto test6 = larvae::RegisterTest("QueenCommandBuffer", "RemoveCommand", []() {
        comb::LinearAllocator alloc{65536};

        queen::CommandBuffer<comb::LinearAllocator> cmd{alloc};

        queen::Entity e{42, 1, queen::Entity::Flags::kAlive};
        cmd.Remove<Position>(e);

        larvae::AssertEqual(cmd.CommandCount(), size_t{1});
    });

    auto test7 = larvae::RegisterTest("QueenCommandBuffer", "SetCommand", []() {
        comb::LinearAllocator alloc{65536};

        queen::CommandBuffer<comb::LinearAllocator> cmd{alloc};

        queen::Entity e{42, 1, queen::Entity::Flags::kAlive};
        cmd.Set(e, Position{1.0f, 2.0f, 3.0f});

        larvae::AssertEqual(cmd.CommandCount(), size_t{1});
    });

    auto test8 = larvae::RegisterTest("QueenCommandBuffer", "Clear", []() {
        comb::LinearAllocator alloc{65536};

        queen::CommandBuffer<comb::LinearAllocator> cmd{alloc};

        cmd.Spawn().With(Position{1.0f, 2.0f, 3.0f});
        larvae::AssertEqual(cmd.CommandCount(), size_t{2});

        cmd.Clear();

        larvae::AssertEqual(cmd.CommandCount(), size_t{0});
        larvae::AssertTrue(cmd.IsEmpty());
    });

    // ─────────────────────────────────────────────────────────────────────────
    // Flush Tests - Spawn
    // ─────────────────────────────────────────────────────────────────────────

    auto test9 = larvae::RegisterTest("QueenCommandBuffer", "FlushSpawnEmpty", []() {
        comb::LinearAllocator alloc{131072};

        queen::World world{};
        queen::CommandBuffer<comb::LinearAllocator> cmd{alloc};

        larvae::AssertEqual(world.EntityCount(), size_t{0});

        auto builder = cmd.Spawn();
        cmd.Flush(world);

        larvae::AssertEqual(world.EntityCount(), size_t{1});

        queen::Entity spawned = cmd.GetSpawnedEntity(builder.GetSpawnIndex());
        larvae::AssertTrue(world.IsAlive(spawned));
    });

    auto test10 = larvae::RegisterTest("QueenCommandBuffer", "FlushSpawnWithComponent", []() {
        comb::LinearAllocator alloc{131072};

        queen::World world{};
        queen::CommandBuffer<comb::LinearAllocator> cmd{alloc};

        auto builder = cmd.Spawn()
            .With(Position{1.0f, 2.0f, 3.0f});

        cmd.Flush(world);

        queen::Entity spawned = cmd.GetSpawnedEntity(builder.GetSpawnIndex());
        larvae::AssertTrue(world.IsAlive(spawned));
        larvae::AssertTrue(world.Has<Position>(spawned));

        Position* pos = world.Get<Position>(spawned);
        larvae::AssertNotNull(pos);
        larvae::AssertEqual(pos->x, 1.0f);
        larvae::AssertEqual(pos->y, 2.0f);
        larvae::AssertEqual(pos->z, 3.0f);
    });

    auto test11 = larvae::RegisterTest("QueenCommandBuffer", "FlushSpawnWithMultipleComponents", []() {
        comb::LinearAllocator alloc{131072};

        queen::World world{};
        queen::CommandBuffer<comb::LinearAllocator> cmd{alloc};

        auto builder = cmd.Spawn()
            .With(Position{1.0f, 2.0f, 3.0f})
            .With(Velocity{0.1f, 0.2f, 0.3f})
            .With(Health{100, 100});

        cmd.Flush(world);

        queen::Entity spawned = cmd.GetSpawnedEntity(builder.GetSpawnIndex());
        larvae::AssertTrue(world.IsAlive(spawned));
        larvae::AssertTrue(world.Has<Position>(spawned));
        larvae::AssertTrue(world.Has<Velocity>(spawned));
        larvae::AssertTrue(world.Has<Health>(spawned));

        larvae::AssertEqual(world.Get<Position>(spawned)->x, 1.0f);
        larvae::AssertEqual(world.Get<Velocity>(spawned)->dx, 0.1f);
        larvae::AssertEqual(world.Get<Health>(spawned)->current, 100);
    });

    auto test12 = larvae::RegisterTest("QueenCommandBuffer", "FlushMultipleSpawns", []() {
        comb::LinearAllocator alloc{131072};

        queen::World world{};
        queen::CommandBuffer<comb::LinearAllocator> cmd{alloc};

        auto b1 = cmd.Spawn().With(Position{1.0f, 0.0f, 0.0f});
        auto b2 = cmd.Spawn().With(Position{2.0f, 0.0f, 0.0f});
        auto b3 = cmd.Spawn().With(Position{3.0f, 0.0f, 0.0f});

        cmd.Flush(world);

        larvae::AssertEqual(world.EntityCount(), size_t{3});

        queen::Entity e1 = cmd.GetSpawnedEntity(b1.GetSpawnIndex());
        queen::Entity e2 = cmd.GetSpawnedEntity(b2.GetSpawnIndex());
        queen::Entity e3 = cmd.GetSpawnedEntity(b3.GetSpawnIndex());

        larvae::AssertEqual(world.Get<Position>(e1)->x, 1.0f);
        larvae::AssertEqual(world.Get<Position>(e2)->x, 2.0f);
        larvae::AssertEqual(world.Get<Position>(e3)->x, 3.0f);
    });

    // ─────────────────────────────────────────────────────────────────────────
    // Flush Tests - Despawn
    // ─────────────────────────────────────────────────────────────────────────

    auto test13 = larvae::RegisterTest("QueenCommandBuffer", "FlushDespawn", []() {
        comb::LinearAllocator alloc{131072};

        queen::World world{};

        queen::Entity e = world.Spawn(Position{1.0f, 2.0f, 3.0f});
        larvae::AssertTrue(world.IsAlive(e));
        larvae::AssertEqual(world.EntityCount(), size_t{1});

        queen::CommandBuffer<comb::LinearAllocator> cmd{alloc};
        cmd.Despawn(e);
        cmd.Flush(world);

        larvae::AssertFalse(world.IsAlive(e));
        larvae::AssertEqual(world.EntityCount(), size_t{0});
    });

    auto test14 = larvae::RegisterTest("QueenCommandBuffer", "FlushDespawnMultiple", []() {
        comb::LinearAllocator alloc{131072};

        queen::World world{};

        queen::Entity e1 = world.Spawn(Position{1.0f, 0.0f, 0.0f});
        queen::Entity e2 = world.Spawn(Position{2.0f, 0.0f, 0.0f});
        queen::Entity e3 = world.Spawn(Position{3.0f, 0.0f, 0.0f});

        larvae::AssertEqual(world.EntityCount(), size_t{3});

        queen::CommandBuffer<comb::LinearAllocator> cmd{alloc};
        cmd.Despawn(e1);
        cmd.Despawn(e3);
        cmd.Flush(world);

        larvae::AssertFalse(world.IsAlive(e1));
        larvae::AssertTrue(world.IsAlive(e2));
        larvae::AssertFalse(world.IsAlive(e3));
        larvae::AssertEqual(world.EntityCount(), size_t{1});
    });

    // ─────────────────────────────────────────────────────────────────────────
    // Flush Tests - Add Component
    // ─────────────────────────────────────────────────────────────────────────

    auto test15 = larvae::RegisterTest("QueenCommandBuffer", "FlushAddComponent", []() {
        comb::LinearAllocator alloc{131072};

        queen::World world{};

        queen::Entity e = world.Spawn(Position{1.0f, 2.0f, 3.0f});
        larvae::AssertTrue(world.Has<Position>(e));
        larvae::AssertFalse(world.Has<Velocity>(e));

        queen::CommandBuffer<comb::LinearAllocator> cmd{alloc};
        cmd.Add(e, Velocity{0.1f, 0.2f, 0.3f});
        cmd.Flush(world);

        larvae::AssertTrue(world.Has<Position>(e));
        larvae::AssertTrue(world.Has<Velocity>(e));

        larvae::AssertEqual(world.Get<Position>(e)->x, 1.0f);
        larvae::AssertEqual(world.Get<Velocity>(e)->dx, 0.1f);
    });

    auto test16 = larvae::RegisterTest("QueenCommandBuffer", "FlushAddExistingComponent", []() {
        comb::LinearAllocator alloc{131072};

        queen::World world{};

        queen::Entity e = world.Spawn(Position{1.0f, 2.0f, 3.0f});
        larvae::AssertEqual(world.Get<Position>(e)->x, 1.0f);

        queen::CommandBuffer<comb::LinearAllocator> cmd{alloc};
        cmd.Add(e, Position{5.0f, 6.0f, 7.0f});
        cmd.Flush(world);

        larvae::AssertEqual(world.Get<Position>(e)->x, 5.0f);
        larvae::AssertEqual(world.Get<Position>(e)->y, 6.0f);
    });

    // ─────────────────────────────────────────────────────────────────────────
    // Flush Tests - Remove Component
    // ─────────────────────────────────────────────────────────────────────────

    auto test17 = larvae::RegisterTest("QueenCommandBuffer", "FlushRemoveComponent", []() {
        comb::LinearAllocator alloc{131072};

        queen::World world{};

        queen::Entity e = world.Spawn(Position{1.0f, 2.0f, 3.0f}, Velocity{0.1f, 0.2f, 0.3f});
        larvae::AssertTrue(world.Has<Position>(e));
        larvae::AssertTrue(world.Has<Velocity>(e));

        queen::CommandBuffer<comb::LinearAllocator> cmd{alloc};
        cmd.Remove<Velocity>(e);
        cmd.Flush(world);

        larvae::AssertTrue(world.Has<Position>(e));
        larvae::AssertFalse(world.Has<Velocity>(e));
        larvae::AssertEqual(world.Get<Position>(e)->x, 1.0f);
    });

    auto test18 = larvae::RegisterTest("QueenCommandBuffer", "FlushRemoveNonExistent", []() {
        comb::LinearAllocator alloc{131072};

        queen::World world{};

        queen::Entity e = world.Spawn(Position{1.0f, 2.0f, 3.0f});
        larvae::AssertTrue(world.Has<Position>(e));
        larvae::AssertFalse(world.Has<Velocity>(e));

        queen::CommandBuffer<comb::LinearAllocator> cmd{alloc};
        cmd.Remove<Velocity>(e);
        cmd.Flush(world);

        larvae::AssertTrue(world.IsAlive(e));
        larvae::AssertTrue(world.Has<Position>(e));
        larvae::AssertFalse(world.Has<Velocity>(e));
    });

    // ─────────────────────────────────────────────────────────────────────────
    // Flush Tests - Set Component
    // ─────────────────────────────────────────────────────────────────────────

    auto test19 = larvae::RegisterTest("QueenCommandBuffer", "FlushSetExisting", []() {
        comb::LinearAllocator alloc{131072};

        queen::World world{};

        queen::Entity e = world.Spawn(Position{1.0f, 2.0f, 3.0f});

        queen::CommandBuffer<comb::LinearAllocator> cmd{alloc};
        cmd.Set(e, Position{10.0f, 20.0f, 30.0f});
        cmd.Flush(world);

        larvae::AssertEqual(world.Get<Position>(e)->x, 10.0f);
        larvae::AssertEqual(world.Get<Position>(e)->y, 20.0f);
        larvae::AssertEqual(world.Get<Position>(e)->z, 30.0f);
    });

    auto test20 = larvae::RegisterTest("QueenCommandBuffer", "FlushSetNew", []() {
        comb::LinearAllocator alloc{131072};

        queen::World world{};

        queen::Entity e = world.Spawn(Position{1.0f, 2.0f, 3.0f});
        larvae::AssertFalse(world.Has<Velocity>(e));

        queen::CommandBuffer<comb::LinearAllocator> cmd{alloc};
        cmd.Set(e, Velocity{0.1f, 0.2f, 0.3f});
        cmd.Flush(world);

        larvae::AssertTrue(world.Has<Velocity>(e));
        larvae::AssertEqual(world.Get<Velocity>(e)->dx, 0.1f);
    });

    // ─────────────────────────────────────────────────────────────────────────
    // Integration Tests - Query + CommandBuffer
    // ─────────────────────────────────────────────────────────────────────────

    auto test21 = larvae::RegisterTest("QueenCommandBuffer", "DespawnDuringQuery", []() {
        comb::LinearAllocator alloc{262144};

        queen::World world{};

        world.Spawn(Position{1.0f, 0.0f, 0.0f}, Health{0, 100});
        world.Spawn(Position{2.0f, 0.0f, 0.0f}, Health{50, 100});
        world.Spawn(Position{3.0f, 0.0f, 0.0f}, Health{0, 100});

        larvae::AssertEqual(world.EntityCount(), size_t{3});

        queen::CommandBuffer<comb::LinearAllocator> cmd{alloc};

        world.Query<queen::Read<Health>>().EachWithEntity([&](queen::Entity e, const Health& hp) {
            if (hp.current <= 0)
            {
                cmd.Despawn(e);
            }
        });

        cmd.Flush(world);

        larvae::AssertEqual(world.EntityCount(), size_t{1});
    });

    auto test22 = larvae::RegisterTest("QueenCommandBuffer", "SpawnDuringQuery", []() {
        comb::LinearAllocator alloc{262144};

        queen::World world{};

        world.Spawn(Position{1.0f, 0.0f, 0.0f});
        world.Spawn(Position{2.0f, 0.0f, 0.0f});

        larvae::AssertEqual(world.EntityCount(), size_t{2});

        queen::CommandBuffer<comb::LinearAllocator> cmd{alloc};

        world.Query<queen::Read<Position>>().Each([&](const Position& pos) {
            cmd.Spawn().With(Position{pos.x * 2.0f, pos.y, pos.z});
        });

        cmd.Flush(world);

        larvae::AssertEqual(world.EntityCount(), size_t{4});
    });

    auto test23 = larvae::RegisterTest("QueenCommandBuffer", "AddComponentDuringQuery", []() {
        comb::LinearAllocator alloc{262144};

        queen::World world{};

        queen::Entity e1 = world.Spawn(Position{1.0f, 0.0f, 0.0f});
        queen::Entity e2 = world.Spawn(Position{2.0f, 0.0f, 0.0f});

        queen::CommandBuffer<comb::LinearAllocator> cmd{alloc};

        world.Query<queen::Read<Position>>().EachWithEntity([&](queen::Entity e, const Position& pos) {
            cmd.Add(e, Velocity{pos.x, 0.0f, 0.0f});
        });

        cmd.Flush(world);

        larvae::AssertTrue(world.Has<Velocity>(e1));
        larvae::AssertTrue(world.Has<Velocity>(e2));
        larvae::AssertEqual(world.Get<Velocity>(e1)->dx, 1.0f);
        larvae::AssertEqual(world.Get<Velocity>(e2)->dx, 2.0f);
    });

    // ─────────────────────────────────────────────────────────────────────────
    // Edge Cases
    // ─────────────────────────────────────────────────────────────────────────

    auto test24 = larvae::RegisterTest("QueenCommandBuffer", "FlushEmpty", []() {
        comb::LinearAllocator alloc{131072};

        queen::World world{};
        queen::CommandBuffer<comb::LinearAllocator> cmd{alloc};

        cmd.Flush(world);

        larvae::AssertEqual(world.EntityCount(), size_t{0});
        larvae::AssertTrue(cmd.IsEmpty());
    });

    auto test25 = larvae::RegisterTest("QueenCommandBuffer", "DespawnDeadEntity", []() {
        comb::LinearAllocator alloc{131072};

        queen::World world{};

        queen::Entity e = world.Spawn(Position{1.0f, 2.0f, 3.0f});
        world.Despawn(e);
        larvae::AssertFalse(world.IsAlive(e));

        queen::CommandBuffer<comb::LinearAllocator> cmd{alloc};
        cmd.Despawn(e);
        cmd.Flush(world);
    });

    auto test26 = larvae::RegisterTest("QueenCommandBuffer", "AddToDeadEntity", []() {
        comb::LinearAllocator alloc{131072};

        queen::World world{};

        queen::Entity e = world.Spawn(Position{1.0f, 2.0f, 3.0f});
        world.Despawn(e);

        queen::CommandBuffer<comb::LinearAllocator> cmd{alloc};
        cmd.Add(e, Velocity{0.1f, 0.2f, 0.3f});
        cmd.Flush(world);
    });

    auto test27 = larvae::RegisterTest("QueenCommandBuffer", "ReuseAfterFlush", []() {
        comb::LinearAllocator alloc{131072};

        queen::World world{};
        queen::CommandBuffer<comb::LinearAllocator> cmd{alloc};

        cmd.Spawn().With(Position{1.0f, 0.0f, 0.0f});
        cmd.Flush(world);

        larvae::AssertEqual(world.EntityCount(), size_t{1});
        larvae::AssertTrue(cmd.IsEmpty());

        cmd.Spawn().With(Position{2.0f, 0.0f, 0.0f});
        cmd.Flush(world);

        larvae::AssertEqual(world.EntityCount(), size_t{2});
    });

    auto test28 = larvae::RegisterTest("QueenCommandBuffer", "ManyCommands", []() {
        comb::LinearAllocator alloc{1048576};

        queen::World world{};
        queen::CommandBuffer<comb::LinearAllocator> cmd{alloc};

        for (int i = 0; i < 100; ++i)
        {
            cmd.Spawn().With(Position{static_cast<float>(i), 0.0f, 0.0f});
        }

        cmd.Flush(world);

        larvae::AssertEqual(world.EntityCount(), size_t{100});
    });
}
