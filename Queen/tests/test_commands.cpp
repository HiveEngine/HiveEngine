#include <larvae/larvae.h>
#include <queen/world/world.h>
#include <queen/command/commands.h>
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
    // Commands Basic Tests
    // ─────────────────────────────────────────────────────────────

    auto test1 = larvae::RegisterTest("QueenCommands", "CommandsConstruction", []() {
        comb::LinearAllocator alloc{262144};
        queen::Commands<comb::LinearAllocator> commands{alloc};

        larvae::AssertEqual(commands.BufferCount(), size_t{0});
        larvae::AssertTrue(commands.IsEmpty());
        larvae::AssertEqual(commands.TotalCommandCount(), size_t{0});
    });

    auto test2 = larvae::RegisterTest("QueenCommands", "CommandsGetCreatesBuffer", []() {
        comb::LinearAllocator alloc{262144};
        queen::Commands<comb::LinearAllocator> commands{alloc};

        auto& buffer = commands.Get();

        larvae::AssertEqual(commands.BufferCount(), size_t{1});
        larvae::AssertTrue(buffer.IsEmpty());
    });

    auto test3 = larvae::RegisterTest("QueenCommands", "CommandsGetSameThread", []() {
        comb::LinearAllocator alloc{262144};
        queen::Commands<comb::LinearAllocator> commands{alloc};

        auto& buffer1 = commands.Get();
        auto& buffer2 = commands.Get();

        // Should return same buffer for same thread
        larvae::AssertEqual(&buffer1, &buffer2);
        larvae::AssertEqual(commands.BufferCount(), size_t{1});
    });

    auto test4 = larvae::RegisterTest("QueenCommands", "CommandsDespawnDeferred", []() {
        comb::LinearAllocator alloc{262144};
        queen::World world{};

        queen::Entity e1 = world.Spawn(Position{1.0f, 0.0f, 0.0f});
        queen::Entity e2 = world.Spawn(Position{2.0f, 0.0f, 0.0f});

        auto& commands = world.GetCommands();
        commands.Get().Despawn(e1);

        // Entity should still be alive (deferred)
        larvae::AssertTrue(world.IsAlive(e1));
        larvae::AssertTrue(world.IsAlive(e2));
        larvae::AssertEqual(world.EntityCount(), size_t{2});

        // Flush commands
        commands.FlushAll(world);

        // Now e1 should be dead
        larvae::AssertFalse(world.IsAlive(e1));
        larvae::AssertTrue(world.IsAlive(e2));
        larvae::AssertEqual(world.EntityCount(), size_t{1});
    });

    auto test5 = larvae::RegisterTest("QueenCommands", "CommandsSpawnDeferred", []() {
        comb::LinearAllocator alloc{262144};
        queen::World world{};

        auto& commands = world.GetCommands();
        auto builder = commands.Get().Spawn()
            .With(Position{1.0f, 2.0f, 3.0f})
            .With(Velocity{0.1f, 0.2f, 0.3f});

        larvae::AssertEqual(world.EntityCount(), size_t{0});

        commands.FlushAll(world);

        larvae::AssertEqual(world.EntityCount(), size_t{1});

        // Get the spawned entity
        queen::Entity spawned = commands.Get().GetSpawnedEntity(builder.GetSpawnIndex());
        larvae::AssertTrue(world.IsAlive(spawned));

        Position* pos = world.Get<Position>(spawned);
        larvae::AssertNotNull(pos);
        larvae::AssertEqual(pos->x, 1.0f);
    });

    auto test6 = larvae::RegisterTest("QueenCommands", "CommandsAddComponentDeferred", []() {
        comb::LinearAllocator alloc{262144};
        queen::World world{};

        queen::Entity e = world.Spawn(Position{1.0f, 0.0f, 0.0f});

        larvae::AssertFalse(world.Has<Velocity>(e));

        auto& commands = world.GetCommands();
        commands.Get().Add(e, Velocity{0.5f, 0.6f, 0.7f});

        // Component not added yet
        larvae::AssertFalse(world.Has<Velocity>(e));

        commands.FlushAll(world);

        // Now component should be added
        larvae::AssertTrue(world.Has<Velocity>(e));
        Velocity* vel = world.Get<Velocity>(e);
        larvae::AssertEqual(vel->dx, 0.5f);
    });

    auto test7 = larvae::RegisterTest("QueenCommands", "CommandsRemoveComponentDeferred", []() {
        comb::LinearAllocator alloc{262144};
        queen::World world{};

        queen::Entity e = world.Spawn(Position{1.0f, 0.0f, 0.0f}, Velocity{0.5f, 0.0f, 0.0f});

        larvae::AssertTrue(world.Has<Velocity>(e));

        auto& commands = world.GetCommands();
        commands.Get().Remove<Velocity>(e);

        // Component not removed yet
        larvae::AssertTrue(world.Has<Velocity>(e));

        commands.FlushAll(world);

        // Now component should be removed
        larvae::AssertFalse(world.Has<Velocity>(e));
        larvae::AssertTrue(world.Has<Position>(e));
    });

    auto test8 = larvae::RegisterTest("QueenCommands", "CommandsClearAll", []() {
        comb::LinearAllocator alloc{262144};
        queen::World world{};

        queen::Entity e = world.Spawn(Position{1.0f, 0.0f, 0.0f});

        auto& commands = world.GetCommands();
        commands.Get().Despawn(e);
        commands.Get().Spawn().With(Position{2.0f, 0.0f, 0.0f});

        larvae::AssertFalse(commands.IsEmpty());
        // 1 Despawn + 1 Spawn + 1 AddComponent (for With<Position>) = 3 commands
        larvae::AssertEqual(commands.TotalCommandCount(), size_t{3});

        commands.ClearAll();

        larvae::AssertTrue(commands.IsEmpty());
        larvae::AssertEqual(commands.TotalCommandCount(), size_t{0});

        // Entity should still be alive since we cleared without flushing
        larvae::AssertTrue(world.IsAlive(e));
    });

    // ─────────────────────────────────────────────────────────────
    // Commands with Scheduler Tests
    // ─────────────────────────────────────────────────────────────

    auto test9 = larvae::RegisterTest("QueenCommands", "SchedulerFlushesCommands", []() {
        comb::LinearAllocator alloc{262144};
        queen::World world{};

        queen::Entity e = world.Spawn(Position{1.0f, 0.0f, 0.0f}, Health{0, 100});

        world.System<queen::Read<Health>>("DespawnDead")
            .EachWithEntity([&](queen::Entity entity, const Health& hp) {
                if (hp.current <= 0)
                {
                    world.GetCommands().Get().Despawn(entity);
                }
            });

        larvae::AssertTrue(world.IsAlive(e));

        world.Update();  // Scheduler should flush commands after all systems run

        larvae::AssertFalse(world.IsAlive(e));
    });

    auto test10 = larvae::RegisterTest("QueenCommands", "EachWithCommandsDespawn", []() {
        comb::LinearAllocator alloc{262144};
        queen::World world{};

        world.Spawn(Position{1.0f, 0.0f, 0.0f}, Health{0, 100});
        world.Spawn(Position{2.0f, 0.0f, 0.0f}, Health{50, 100});
        world.Spawn(Position{3.0f, 0.0f, 0.0f}, Health{0, 100});

        larvae::AssertEqual(world.EntityCount(), size_t{3});

        world.System<queen::Read<Health>>("DeathCheck")
            .EachWithCommands([](queen::Entity e, const Health& hp, queen::Commands<queen::PersistentAllocator>& cmd) {
                if (hp.current <= 0)
                {
                    cmd.Get().Despawn(e);
                }
            });

        world.Update();

        // Two entities with 0 health should be despawned
        larvae::AssertEqual(world.EntityCount(), size_t{1});
    });

    auto test11 = larvae::RegisterTest("QueenCommands", "EachWithCommandsSpawn", []() {
        comb::LinearAllocator alloc{262144};
        queen::World world{};

        world.Spawn(Position{1.0f, 0.0f, 0.0f});
        world.Spawn(Position{2.0f, 0.0f, 0.0f});

        larvae::AssertEqual(world.EntityCount(), size_t{2});

        world.System<queen::Read<Position>>("SpawnClone")
            .EachWithCommands([](queen::Entity, const Position& pos, queen::Commands<queen::PersistentAllocator>& cmd) {
                // Spawn a clone at double the position
                cmd.Get().Spawn().With(Position{pos.x * 2.0f, pos.y * 2.0f, pos.z * 2.0f});
            });

        world.Update();

        // Should have original 2 + 2 clones
        larvae::AssertEqual(world.EntityCount(), size_t{4});
    });

    auto test12 = larvae::RegisterTest("QueenCommands", "EachWithCommandsAddComponent", []() {
        comb::LinearAllocator alloc{262144};
        queen::World world{};

        queen::Entity e = world.Spawn(Position{1.0f, 0.0f, 0.0f});

        larvae::AssertFalse(world.Has<Velocity>(e));

        world.System<queen::Read<Position>>("AddVelocity")
            .EachWithCommands([](queen::Entity entity, const Position& pos, queen::Commands<queen::PersistentAllocator>& cmd) {
                cmd.Get().Add(entity, Velocity{pos.x * 0.1f, 0.0f, 0.0f});
            });

        world.Update();

        larvae::AssertTrue(world.Has<Velocity>(e));
        Velocity* vel = world.Get<Velocity>(e);
        larvae::AssertEqual(vel->dx, 0.1f);
    });

    auto test13 = larvae::RegisterTest("QueenCommands", "MultipleUpdatesWithCommands", []() {
        comb::LinearAllocator alloc{262144};
        queen::World world{};

        // Start with 3 entities each with Health=100
        world.Spawn(Position{1.0f, 0.0f, 0.0f}, Health{100, 100});
        world.Spawn(Position{2.0f, 0.0f, 0.0f}, Health{100, 100});
        world.Spawn(Position{3.0f, 0.0f, 0.0f}, Health{100, 100});

        // System that reduces health each frame
        world.System<queen::Write<Health>>("DamageSystem")
            .Each([](Health& hp) {
                hp.current -= 40;
            });

        // System that despawns dead entities
        world.System<queen::Read<Health>>("DeathSystem")
            .EachWithCommands([](queen::Entity e, const Health& hp, queen::Commands<queen::PersistentAllocator>& cmd) {
                if (hp.current <= 0)
                {
                    cmd.Get().Despawn(e);
                }
            });

        // Frame 1: Health goes to 60, no deaths
        world.Update();
        larvae::AssertEqual(world.EntityCount(), size_t{3});

        // Frame 2: Health goes to 20, no deaths
        world.Update();
        larvae::AssertEqual(world.EntityCount(), size_t{3});

        // Frame 3: Health goes to -20, all die
        world.Update();
        larvae::AssertEqual(world.EntityCount(), size_t{0});
    });

    auto test14 = larvae::RegisterTest("QueenCommands", "CommandsForEach", []() {
        comb::LinearAllocator alloc{262144};
        queen::Commands<comb::LinearAllocator> commands{alloc};

        // Create buffers by getting them (simulates multiple threads)
        auto& buffer1 = commands.Get();
        buffer1.Despawn(queen::Entity{0, 0, queen::Entity::Flags::kNone});
        buffer1.Despawn(queen::Entity{1, 0, queen::Entity::Flags::kNone});

        size_t total_commands = 0;
        commands.ForEach([&total_commands](queen::CommandBuffer<comb::LinearAllocator>& buf) {
            total_commands += buf.CommandCount();
        });

        larvae::AssertEqual(total_commands, size_t{2});
    });

    auto test15 = larvae::RegisterTest("QueenCommands", "WorldHasCommands", []() {
        comb::LinearAllocator alloc{262144};
        queen::World world{};

        auto& commands = world.GetCommands();

        // Initially empty
        larvae::AssertTrue(commands.IsEmpty());
        larvae::AssertEqual(commands.BufferCount(), size_t{0});
    });

    auto test16 = larvae::RegisterTest("QueenCommands", "CommandsSetComponentDeferred", []() {
        comb::LinearAllocator alloc{262144};
        queen::World world{};

        queen::Entity e = world.Spawn(Position{1.0f, 0.0f, 0.0f});

        auto& commands = world.GetCommands();
        commands.Get().Set(e, Position{5.0f, 6.0f, 7.0f});

        // Position not changed yet
        Position* pos = world.Get<Position>(e);
        larvae::AssertEqual(pos->x, 1.0f);

        commands.FlushAll(world);

        // Now position should be updated
        pos = world.Get<Position>(e);
        larvae::AssertEqual(pos->x, 5.0f);
        larvae::AssertEqual(pos->y, 6.0f);
        larvae::AssertEqual(pos->z, 7.0f);
    });

    auto test17 = larvae::RegisterTest("QueenCommands", "CommandsMultipleOperations", []() {
        comb::LinearAllocator alloc{262144};
        queen::World world{};

        queen::Entity e = world.Spawn(Position{1.0f, 0.0f, 0.0f});

        auto& commands = world.GetCommands();

        // Queue multiple operations
        commands.Get().Add(e, Velocity{0.1f, 0.0f, 0.0f});
        commands.Get().Add(e, Health{100, 100});
        commands.Get().Set(e, Position{5.0f, 0.0f, 0.0f});

        // Before flush
        larvae::AssertFalse(world.Has<Velocity>(e));
        larvae::AssertFalse(world.Has<Health>(e));
        larvae::AssertEqual(world.Get<Position>(e)->x, 1.0f);

        commands.FlushAll(world);

        // After flush
        larvae::AssertTrue(world.Has<Velocity>(e));
        larvae::AssertTrue(world.Has<Health>(e));
        larvae::AssertEqual(world.Get<Position>(e)->x, 5.0f);
    });
}
