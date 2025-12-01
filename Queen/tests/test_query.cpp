#include <larvae/larvae.h>
#include <queen/query/query.h>
#include <queen/world/world.h>
#include <comb/linear_allocator.h>

namespace
{
    struct Position { float x, y, z; };
    struct Velocity { float dx, dy, dz; };
    struct Health { int current, max; };
    struct Player {};
    struct Enemy {};
    struct Dead {};

    // ─────────────────────────────────────────────────────────────
    // Basic Query tests
    // ─────────────────────────────────────────────────────────────

    auto test1 = larvae::RegisterTest("QueenQuery", "EmptyQuery", []() {
        comb::LinearAllocator alloc{262144};

        queen::World<comb::LinearAllocator> world{alloc};

        queen::Query<comb::LinearAllocator, queen::Read<Position>> query{alloc, world.GetComponentIndex()};

        larvae::AssertEqual(query.ArchetypeCount(), size_t{0});
        larvae::AssertEqual(query.EntityCount(), size_t{0});
        larvae::AssertTrue(query.IsEmpty());
    });

    auto test2 = larvae::RegisterTest("QueenQuery", "SingleComponentQuery", []() {
        comb::LinearAllocator alloc{524288};

        queen::World<comb::LinearAllocator> world{alloc};

        auto e1 = world.Spawn(Position{1.0f, 2.0f, 3.0f});
        auto e2 = world.Spawn(Position{4.0f, 5.0f, 6.0f});

        queen::Query<comb::LinearAllocator, queen::Read<Position>> query{alloc, world.GetComponentIndex()};

        larvae::AssertEqual(query.ArchetypeCount(), size_t{1});
        larvae::AssertEqual(query.EntityCount(), size_t{2});
        larvae::AssertFalse(query.IsEmpty());

        world.Despawn(e1);
        world.Despawn(e2);
    });

    auto test3 = larvae::RegisterTest("QueenQuery", "MultiComponentQuery", []() {
        comb::LinearAllocator alloc{524288};

        queen::World<comb::LinearAllocator> world{alloc};

        auto e1 = world.Spawn(Position{0, 0, 0}, Velocity{1, 0, 0});
        auto e2 = world.Spawn(Position{0, 0, 0});
        auto e3 = world.Spawn(Position{0, 0, 0}, Velocity{2, 0, 0});

        queen::Query<comb::LinearAllocator, queen::Read<Position>, queen::Read<Velocity>> query{alloc, world.GetComponentIndex()};

        larvae::AssertEqual(query.EntityCount(), size_t{2});

        world.Despawn(e1);
        world.Despawn(e2);
        world.Despawn(e3);
    });

    // ─────────────────────────────────────────────────────────────
    // Each iteration tests
    // ─────────────────────────────────────────────────────────────

    auto test4 = larvae::RegisterTest("QueenQuery", "EachReadOnly", []() {
        comb::LinearAllocator alloc{524288};

        queen::World<comb::LinearAllocator> world{alloc};

        (void)world.Spawn(Position{1.0f, 0, 0});
        (void)world.Spawn(Position{2.0f, 0, 0});
        (void)world.Spawn(Position{3.0f, 0, 0});

        queen::Query<comb::LinearAllocator, queen::Read<Position>> query{alloc, world.GetComponentIndex()};

        float sum = 0.0f;
        query.Each([&sum](const Position& pos) {
            sum += pos.x;
        });

        larvae::AssertEqual(sum, 6.0f);
    });

    auto test5 = larvae::RegisterTest("QueenQuery", "EachWrite", []() {
        comb::LinearAllocator alloc{524288};

        queen::World<comb::LinearAllocator> world{alloc};

        auto e1 = world.Spawn(Position{0, 0, 0}, Velocity{1, 0, 0});
        auto e2 = world.Spawn(Position{0, 0, 0}, Velocity{2, 0, 0});

        queen::Query<comb::LinearAllocator, queen::Read<Velocity>, queen::Write<Position>> query{alloc, world.GetComponentIndex()};

        query.Each([](const Velocity& vel, Position& pos) {
            pos.x += vel.dx;
        });

        larvae::AssertEqual(world.Get<Position>(e1)->x, 1.0f);
        larvae::AssertEqual(world.Get<Position>(e2)->x, 2.0f);

        world.Despawn(e1);
        world.Despawn(e2);
    });

    auto test6 = larvae::RegisterTest("QueenQuery", "EachWithEntity", []() {
        comb::LinearAllocator alloc{524288};

        queen::World<comb::LinearAllocator> world{alloc};

        auto e1 = world.Spawn(Position{1, 0, 0});
        auto e2 = world.Spawn(Position{2, 0, 0});

        queen::Query<comb::LinearAllocator, queen::Read<Position>> query{alloc, world.GetComponentIndex()};

        int count = 0;
        query.EachWithEntity([&count, e1, e2](queen::Entity entity, const Position& pos) {
            if (entity == e1)
            {
                larvae::AssertEqual(pos.x, 1.0f);
            }
            else if (entity == e2)
            {
                larvae::AssertEqual(pos.x, 2.0f);
            }
            ++count;
        });

        larvae::AssertEqual(count, 2);

        world.Despawn(e1);
        world.Despawn(e2);
    });

    // ─────────────────────────────────────────────────────────────
    // Filter tests
    // ─────────────────────────────────────────────────────────────

    auto test7 = larvae::RegisterTest("QueenQuery", "WithFilter", []() {
        comb::LinearAllocator alloc{524288};

        queen::World<comb::LinearAllocator> world{alloc};

        auto e1 = world.Spawn(Position{1, 0, 0}, Player{});
        auto e2 = world.Spawn(Position{2, 0, 0});
        auto e3 = world.Spawn(Position{3, 0, 0}, Player{});

        queen::Query<comb::LinearAllocator, queen::Read<Position>, queen::With<Player>> query{alloc, world.GetComponentIndex()};

        float sum = 0.0f;
        query.Each([&sum](const Position& pos) {
            sum += pos.x;
        });

        larvae::AssertEqual(sum, 4.0f);

        world.Despawn(e1);
        world.Despawn(e2);
        world.Despawn(e3);
    });

    auto test8 = larvae::RegisterTest("QueenQuery", "WithoutFilter", []() {
        comb::LinearAllocator alloc{524288};

        queen::World<comb::LinearAllocator> world{alloc};

        auto e1 = world.Spawn(Position{1, 0, 0});
        auto e2 = world.Spawn(Position{2, 0, 0}, Dead{});
        auto e3 = world.Spawn(Position{3, 0, 0});

        queen::Query<comb::LinearAllocator, queen::Read<Position>, queen::Without<Dead>> query{alloc, world.GetComponentIndex()};

        float sum = 0.0f;
        query.Each([&sum](const Position& pos) {
            sum += pos.x;
        });

        larvae::AssertEqual(sum, 4.0f);

        world.Despawn(e1);
        world.Despawn(e2);
        world.Despawn(e3);
    });

    // ─────────────────────────────────────────────────────────────
    // Optional tests
    // ─────────────────────────────────────────────────────────────

    auto test9 = larvae::RegisterTest("QueenQuery", "MaybeOptional", []() {
        comb::LinearAllocator alloc{524288};

        queen::World<comb::LinearAllocator> world{alloc};

        auto e1 = world.Spawn(Position{1, 0, 0}, Health{100, 100});
        auto e2 = world.Spawn(Position{2, 0, 0});

        queen::Query<comb::LinearAllocator, queen::Read<Position>, queen::Maybe<Health>> query{alloc, world.GetComponentIndex()};

        int with_health = 0;
        int without_health = 0;

        query.Each([&with_health, &without_health]([[maybe_unused]] const Position& pos, const Health* health) {
            if (health != nullptr)
            {
                ++with_health;
                larvae::AssertEqual(health->current, 100);
            }
            else
            {
                ++without_health;
            }
        });

        larvae::AssertEqual(with_health, 1);
        larvae::AssertEqual(without_health, 1);

        world.Despawn(e1);
        world.Despawn(e2);
    });

    // ─────────────────────────────────────────────────────────────
    // Multiple archetypes tests
    // ─────────────────────────────────────────────────────────────

    auto test10 = larvae::RegisterTest("QueenQuery", "MultipleArchetypes", []() {
        comb::LinearAllocator alloc{524288};

        queen::World<comb::LinearAllocator> world{alloc};

        (void)world.Spawn(Position{1, 0, 0}, Velocity{0, 0, 0});
        (void)world.Spawn(Position{2, 0, 0}, Velocity{0, 0, 0}, Health{100, 100});
        (void)world.Spawn(Position{3, 0, 0}, Velocity{0, 0, 0}, Player{});

        queen::Query<comb::LinearAllocator, queen::Read<Position>, queen::Read<Velocity>> query{alloc, world.GetComponentIndex()};

        larvae::AssertEqual(query.ArchetypeCount(), size_t{3});
        larvae::AssertEqual(query.EntityCount(), size_t{3});

        float sum = 0.0f;
        query.Each([&sum](const Position& pos, [[maybe_unused]] const Velocity& vel) {
            sum += pos.x;
        });

        larvae::AssertEqual(sum, 6.0f);
    });

    // ─────────────────────────────────────────────────────────────
    // Complex query tests
    // ─────────────────────────────────────────────────────────────

    auto test11 = larvae::RegisterTest("QueenQuery", "ComplexQuery", []() {
        comb::LinearAllocator alloc{524288};

        queen::World<comb::LinearAllocator> world{alloc};

        (void)world.Spawn(Position{1, 0, 0}, Velocity{0.1f, 0, 0}, Player{});
        (void)world.Spawn(Position{2, 0, 0}, Velocity{0.2f, 0, 0}, Enemy{});
        (void)world.Spawn(Position{3, 0, 0}, Velocity{0.3f, 0, 0}, Player{}, Dead{});
        (void)world.Spawn(Position{4, 0, 0}, Velocity{0.4f, 0, 0});

        queen::Query<comb::LinearAllocator,
            queen::Read<Position>,
            queen::Write<Velocity>,
            queen::With<Player>,
            queen::Without<Dead>
        > query{alloc, world.GetComponentIndex()};

        larvae::AssertEqual(query.EntityCount(), size_t{1});

        query.Each([](const Position& pos, Velocity& vel) {
            larvae::AssertEqual(pos.x, 1.0f);
            larvae::AssertEqual(vel.dx, 0.1f);
            vel.dx *= 2.0f;
        });
    });

    auto test12 = larvae::RegisterTest("QueenQuery", "SystemSimulation", []() {
        comb::LinearAllocator alloc{524288};

        queen::World<comb::LinearAllocator> world{alloc};

        world.InsertResource(queen::Entity{});

        auto e1 = world.Spawn(Position{0, 0, 0}, Velocity{10, 0, 0});
        auto e2 = world.Spawn(Position{100, 0, 0}, Velocity{-5, 0, 0});

        for (int frame = 0; frame < 10; ++frame)
        {
            queen::Query<comb::LinearAllocator, queen::Read<Velocity>, queen::Write<Position>> query{alloc, world.GetComponentIndex()};

            query.Each([](const Velocity& vel, Position& pos) {
                pos.x += vel.dx * 0.016f;
            });
        }

        larvae::AssertTrue(world.Get<Position>(e1)->x > 0.0f);
        larvae::AssertTrue(world.Get<Position>(e2)->x < 100.0f);

        world.Despawn(e1);
        world.Despawn(e2);
    });

    // ─────────────────────────────────────────────────────────────
    // World::Query builder tests
    // ─────────────────────────────────────────────────────────────

    auto test13 = larvae::RegisterTest("QueenQuery", "WorldQueryBuilder", []() {
        comb::LinearAllocator alloc{524288};

        queen::World<comb::LinearAllocator> world{alloc};

        auto e1 = world.Spawn(Position{1, 0, 0}, Velocity{0.1f, 0, 0});
        auto e2 = world.Spawn(Position{2, 0, 0}, Velocity{0.2f, 0, 0});

        float sum = 0.0f;
        world.Query<queen::Read<Position>>().Each([&sum](const Position& pos) {
            sum += pos.x;
        });

        larvae::AssertEqual(sum, 3.0f);

        world.Despawn(e1);
        world.Despawn(e2);
    });

    auto test14 = larvae::RegisterTest("QueenQuery", "WorldQueryBuilderWithFilters", []() {
        comb::LinearAllocator alloc{524288};

        queen::World<comb::LinearAllocator> world{alloc};

        (void)world.Spawn(Position{1, 0, 0}, Player{});
        (void)world.Spawn(Position{2, 0, 0});
        (void)world.Spawn(Position{3, 0, 0}, Player{}, Dead{});

        float sum = 0.0f;
        world.Query<queen::Read<Position>, queen::With<Player>, queen::Without<Dead>>()
            .Each([&sum](const Position& pos) {
                sum += pos.x;
            });

        larvae::AssertEqual(sum, 1.0f);
    });

    auto test15 = larvae::RegisterTest("QueenQuery", "WorldQueryBuilderMutation", []() {
        comb::LinearAllocator alloc{524288};

        queen::World<comb::LinearAllocator> world{alloc};

        auto e1 = world.Spawn(Position{0, 0, 0}, Velocity{1, 0, 0});
        auto e2 = world.Spawn(Position{0, 0, 0}, Velocity{2, 0, 0});

        world.Query<queen::Read<Velocity>, queen::Write<Position>>()
            .Each([](const Velocity& vel, Position& pos) {
                pos.x += vel.dx;
            });

        larvae::AssertEqual(world.Get<Position>(e1)->x, 1.0f);
        larvae::AssertEqual(world.Get<Position>(e2)->x, 2.0f);

        world.Despawn(e1);
        world.Despawn(e2);
    });

    auto test16 = larvae::RegisterTest("QueenQuery", "WorldQueryBuilderWithEntity", []() {
        comb::LinearAllocator alloc{524288};

        queen::World<comb::LinearAllocator> world{alloc};

        auto e1 = world.Spawn(Position{1, 0, 0});
        auto e2 = world.Spawn(Position{2, 0, 0});

        int count = 0;
        world.Query<queen::Read<Position>>()
            .EachWithEntity([&count, e1, e2](queen::Entity entity, const Position& pos) {
                if (entity == e1)
                {
                    larvae::AssertEqual(pos.x, 1.0f);
                }
                else if (entity == e2)
                {
                    larvae::AssertEqual(pos.x, 2.0f);
                }
                ++count;
            });

        larvae::AssertEqual(count, 2);

        world.Despawn(e1);
        world.Despawn(e2);
    });
}
