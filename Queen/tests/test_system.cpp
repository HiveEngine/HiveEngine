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
    // SystemId Tests
    // ─────────────────────────────────────────────────────────────

    auto test1 = larvae::RegisterTest("QueenSystem", "SystemIdDefault", []() {
        queen::SystemId id{};

        larvae::AssertFalse(id.IsValid());
        larvae::AssertTrue(id == queen::SystemId::Invalid());
    });

    auto test2 = larvae::RegisterTest("QueenSystem", "SystemIdValid", []() {
        queen::SystemId id{42};

        larvae::AssertTrue(id.IsValid());
        larvae::AssertEqual(id.Index(), uint32_t{42});
    });

    auto test3 = larvae::RegisterTest("QueenSystem", "SystemIdComparison", []() {
        queen::SystemId id1{1};
        queen::SystemId id2{2};
        queen::SystemId id3{1};

        larvae::AssertFalse(id1 == id2);
        larvae::AssertTrue(id1 == id3);
        larvae::AssertTrue(id1 < id2);
    });

    // ─────────────────────────────────────────────────────────────
    // AccessDescriptor Tests
    // ─────────────────────────────────────────────────────────────

    auto test4 = larvae::RegisterTest("QueenSystem", "AccessDescriptorEmpty", []() {
        comb::LinearAllocator alloc{65536};
        queen::AccessDescriptor<comb::LinearAllocator> access{alloc};

        larvae::AssertTrue(access.IsEmpty());
        larvae::AssertTrue(access.IsPure());
        larvae::AssertFalse(access.IsExclusive());
    });

    auto test5 = larvae::RegisterTest("QueenSystem", "AccessDescriptorAddComponentRead", []() {
        comb::LinearAllocator alloc{65536};
        queen::AccessDescriptor<comb::LinearAllocator> access{alloc};

        access.AddComponentRead<Position>();

        larvae::AssertFalse(access.IsEmpty());
        larvae::AssertEqual(access.ComponentReads().Size(), size_t{1});
        larvae::AssertEqual(access.ComponentWrites().Size(), size_t{0});
    });

    auto test6 = larvae::RegisterTest("QueenSystem", "AccessDescriptorAddComponentWrite", []() {
        comb::LinearAllocator alloc{65536};
        queen::AccessDescriptor<comb::LinearAllocator> access{alloc};

        access.AddComponentWrite<Velocity>();

        larvae::AssertEqual(access.ComponentWrites().Size(), size_t{1});
    });

    auto test7 = larvae::RegisterTest("QueenSystem", "AccessDescriptorAddResource", []() {
        comb::LinearAllocator alloc{65536};
        queen::AccessDescriptor<comb::LinearAllocator> access{alloc};

        access.AddResourceRead<Health>();
        access.AddResourceWrite<Position>();

        larvae::AssertEqual(access.ResourceReads().Size(), size_t{1});
        larvae::AssertEqual(access.ResourceWrites().Size(), size_t{1});
    });

    auto test8 = larvae::RegisterTest("QueenSystem", "AccessDescriptorConflictWriteRead", []() {
        comb::LinearAllocator alloc{65536};
        queen::AccessDescriptor<comb::LinearAllocator> access1{alloc};
        queen::AccessDescriptor<comb::LinearAllocator> access2{alloc};

        access1.AddComponentWrite<Position>();
        access2.AddComponentRead<Position>();

        larvae::AssertTrue(access1.ConflictsWith(access2));
        larvae::AssertTrue(access2.ConflictsWith(access1));
    });

    auto test9 = larvae::RegisterTest("QueenSystem", "AccessDescriptorConflictWriteWrite", []() {
        comb::LinearAllocator alloc{65536};
        queen::AccessDescriptor<comb::LinearAllocator> access1{alloc};
        queen::AccessDescriptor<comb::LinearAllocator> access2{alloc};

        access1.AddComponentWrite<Position>();
        access2.AddComponentWrite<Position>();

        larvae::AssertTrue(access1.ConflictsWith(access2));
    });

    auto test10 = larvae::RegisterTest("QueenSystem", "AccessDescriptorNoConflictReadRead", []() {
        comb::LinearAllocator alloc{65536};
        queen::AccessDescriptor<comb::LinearAllocator> access1{alloc};
        queen::AccessDescriptor<comb::LinearAllocator> access2{alloc};

        access1.AddComponentRead<Position>();
        access2.AddComponentRead<Position>();

        larvae::AssertFalse(access1.ConflictsWith(access2));
    });

    auto test11 = larvae::RegisterTest("QueenSystem", "AccessDescriptorNoConflictDifferentComponents", []() {
        comb::LinearAllocator alloc{65536};
        queen::AccessDescriptor<comb::LinearAllocator> access1{alloc};
        queen::AccessDescriptor<comb::LinearAllocator> access2{alloc};

        access1.AddComponentWrite<Position>();
        access2.AddComponentWrite<Velocity>();

        larvae::AssertFalse(access1.ConflictsWith(access2));
    });

    auto test12 = larvae::RegisterTest("QueenSystem", "AccessDescriptorExclusiveConflict", []() {
        comb::LinearAllocator alloc{65536};
        queen::AccessDescriptor<comb::LinearAllocator> access1{alloc};
        queen::AccessDescriptor<comb::LinearAllocator> access2{alloc};

        access1.SetWorldAccess(queen::WorldAccess::Exclusive);

        larvae::AssertTrue(access1.ConflictsWith(access2));
        larvae::AssertTrue(access2.ConflictsWith(access1));
    });

    // ─────────────────────────────────────────────────────────────
    // System Registration Tests
    // ─────────────────────────────────────────────────────────────

    auto test13 = larvae::RegisterTest("QueenSystem", "RegisterSystem", []() {
        comb::LinearAllocator alloc{262144};
        queen::World world{};

        int call_count = 0;

        queen::SystemId id = world.System<queen::Read<Position>>("TestSystem")
            .Each([&](const Position&) {
                ++call_count;
            });

        larvae::AssertTrue(id.IsValid());
        larvae::AssertEqual(world.SystemCount(), size_t{1});
    });

    auto test14 = larvae::RegisterTest("QueenSystem", "RunSystem", []() {
        comb::LinearAllocator alloc{262144};
        queen::World world{};

        world.Spawn(Position{1.0f, 2.0f, 3.0f});
        world.Spawn(Position{4.0f, 5.0f, 6.0f});

        int call_count = 0;

        queen::SystemId id = world.System<queen::Read<Position>>("CountSystem")
            .Each([&](const Position&) {
                ++call_count;
            });

        world.RunSystem(id);

        larvae::AssertEqual(call_count, 2);
    });

    auto test15 = larvae::RegisterTest("QueenSystem", "SystemModifiesComponent", []() {
        comb::LinearAllocator alloc{262144};
        queen::World world{};

        queen::Entity e = world.Spawn(Position{0.0f, 0.0f, 0.0f}, Velocity{1.0f, 2.0f, 3.0f});

        queen::SystemId id = world.System<queen::Read<Velocity>, queen::Write<Position>>("Movement")
            .Each([](const Velocity& vel, Position& pos) {
                pos.x += vel.dx;
                pos.y += vel.dy;
                pos.z += vel.dz;
            });

        world.RunSystem(id);

        Position* pos = world.Get<Position>(e);
        larvae::AssertEqual(pos->x, 1.0f);
        larvae::AssertEqual(pos->y, 2.0f);
        larvae::AssertEqual(pos->z, 3.0f);
    });

    auto test16 = larvae::RegisterTest("QueenSystem", "RunMultipleSystems", []() {
        comb::LinearAllocator alloc{262144};
        queen::World world{};

        queen::Entity e = world.Spawn(Position{0.0f, 0.0f, 0.0f}, Velocity{1.0f, 0.0f, 0.0f});

        queen::SystemId sys1 = world.System<queen::Read<Velocity>, queen::Write<Position>>("ApplyVelocity")
            .Each([](const Velocity& vel, Position& pos) {
                pos.x += vel.dx;
            });

        queen::SystemId sys2 = world.System<queen::Write<Position>>("DoublePosition")
            .Each([](Position& pos) {
                pos.x *= 2.0f;
            });

        world.RunSystem(sys1);
        world.RunSystem(sys2);

        Position* pos = world.Get<Position>(e);
        larvae::AssertEqual(pos->x, 2.0f);
    });

    auto test17 = larvae::RegisterTest("QueenSystem", "RunAllSystems", []() {
        comb::LinearAllocator alloc{262144};
        queen::World world{};

        queen::Entity e = world.Spawn(Position{1.0f, 0.0f, 0.0f});

        int order = 0;
        int sys1_order = -1;
        int sys2_order = -1;

        world.System<queen::Write<Position>>("First")
            .Each([&](Position& pos) {
                pos.x += 1.0f;
                sys1_order = order++;
            });

        world.System<queen::Write<Position>>("Second")
            .Each([&](Position& pos) {
                pos.x *= 2.0f;
                sys2_order = order++;
            });

        world.RunAllSystems();

        larvae::AssertEqual(sys1_order, 0);
        larvae::AssertEqual(sys2_order, 1);

        Position* pos = world.Get<Position>(e);
        larvae::AssertEqual(pos->x, 4.0f);
    });

    auto test18 = larvae::RegisterTest("QueenSystem", "DisableSystem", []() {
        comb::LinearAllocator alloc{262144};
        queen::World world{};

        world.Spawn(Position{1.0f, 0.0f, 0.0f});

        int call_count = 0;

        queen::SystemId id = world.System<queen::Read<Position>>("Disabled")
            .Each([&](const Position&) {
                ++call_count;
            });

        world.SetSystemEnabled(id, false);
        world.RunSystem(id);

        larvae::AssertEqual(call_count, 0);
    });

    auto test19 = larvae::RegisterTest("QueenSystem", "ReenableSystem", []() {
        comb::LinearAllocator alloc{262144};
        queen::World world{};

        world.Spawn(Position{1.0f, 0.0f, 0.0f});

        int call_count = 0;

        queen::SystemId id = world.System<queen::Read<Position>>("Toggle")
            .Each([&](const Position&) {
                ++call_count;
            });

        world.SetSystemEnabled(id, false);
        world.RunSystem(id);
        larvae::AssertEqual(call_count, 0);

        world.SetSystemEnabled(id, true);
        world.RunSystem(id);
        larvae::AssertEqual(call_count, 1);
    });

    auto test20 = larvae::RegisterTest("QueenSystem", "SystemWithEntity", []() {
        comb::LinearAllocator alloc{262144};
        queen::World world{};

        queen::Entity e1 = world.Spawn(Position{1.0f, 0.0f, 0.0f});
        queen::Entity e2 = world.Spawn(Position{2.0f, 0.0f, 0.0f});

        queen::Entity found1 = queen::Entity::Invalid();
        queen::Entity found2 = queen::Entity::Invalid();

        queen::SystemId id = world.System<queen::Read<Position>>("FindEntity")
            .EachWithEntity([&](queen::Entity e, const Position& pos) {
                if (pos.x == 1.0f) found1 = e;
                if (pos.x == 2.0f) found2 = e;
            });

        world.RunSystem(id);

        larvae::AssertTrue(found1 == e1);
        larvae::AssertTrue(found2 == e2);
    });

    auto test21 = larvae::RegisterTest("QueenSystem", "SystemAccessExtraction", []() {
        comb::LinearAllocator alloc{262144};
        queen::World world{};

        world.System<queen::Read<Position>, queen::Write<Velocity>>("AccessTest")
            .Each([](const Position&, Velocity&) {});

        auto* storage = &world.GetSystemStorage();
        auto* system = storage->GetSystemByName("AccessTest");

        larvae::AssertNotNull(system);
        larvae::AssertEqual(system->Access().ComponentReads().Size(), size_t{1});
        larvae::AssertEqual(system->Access().ComponentWrites().Size(), size_t{1});
    });

    auto test22 = larvae::RegisterTest("QueenSystem", "SystemResourceAccess", []() {
        comb::LinearAllocator alloc{262144};
        queen::World world{};

        world.System<queen::Read<Position>>("ResourceTest")
            .WithResource<Health>()
            .WithResourceMut<Velocity>()
            .Each([](const Position&) {});

        auto* system = world.GetSystemStorage().GetSystemByName("ResourceTest");

        larvae::AssertNotNull(system);
        larvae::AssertEqual(system->Access().ResourceReads().Size(), size_t{1});
        larvae::AssertEqual(system->Access().ResourceWrites().Size(), size_t{1});
    });

    auto test23 = larvae::RegisterTest("QueenSystem", "SystemName", []() {
        comb::LinearAllocator alloc{262144};
        queen::World world{};

        world.System<queen::Read<Position>>("MySystemName")
            .Each([](const Position&) {});

        auto* system = world.GetSystemStorage().GetSystemByName("MySystemName");

        larvae::AssertNotNull(system);
        larvae::AssertTrue(std::strcmp(system->Name(), "MySystemName") == 0);
    });

    auto test24 = larvae::RegisterTest("QueenSystem", "NoMatchingEntities", []() {
        comb::LinearAllocator alloc{262144};
        queen::World world{};

        world.Spawn(Velocity{1.0f, 0.0f, 0.0f});

        int call_count = 0;

        queen::SystemId id = world.System<queen::Read<Position>>("NoMatch")
            .Each([&](const Position&) {
                ++call_count;
            });

        world.RunSystem(id);

        larvae::AssertEqual(call_count, 0);
    });

    auto test25 = larvae::RegisterTest("QueenSystem", "MultipleArchetypes", []() {
        comb::LinearAllocator alloc{262144};
        queen::World world{};

        world.Spawn(Position{1.0f, 0.0f, 0.0f});
        world.Spawn(Position{2.0f, 0.0f, 0.0f}, Velocity{0.0f, 0.0f, 0.0f});
        world.Spawn(Position{3.0f, 0.0f, 0.0f}, Health{100, 100});

        float sum = 0.0f;

        queen::SystemId id = world.System<queen::Read<Position>>("SumPosition")
            .Each([&](const Position& pos) {
                sum += pos.x;
            });

        world.RunSystem(id);

        larvae::AssertEqual(sum, 6.0f);
    });
}
