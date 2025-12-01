#include <larvae/larvae.h>
#include <queen/query/query_descriptor.h>
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
    // QueryDescriptor basic construction
    // ─────────────────────────────────────────────────────────────

    auto test1 = larvae::RegisterTest("QueenQueryDescriptor", "EmptyDescriptor", []() {
        comb::LinearAllocator alloc{65536};

        queen::QueryDescriptor<comb::LinearAllocator> desc{alloc};

        larvae::AssertTrue(desc.IsEmpty());
        larvae::AssertEqual(desc.TermCount(), size_t{0});
        larvae::AssertEqual(desc.RequiredCount(), size_t{0});
        larvae::AssertEqual(desc.ExcludedCount(), size_t{0});
        larvae::AssertEqual(desc.OptionalCount(), size_t{0});
    });

    auto test2 = larvae::RegisterTest("QueenQueryDescriptor", "AddTermManually", []() {
        comb::LinearAllocator alloc{65536};

        queen::QueryDescriptor<comb::LinearAllocator> desc{alloc};
        desc.AddTerm(queen::Read<Position>::ToTerm());
        desc.AddTerm(queen::Write<Velocity>::ToTerm());
        desc.Finalize();

        larvae::AssertEqual(desc.TermCount(), size_t{2});
        larvae::AssertEqual(desc.RequiredCount(), size_t{2});
        larvae::AssertEqual(desc.ExcludedCount(), size_t{0});
        larvae::AssertEqual(desc.DataAccessCount(), size_t{2});
    });

    auto test3 = larvae::RegisterTest("QueenQueryDescriptor", "AddTermTemplate", []() {
        comb::LinearAllocator alloc{65536};

        queen::QueryDescriptor<comb::LinearAllocator> desc{alloc};
        desc.AddTerm<queen::Read<Position>>();
        desc.AddTerm<queen::With<Player>>();
        desc.AddTerm<queen::Without<Dead>>();
        desc.Finalize();

        larvae::AssertEqual(desc.TermCount(), size_t{3});
        larvae::AssertEqual(desc.RequiredCount(), size_t{2});
        larvae::AssertEqual(desc.ExcludedCount(), size_t{1});
        larvae::AssertEqual(desc.DataAccessCount(), size_t{1});
    });

    auto test4 = larvae::RegisterTest("QueenQueryDescriptor", "FromTermsFactory", []() {
        comb::LinearAllocator alloc{65536};

        auto desc = queen::QueryDescriptor<comb::LinearAllocator>::FromTerms<
            queen::Read<Position>,
            queen::Write<Velocity>,
            queen::Without<Dead>
        >(alloc);

        larvae::AssertEqual(desc.TermCount(), size_t{3});
        larvae::AssertEqual(desc.RequiredCount(), size_t{2});
        larvae::AssertEqual(desc.ExcludedCount(), size_t{1});
        larvae::AssertTrue(desc.HasRequired());
        larvae::AssertTrue(desc.HasExcluded());
        larvae::AssertFalse(desc.HasOptional());
    });

    // ─────────────────────────────────────────────────────────────
    // Optional terms
    // ─────────────────────────────────────────────────────────────

    auto test5 = larvae::RegisterTest("QueenQueryDescriptor", "OptionalTerms", []() {
        comb::LinearAllocator alloc{65536};

        auto desc = queen::QueryDescriptor<comb::LinearAllocator>::FromTerms<
            queen::Read<Position>,
            queen::Maybe<Health>,
            queen::MaybeWrite<Velocity>
        >(alloc);

        larvae::AssertEqual(desc.TermCount(), size_t{3});
        larvae::AssertEqual(desc.RequiredCount(), size_t{1});
        larvae::AssertEqual(desc.OptionalCount(), size_t{2});
        larvae::AssertEqual(desc.DataAccessCount(), size_t{3});
        larvae::AssertTrue(desc.HasOptional());
    });

    // ─────────────────────────────────────────────────────────────
    // MatchesArchetype tests
    // ─────────────────────────────────────────────────────────────

    auto test6 = larvae::RegisterTest("QueenQueryDescriptor", "MatchesArchetypeWithRequired", []() {
        comb::LinearAllocator alloc{262144};

        queen::World<comb::LinearAllocator> world{alloc};

        auto e1 = world.Spawn(Position{0, 0, 0}, Velocity{1, 0, 0});

        auto* record = world.GetArchetypeGraph().GetEmptyArchetype();
        record = world.GetArchetypeGraph().GetOrCreateAddTarget(*record, queen::ComponentMeta::Of<Position>());
        record = world.GetArchetypeGraph().GetOrCreateAddTarget(*record, queen::ComponentMeta::Of<Velocity>());

        auto desc = queen::QueryDescriptor<comb::LinearAllocator>::FromTerms<
            queen::Read<Position>,
            queen::Read<Velocity>
        >(alloc);

        larvae::AssertTrue(desc.MatchesArchetype(*record));

        world.Despawn(e1);
    });

    auto test7 = larvae::RegisterTest("QueenQueryDescriptor", "MatchesArchetypeWithMissingRequired", []() {
        comb::LinearAllocator alloc{262144};

        queen::World<comb::LinearAllocator> world{alloc};

        auto e1 = world.Spawn(Position{0, 0, 0});

        auto* record = world.GetArchetypeGraph().GetEmptyArchetype();
        record = world.GetArchetypeGraph().GetOrCreateAddTarget(*record, queen::ComponentMeta::Of<Position>());

        auto desc = queen::QueryDescriptor<comb::LinearAllocator>::FromTerms<
            queen::Read<Position>,
            queen::Read<Velocity>
        >(alloc);

        larvae::AssertFalse(desc.MatchesArchetype(*record));

        world.Despawn(e1);
    });

    auto test8 = larvae::RegisterTest("QueenQueryDescriptor", "MatchesArchetypeWithExcluded", []() {
        comb::LinearAllocator alloc{262144};

        queen::World<comb::LinearAllocator> world{alloc};

        auto e1 = world.Spawn(Position{0, 0, 0}, Dead{});

        auto* record = world.GetArchetypeGraph().GetEmptyArchetype();
        record = world.GetArchetypeGraph().GetOrCreateAddTarget(*record, queen::ComponentMeta::Of<Position>());
        record = world.GetArchetypeGraph().GetOrCreateAddTarget(*record, queen::ComponentMeta::Of<Dead>());

        auto desc = queen::QueryDescriptor<comb::LinearAllocator>::FromTerms<
            queen::Read<Position>,
            queen::Without<Dead>
        >(alloc);

        larvae::AssertFalse(desc.MatchesArchetype(*record));

        world.Despawn(e1);
    });

    auto test9 = larvae::RegisterTest("QueenQueryDescriptor", "MatchesArchetypeWithoutExcluded", []() {
        comb::LinearAllocator alloc{262144};

        queen::World<comb::LinearAllocator> world{alloc};

        auto e1 = world.Spawn(Position{0, 0, 0});

        auto* record = world.GetArchetypeGraph().GetEmptyArchetype();
        record = world.GetArchetypeGraph().GetOrCreateAddTarget(*record, queen::ComponentMeta::Of<Position>());

        auto desc = queen::QueryDescriptor<comb::LinearAllocator>::FromTerms<
            queen::Read<Position>,
            queen::Without<Dead>
        >(alloc);

        larvae::AssertTrue(desc.MatchesArchetype(*record));

        world.Despawn(e1);
    });

    auto test10 = larvae::RegisterTest("QueenQueryDescriptor", "MatchesArchetypeWithOptional", []() {
        comb::LinearAllocator alloc{262144};

        queen::World<comb::LinearAllocator> world{alloc};

        auto* arch_with_health = world.GetArchetypeGraph().GetEmptyArchetype();
        arch_with_health = world.GetArchetypeGraph().GetOrCreateAddTarget(*arch_with_health, queen::ComponentMeta::Of<Position>());
        arch_with_health = world.GetArchetypeGraph().GetOrCreateAddTarget(*arch_with_health, queen::ComponentMeta::Of<Health>());

        auto* arch_without_health = world.GetArchetypeGraph().GetEmptyArchetype();
        arch_without_health = world.GetArchetypeGraph().GetOrCreateAddTarget(*arch_without_health, queen::ComponentMeta::Of<Position>());

        auto desc = queen::QueryDescriptor<comb::LinearAllocator>::FromTerms<
            queen::Read<Position>,
            queen::Maybe<Health>
        >(alloc);

        larvae::AssertTrue(desc.MatchesArchetype(*arch_with_health));
        larvae::AssertTrue(desc.MatchesArchetype(*arch_without_health));
    });

    // ─────────────────────────────────────────────────────────────
    // FindMatchingArchetypes tests
    // ─────────────────────────────────────────────────────────────

    auto test11 = larvae::RegisterTest("QueenQueryDescriptor", "FindMatchingArchetypes", []() {
        comb::LinearAllocator alloc{524288};

        queen::World<comb::LinearAllocator> world{alloc};

        auto e1 = world.Spawn(Position{0, 0, 0}, Velocity{1, 0, 0});
        auto e2 = world.Spawn(Position{5, 0, 0});
        auto e3 = world.Spawn(Position{10, 0, 0}, Velocity{-1, 0, 0}, Health{100, 100});

        auto desc = queen::QueryDescriptor<comb::LinearAllocator>::FromTerms<
            queen::Read<Position>,
            queen::Read<Velocity>
        >(alloc);

        auto matching = desc.FindMatchingArchetypes(world.GetComponentIndex());

        larvae::AssertEqual(matching.Size(), size_t{2});

        world.Despawn(e1);
        world.Despawn(e2);
        world.Despawn(e3);
    });

    auto test12 = larvae::RegisterTest("QueenQueryDescriptor", "FindMatchingArchetypesWithExclusion", []() {
        comb::LinearAllocator alloc{524288};

        queen::World<comb::LinearAllocator> world{alloc};

        auto e1 = world.Spawn(Position{0, 0, 0}, Velocity{1, 0, 0});
        auto e2 = world.Spawn(Position{5, 0, 0}, Velocity{2, 0, 0}, Dead{});
        auto e3 = world.Spawn(Position{10, 0, 0}, Velocity{-1, 0, 0});

        auto desc = queen::QueryDescriptor<comb::LinearAllocator>::FromTerms<
            queen::Read<Position>,
            queen::Read<Velocity>,
            queen::Without<Dead>
        >(alloc);

        auto matching = desc.FindMatchingArchetypes(world.GetComponentIndex());

        larvae::AssertEqual(matching.Size(), size_t{1});

        for (size_t i = 0; i < matching.Size(); ++i)
        {
            larvae::AssertFalse(matching[i]->HasComponent<Dead>());
        }

        world.Despawn(e1);
        world.Despawn(e2);
        world.Despawn(e3);
    });

    auto test13 = larvae::RegisterTest("QueenQueryDescriptor", "FindMatchingArchetypesNoMatches", []() {
        comb::LinearAllocator alloc{262144};

        queen::World<comb::LinearAllocator> world{alloc};

        auto e1 = world.Spawn(Position{0, 0, 0});

        auto desc = queen::QueryDescriptor<comb::LinearAllocator>::FromTerms<
            queen::Read<Position>,
            queen::Read<Velocity>
        >(alloc);

        auto matching = desc.FindMatchingArchetypes(world.GetComponentIndex());

        larvae::AssertEqual(matching.Size(), size_t{0});

        world.Despawn(e1);
    });

    // ─────────────────────────────────────────────────────────────
    // Data access extraction
    // ─────────────────────────────────────────────────────────────

    auto test14 = larvae::RegisterTest("QueenQueryDescriptor", "DataAccessExtraction", []() {
        comb::LinearAllocator alloc{65536};

        auto desc = queen::QueryDescriptor<comb::LinearAllocator>::FromTerms<
            queen::Read<Position>,
            queen::Write<Velocity>,
            queen::With<Player>,
            queen::Without<Dead>,
            queen::Maybe<Health>
        >(alloc);

        larvae::AssertEqual(desc.DataAccessCount(), size_t{3});

        const auto& data_terms = desc.GetDataAccessTerms();
        larvae::AssertEqual(data_terms[0].type_id, queen::TypeIdOf<Position>());
        larvae::AssertTrue(data_terms[0].IsReadOnly());
        larvae::AssertEqual(data_terms[1].type_id, queen::TypeIdOf<Velocity>());
        larvae::AssertTrue(data_terms[1].IsWritable());
        larvae::AssertEqual(data_terms[2].type_id, queen::TypeIdOf<Health>());
        larvae::AssertTrue(data_terms[2].IsOptional());
    });

    // ─────────────────────────────────────────────────────────────
    // Getters tests
    // ─────────────────────────────────────────────────────────────

    auto test15 = larvae::RegisterTest("QueenQueryDescriptor", "GetterMethods", []() {
        comb::LinearAllocator alloc{65536};

        auto desc = queen::QueryDescriptor<comb::LinearAllocator>::FromTerms<
            queen::Read<Position>,
            queen::Write<Velocity>,
            queen::Without<Dead>,
            queen::Maybe<Health>
        >(alloc);

        const auto& required = desc.GetRequired();
        larvae::AssertEqual(required.Size(), size_t{2});
        larvae::AssertEqual(required[0], queen::TypeIdOf<Position>());
        larvae::AssertEqual(required[1], queen::TypeIdOf<Velocity>());

        const auto& excluded = desc.GetExcluded();
        larvae::AssertEqual(excluded.Size(), size_t{1});
        larvae::AssertEqual(excluded[0], queen::TypeIdOf<Dead>());

        const auto& optional = desc.GetOptional();
        larvae::AssertEqual(optional.Size(), size_t{1});
        larvae::AssertEqual(optional[0], queen::TypeIdOf<Health>());
    });

    // ─────────────────────────────────────────────────────────────
    // Finalize multiple times
    // ─────────────────────────────────────────────────────────────

    auto test16 = larvae::RegisterTest("QueenQueryDescriptor", "FinalizeMultipleTimes", []() {
        comb::LinearAllocator alloc{65536};

        queen::QueryDescriptor<comb::LinearAllocator> desc{alloc};
        desc.AddTerm<queen::Read<Position>>();
        desc.Finalize();

        larvae::AssertEqual(desc.RequiredCount(), size_t{1});

        desc.AddTerm<queen::Read<Velocity>>();
        desc.Finalize();

        larvae::AssertEqual(desc.RequiredCount(), size_t{2});
    });
}
