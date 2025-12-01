#include <larvae/larvae.h>
#include <queen/storage/archetype_graph.h>
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

    auto test1 = larvae::RegisterTest("QueenArchetypeGraph", "Creation", []() {
        comb::LinearAllocator alloc{65536};

        queen::ArchetypeGraph<comb::LinearAllocator> graph{alloc};

        larvae::AssertEqual(graph.ArchetypeCount(), size_t{1});
        larvae::AssertNotNull(graph.GetEmptyArchetype());
        larvae::AssertEqual(graph.GetEmptyArchetype()->ComponentCount(), size_t{0});
    });

    auto test2 = larvae::RegisterTest("QueenArchetypeGraph", "AddComponent", []() {
        comb::LinearAllocator alloc{131072};

        queen::ArchetypeGraph<comb::LinearAllocator> graph{alloc};

        auto* empty = graph.GetEmptyArchetype();
        auto* with_pos = graph.GetOrCreateAddTarget<Position>(*empty);

        larvae::AssertNotNull(with_pos);
        larvae::AssertTrue(with_pos != empty);
        larvae::AssertTrue(with_pos->HasComponent<Position>());
        larvae::AssertEqual(with_pos->ComponentCount(), size_t{1});
        larvae::AssertEqual(graph.ArchetypeCount(), size_t{2});
    });

    auto test3 = larvae::RegisterTest("QueenArchetypeGraph", "AddMultipleComponents", []() {
        comb::LinearAllocator alloc{131072};

        queen::ArchetypeGraph<comb::LinearAllocator> graph{alloc};

        auto* empty = graph.GetEmptyArchetype();
        auto* with_pos = graph.GetOrCreateAddTarget<Position>(*empty);
        auto* with_pos_vel = graph.GetOrCreateAddTarget<Velocity>(*with_pos);

        larvae::AssertTrue(with_pos_vel->HasComponent<Position>());
        larvae::AssertTrue(with_pos_vel->HasComponent<Velocity>());
        larvae::AssertEqual(with_pos_vel->ComponentCount(), size_t{2});
        larvae::AssertEqual(graph.ArchetypeCount(), size_t{3});
    });

    auto test4 = larvae::RegisterTest("QueenArchetypeGraph", "RemoveComponent", []() {
        comb::LinearAllocator alloc{131072};

        queen::ArchetypeGraph<comb::LinearAllocator> graph{alloc};

        auto* empty = graph.GetEmptyArchetype();
        auto* with_pos = graph.GetOrCreateAddTarget<Position>(*empty);
        auto* with_pos_vel = graph.GetOrCreateAddTarget<Velocity>(*with_pos);

        auto* back_to_pos = graph.GetOrCreateRemoveTarget<Velocity>(*with_pos_vel);

        larvae::AssertTrue(back_to_pos == with_pos);
        larvae::AssertTrue(back_to_pos->HasComponent<Position>());
        larvae::AssertFalse(back_to_pos->HasComponent<Velocity>());
    });

    auto test5 = larvae::RegisterTest("QueenArchetypeGraph", "RemoveToEmpty", []() {
        comb::LinearAllocator alloc{131072};

        queen::ArchetypeGraph<comb::LinearAllocator> graph{alloc};

        auto* empty = graph.GetEmptyArchetype();
        auto* with_pos = graph.GetOrCreateAddTarget<Position>(*empty);
        auto* back_to_empty = graph.GetOrCreateRemoveTarget<Position>(*with_pos);

        larvae::AssertTrue(back_to_empty == empty);
        larvae::AssertEqual(back_to_empty->ComponentCount(), size_t{0});
    });

    auto test6 = larvae::RegisterTest("QueenArchetypeGraph", "EdgeCaching", []() {
        comb::LinearAllocator alloc{131072};

        queen::ArchetypeGraph<comb::LinearAllocator> graph{alloc};

        auto* empty = graph.GetEmptyArchetype();

        auto* with_pos1 = graph.GetOrCreateAddTarget<Position>(*empty);
        auto* with_pos2 = graph.GetOrCreateAddTarget<Position>(*empty);

        larvae::AssertTrue(with_pos1 == with_pos2);
        larvae::AssertEqual(graph.ArchetypeCount(), size_t{2});
    });

    auto test7 = larvae::RegisterTest("QueenArchetypeGraph", "AddExistingComponentNoOp", []() {
        comb::LinearAllocator alloc{131072};

        queen::ArchetypeGraph<comb::LinearAllocator> graph{alloc};

        auto* empty = graph.GetEmptyArchetype();
        auto* with_pos = graph.GetOrCreateAddTarget<Position>(*empty);

        auto* same = graph.GetOrCreateAddTarget<Position>(*with_pos);

        larvae::AssertTrue(same == with_pos);
    });

    auto test8 = larvae::RegisterTest("QueenArchetypeGraph", "RemoveNonExistingComponentNoOp", []() {
        comb::LinearAllocator alloc{131072};

        queen::ArchetypeGraph<comb::LinearAllocator> graph{alloc};

        auto* empty = graph.GetEmptyArchetype();
        auto* with_pos = graph.GetOrCreateAddTarget<Position>(*empty);

        auto* same = graph.GetOrCreateRemoveTarget<Velocity>(*with_pos);

        larvae::AssertTrue(same == with_pos);
    });

    auto test9 = larvae::RegisterTest("QueenArchetypeGraph", "GetArchetypeById", []() {
        comb::LinearAllocator alloc{131072};

        queen::ArchetypeGraph<comb::LinearAllocator> graph{alloc};

        auto* empty = graph.GetEmptyArchetype();
        auto* with_pos = graph.GetOrCreateAddTarget<Position>(*empty);

        auto* found = graph.GetArchetype(with_pos->GetId());

        larvae::AssertTrue(found == with_pos);
    });

    auto test10 = larvae::RegisterTest("QueenArchetypeGraph", "GetArchetypeByIdNotFound", []() {
        comb::LinearAllocator alloc{65536};

        queen::ArchetypeGraph<comb::LinearAllocator> graph{alloc};

        auto* found = graph.GetArchetype(12345);

        larvae::AssertNull(found);
    });

    auto test11 = larvae::RegisterTest("QueenArchetypeGraph", "DifferentPathsSameArchetype", []() {
        comb::LinearAllocator alloc{262144};

        queen::ArchetypeGraph<comb::LinearAllocator> graph{alloc};

        auto* empty = graph.GetEmptyArchetype();

        auto* pos_first = graph.GetOrCreateAddTarget<Position>(*empty);
        auto* pos_vel_path1 = graph.GetOrCreateAddTarget<Velocity>(*pos_first);

        auto* vel_first = graph.GetOrCreateAddTarget<Velocity>(*empty);
        auto* pos_vel_path2 = graph.GetOrCreateAddTarget<Position>(*vel_first);

        larvae::AssertTrue(pos_vel_path1->GetId() == pos_vel_path2->GetId());
        larvae::AssertTrue(pos_vel_path1 == pos_vel_path2);
    });

    auto test12 = larvae::RegisterTest("QueenArchetypeGraph", "BidirectionalEdges", []() {
        comb::LinearAllocator alloc{131072};

        queen::ArchetypeGraph<comb::LinearAllocator> graph{alloc};

        auto* empty = graph.GetEmptyArchetype();
        auto* with_pos = graph.GetOrCreateAddTarget<Position>(*empty);

        larvae::AssertTrue(empty->GetAddEdge(queen::TypeIdOf<Position>()) == with_pos);
        larvae::AssertTrue(with_pos->GetRemoveEdge(queen::TypeIdOf<Position>()) == empty);
    });

    auto test13 = larvae::RegisterTest("QueenArchetypeGraph", "ThreeComponentChain", []() {
        comb::LinearAllocator alloc{262144};

        queen::ArchetypeGraph<comb::LinearAllocator> graph{alloc};

        auto* empty = graph.GetEmptyArchetype();
        auto* a = graph.GetOrCreateAddTarget<Position>(*empty);
        auto* b = graph.GetOrCreateAddTarget<Velocity>(*a);
        auto* c = graph.GetOrCreateAddTarget<Health>(*b);

        larvae::AssertTrue(c->HasComponent<Position>());
        larvae::AssertTrue(c->HasComponent<Velocity>());
        larvae::AssertTrue(c->HasComponent<Health>());
        larvae::AssertEqual(c->ComponentCount(), size_t{3});

        auto* back_to_b = graph.GetOrCreateRemoveTarget<Health>(*c);
        larvae::AssertTrue(back_to_b == b);

        auto* back_to_a = graph.GetOrCreateRemoveTarget<Velocity>(*back_to_b);
        larvae::AssertTrue(back_to_a == a);

        auto* back_to_empty = graph.GetOrCreateRemoveTarget<Position>(*back_to_a);
        larvae::AssertTrue(back_to_empty == empty);
    });
}
