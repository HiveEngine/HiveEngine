#include <larvae/larvae.h>
#include <queen/world/world.h>
#include <queen/system/resource_param.h>
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

    struct Time
    {
        float elapsed;
        float delta;
    };

    struct GameConfig
    {
        int max_entities;
        float gravity;
    };

    // ─────────────────────────────────────────────────────────────
    // Res<T> Basic Tests
    // ─────────────────────────────────────────────────────────────

    auto test1 = larvae::RegisterTest("QueenResourceParam", "ResConstruction", []() {
        Time time{1.0f, 0.016f};
        queen::Res<Time> res{&time};

        larvae::AssertTrue(res.IsValid());
        larvae::AssertEqual(res.Get(), &time);
    });

    auto test2 = larvae::RegisterTest("QueenResourceParam", "ResDefaultConstruction", []() {
        queen::Res<Time> res{};

        larvae::AssertFalse(res.IsValid());
        larvae::AssertNull(res.Get());
    });

    auto test3 = larvae::RegisterTest("QueenResourceParam", "ResArrowOperator", []() {
        Time time{1.0f, 0.016f};
        queen::Res<Time> res{&time};

        larvae::AssertEqual(res->elapsed, 1.0f);
        larvae::AssertEqual(res->delta, 0.016f);
    });

    auto test4 = larvae::RegisterTest("QueenResourceParam", "ResDereferenceOperator", []() {
        Time time{1.0f, 0.016f};
        queen::Res<Time> res{&time};

        const Time& ref = *res;
        larvae::AssertEqual(ref.elapsed, 1.0f);
        larvae::AssertEqual(ref.delta, 0.016f);
    });

    auto test5 = larvae::RegisterTest("QueenResourceParam", "ResIsImmutable", []() {
        larvae::AssertFalse(queen::Res<Time>::is_mutable);
    });

    auto test6 = larvae::RegisterTest("QueenResourceParam", "ResTypeId", []() {
        larvae::AssertEqual(queen::Res<Time>::type_id, queen::TypeIdOf<Time>());
    });

    auto test7 = larvae::RegisterTest("QueenResourceParam", "ResBoolConversion", []() {
        Time time{1.0f, 0.016f};
        queen::Res<Time> valid_res{&time};
        queen::Res<Time> invalid_res{};

        larvae::AssertTrue(static_cast<bool>(valid_res));
        larvae::AssertFalse(static_cast<bool>(invalid_res));
    });

    // ─────────────────────────────────────────────────────────────
    // ResMut<T> Basic Tests
    // ─────────────────────────────────────────────────────────────

    auto test8 = larvae::RegisterTest("QueenResourceParam", "ResMutConstruction", []() {
        Time time{1.0f, 0.016f};
        queen::ResMut<Time> res{&time};

        larvae::AssertTrue(res.IsValid());
        larvae::AssertEqual(res.Get(), &time);
    });

    auto test9 = larvae::RegisterTest("QueenResourceParam", "ResMutDefaultConstruction", []() {
        queen::ResMut<Time> res{};

        larvae::AssertFalse(res.IsValid());
        larvae::AssertNull(res.Get());
    });

    auto test10 = larvae::RegisterTest("QueenResourceParam", "ResMutArrowOperator", []() {
        Time time{1.0f, 0.016f};
        queen::ResMut<Time> res{&time};

        res->elapsed = 2.0f;
        res->delta = 0.032f;

        larvae::AssertEqual(time.elapsed, 2.0f);
        larvae::AssertEqual(time.delta, 0.032f);
    });

    auto test11 = larvae::RegisterTest("QueenResourceParam", "ResMutDereferenceOperator", []() {
        Time time{1.0f, 0.016f};
        queen::ResMut<Time> res{&time};

        Time& ref = *res;
        ref.elapsed = 3.0f;

        larvae::AssertEqual(time.elapsed, 3.0f);
    });

    auto test12 = larvae::RegisterTest("QueenResourceParam", "ResMutIsMutable", []() {
        larvae::AssertTrue(queen::ResMut<Time>::is_mutable);
    });

    auto test13 = larvae::RegisterTest("QueenResourceParam", "ResMutTypeId", []() {
        larvae::AssertEqual(queen::ResMut<Time>::type_id, queen::TypeIdOf<Time>());
    });

    // ─────────────────────────────────────────────────────────────
    // Type Traits Tests
    // ─────────────────────────────────────────────────────────────

    auto test14 = larvae::RegisterTest("QueenResourceParam", "IsResV", []() {
        larvae::AssertTrue(queen::IsResV<queen::Res<Time>>);
        larvae::AssertFalse(queen::IsResV<queen::ResMut<Time>>);
        larvae::AssertFalse(queen::IsResV<Time>);
        larvae::AssertFalse(queen::IsResV<int>);
    });

    auto test15 = larvae::RegisterTest("QueenResourceParam", "IsResMutV", []() {
        larvae::AssertTrue(queen::IsResMutV<queen::ResMut<Time>>);
        larvae::AssertFalse(queen::IsResMutV<queen::Res<Time>>);
        larvae::AssertFalse(queen::IsResMutV<Time>);
        larvae::AssertFalse(queen::IsResMutV<int>);
    });

    auto test16 = larvae::RegisterTest("QueenResourceParam", "IsResourceParam", []() {
        larvae::AssertTrue(queen::IsResourceParam<queen::Res<Time>>);
        larvae::AssertTrue(queen::IsResourceParam<queen::ResMut<Time>>);
        larvae::AssertFalse(queen::IsResourceParam<Time>);
        larvae::AssertFalse(queen::IsResourceParam<int>);
    });

    // ─────────────────────────────────────────────────────────────
    // RunWithRes / RunWithResMut Tests
    // ─────────────────────────────────────────────────────────────

    auto test17 = larvae::RegisterTest("QueenResourceParam", "RunWithResReadsResource", []() {
        comb::LinearAllocator alloc{262144};
        queen::World world{};

        world.InsertResource(Time{0.0f, 0.016f});

        float captured_elapsed = 0.0f;

        world.System("ReadTime")
            .RunWithRes<Time>([&captured_elapsed](queen::Res<Time> time) {
                captured_elapsed = time->elapsed;
            });

        world.Update();

        larvae::AssertEqual(captured_elapsed, 0.0f);
    });

    auto test18 = larvae::RegisterTest("QueenResourceParam", "RunWithResMutModifiesResource", []() {
        comb::LinearAllocator alloc{262144};
        queen::World world{};

        world.InsertResource(Time{0.0f, 0.016f});

        world.System("UpdateTime")
            .RunWithResMut<Time>([](queen::ResMut<Time> time) {
                time->elapsed += time->delta;
            });

        world.Update();

        Time* time = world.Resource<Time>();
        larvae::AssertEqual(time->elapsed, 0.016f);

        world.Update();
        larvae::AssertEqual(time->elapsed, 0.032f);
    });

    auto test19 = larvae::RegisterTest("QueenResourceParam", "RunWithResMultipleUpdates", []() {
        comb::LinearAllocator alloc{262144};
        queen::World world{};

        world.InsertResource(Time{0.0f, 0.016f});

        int call_count = 0;

        world.System("Counter")
            .RunWithRes<Time>([&call_count](queen::Res<Time>) {
                call_count++;
            });

        world.Update();
        world.Update();
        world.Update();

        larvae::AssertEqual(call_count, 3);
    });

    // ─────────────────────────────────────────────────────────────
    // EachWithRes / EachWithResMut Tests
    // ─────────────────────────────────────────────────────────────

    auto test20 = larvae::RegisterTest("QueenResourceParam", "EachWithResIteratesEntities", []() {
        comb::LinearAllocator alloc{262144};
        queen::World world{};

        world.InsertResource(Time{0.0f, 0.016f});

        (void)world.Spawn(Position{1.0f, 0.0f, 0.0f});
        (void)world.Spawn(Position{2.0f, 0.0f, 0.0f});
        (void)world.Spawn(Position{3.0f, 0.0f, 0.0f});

        float sum = 0.0f;

        world.System<queen::Read<Position>>("SumPositions")
            .EachWithRes<Time>([&sum](queen::Entity, const Position& pos, queen::Res<Time> time) {
                sum += pos.x * time->delta;
            });

        world.Update();

        // sum = (1 + 2 + 3) * 0.016 = 0.096
        larvae::AssertEqual(sum, 0.096f);
    });

    auto test21 = larvae::RegisterTest("QueenResourceParam", "EachWithResMutModifies", []() {
        comb::LinearAllocator alloc{262144};
        queen::World world{};

        world.InsertResource(GameConfig{100, 9.8f});

        (void)world.Spawn(Position{0.0f, 0.0f, 0.0f}, Velocity{0.0f, 10.0f, 0.0f});

        world.System<queen::Read<Position>, queen::Write<Velocity>>("ApplyGravity")
            .EachWithResMut<GameConfig>([](queen::Entity, const Position&, Velocity& vel, queen::ResMut<GameConfig> config) {
                vel.dy -= config->gravity;
                // Also modify config to prove we have mutable access
                config->max_entities = 200;
            });

        world.Update();

        GameConfig* config = world.Resource<GameConfig>();
        larvae::AssertEqual(config->max_entities, 200);
    });

    auto test22 = larvae::RegisterTest("QueenResourceParam", "EachWithResNoEntities", []() {
        comb::LinearAllocator alloc{262144};
        queen::World world{};

        world.InsertResource(Time{0.0f, 0.016f});

        int call_count = 0;

        world.System<queen::Read<Position>>("NoEntities")
            .EachWithRes<Time>([&call_count](queen::Entity, const Position&, queen::Res<Time>) {
                call_count++;
            });

        world.Update();

        // No entities, so callback should not be called
        larvae::AssertEqual(call_count, 0);
    });

    // ─────────────────────────────────────────────────────────────
    // Resource Access Descriptor Tests
    // ─────────────────────────────────────────────────────────────

    auto test23 = larvae::RegisterTest("QueenResourceParam", "RunWithResRegistersRead", []() {
        comb::LinearAllocator alloc{262144};
        queen::World world{};

        world.InsertResource(Time{0.0f, 0.016f});

        queen::SystemId id = world.System("ReadTime")
            .RunWithRes<Time>([](queen::Res<Time>) {});

        auto& storage = world.GetSystemStorage();
        auto* desc = storage.GetSystem(id);

        larvae::AssertNotNull(desc);

        // Verify resource read is registered
        const auto& reads = desc->Access().ResourceReads();
        bool found = false;
        for (size_t i = 0; i < reads.Size(); ++i)
        {
            if (reads[i] == queen::TypeIdOf<Time>())
            {
                found = true;
                break;
            }
        }
        larvae::AssertTrue(found);
    });

    auto test24 = larvae::RegisterTest("QueenResourceParam", "RunWithResMutRegistersWrite", []() {
        comb::LinearAllocator alloc{262144};
        queen::World world{};

        world.InsertResource(Time{0.0f, 0.016f});

        queen::SystemId id = world.System("WriteTime")
            .RunWithResMut<Time>([](queen::ResMut<Time>) {});

        auto& storage = world.GetSystemStorage();
        auto* desc = storage.GetSystem(id);

        larvae::AssertNotNull(desc);

        // Verify resource write is registered
        const auto& writes = desc->Access().ResourceWrites();
        bool found = false;
        for (size_t i = 0; i < writes.Size(); ++i)
        {
            if (writes[i] == queen::TypeIdOf<Time>())
            {
                found = true;
                break;
            }
        }
        larvae::AssertTrue(found);
    });

    auto test25 = larvae::RegisterTest("QueenResourceParam", "EachWithResRegistersRead", []() {
        comb::LinearAllocator alloc{262144};
        queen::World world{};

        world.InsertResource(Time{0.0f, 0.016f});

        queen::SystemId id = world.System<queen::Read<Position>>("ReadTimeWithEntities")
            .EachWithRes<Time>([](queen::Entity, const Position&, queen::Res<Time>) {});

        auto& storage = world.GetSystemStorage();
        auto* desc = storage.GetSystem(id);

        larvae::AssertNotNull(desc);

        // Verify resource read is registered
        const auto& resource_reads = desc->Access().ResourceReads();
        bool resource_found = false;
        for (size_t i = 0; i < resource_reads.Size(); ++i)
        {
            if (resource_reads[i] == queen::TypeIdOf<Time>())
            {
                resource_found = true;
                break;
            }
        }
        larvae::AssertTrue(resource_found);

        // Verify component read is registered
        const auto& component_reads = desc->Access().ComponentReads();
        bool component_found = false;
        for (size_t i = 0; i < component_reads.Size(); ++i)
        {
            if (component_reads[i] == queen::TypeIdOf<Position>())
            {
                component_found = true;
                break;
            }
        }
        larvae::AssertTrue(component_found);
    });

    // ─────────────────────────────────────────────────────────────
    // Integration with Multiple Systems
    // ─────────────────────────────────────────────────────────────

    auto test26 = larvae::RegisterTest("QueenResourceParam", "MultipleSystemsWithSameResource", []() {
        comb::LinearAllocator alloc{262144};
        queen::World world{};

        world.InsertResource(Time{0.0f, 0.016f});

        int order_tracker = 0;
        int sys1_order = 0;
        int sys2_order = 0;

        world.System("System1")
            .RunWithRes<Time>([&](queen::Res<Time>) {
                sys1_order = ++order_tracker;
            });

        world.System("System2")
            .RunWithRes<Time>([&](queen::Res<Time>) {
                sys2_order = ++order_tracker;
            });

        world.Update();

        // Both systems ran
        larvae::AssertEqual(sys1_order, 1);
        larvae::AssertEqual(sys2_order, 2);
    });

    auto test27 = larvae::RegisterTest("QueenResourceParam", "MixedResourceAndComponentSystems", []() {
        comb::LinearAllocator alloc{262144};
        queen::World world{};

        world.InsertResource(Time{0.0f, 1.0f}); // delta = 1.0 for easy math

        (void)world.Spawn(Position{0.0f, 0.0f, 0.0f}, Velocity{10.0f, 0.0f, 0.0f});

        // System 1: Update position based on velocity and time
        world.System<queen::Read<Velocity>, queen::Write<Position>>("Movement")
            .EachWithRes<Time>([](queen::Entity, const Velocity& vel, Position& pos, queen::Res<Time> time) {
                pos.x += vel.dx * time->delta;
            });

        // System 2: Update elapsed time
        world.System("UpdateTime")
            .RunWithResMut<Time>([](queen::ResMut<Time> time) {
                time->elapsed += time->delta;
            });

        world.Update();

        // After one frame with delta=1.0, position.x should be 10.0
        auto query = world.Query<queen::Read<Position>>();
        query.Each([](const Position& pos) {
            larvae::AssertEqual(pos.x, 10.0f);
        });

        Time* time = world.Resource<Time>();
        larvae::AssertEqual(time->elapsed, 1.0f);
    });
}
