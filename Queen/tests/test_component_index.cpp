#include <larvae/larvae.h>
#include <queen/storage/component_index.h>
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

    struct Tag {};

    auto test1 = larvae::RegisterTest("QueenComponentIndex", "Empty", []() {
        comb::LinearAllocator alloc{65536};

        queen::ComponentIndex<comb::LinearAllocator> index{alloc};

        larvae::AssertEqual(index.ComponentTypeCount(), size_t{0});
        larvae::AssertNull(index.GetArchetypesWith<Position>());
    });

    auto test2 = larvae::RegisterTest("QueenComponentIndex", "RegisterSingleArchetype", []() {
        comb::LinearAllocator alloc{131072};

        queen::ArchetypeGraph<comb::LinearAllocator> graph{alloc};
        queen::ComponentIndex<comb::LinearAllocator> index{alloc};

        auto* empty = graph.GetEmptyArchetype();
        auto* arch = graph.GetOrCreateAddTarget<Position>(*empty);

        index.RegisterArchetype(arch);

        larvae::AssertEqual(index.ComponentTypeCount(), size_t{1});

        const auto* list = index.GetArchetypesWith<Position>();
        larvae::AssertNotNull(list);
        larvae::AssertEqual(list->Size(), size_t{1});
        larvae::AssertTrue((*list)[0] == arch);
    });

    auto test3 = larvae::RegisterTest("QueenComponentIndex", "RegisterMultipleArchetypes", []() {
        comb::LinearAllocator alloc{262144};

        queen::ArchetypeGraph<comb::LinearAllocator> graph{alloc};
        queen::ComponentIndex<comb::LinearAllocator> index{alloc};

        auto* empty = graph.GetEmptyArchetype();
        auto* arch1 = graph.GetOrCreateAddTarget<Position>(*empty);
        auto* arch2 = graph.GetOrCreateAddTarget<Velocity>(*arch1);
        auto* arch3 = graph.GetOrCreateAddTarget<Velocity>(*empty);

        index.RegisterArchetype(arch1);
        index.RegisterArchetype(arch2);
        index.RegisterArchetype(arch3);

        const auto* pos_list = index.GetArchetypesWith<Position>();
        larvae::AssertNotNull(pos_list);
        larvae::AssertEqual(pos_list->Size(), size_t{2});

        const auto* vel_list = index.GetArchetypesWith<Velocity>();
        larvae::AssertNotNull(vel_list);
        larvae::AssertEqual(vel_list->Size(), size_t{2});
    });

    auto test4 = larvae::RegisterTest("QueenComponentIndex", "GetArchetypesWithAll", []() {
        comb::LinearAllocator alloc{262144};

        queen::ArchetypeGraph<comb::LinearAllocator> graph{alloc};
        queen::ComponentIndex<comb::LinearAllocator> index{alloc};

        auto* empty = graph.GetEmptyArchetype();
        auto* arch_pos = graph.GetOrCreateAddTarget<Position>(*empty);
        auto* arch_pos_vel = graph.GetOrCreateAddTarget<Velocity>(*arch_pos);
        auto* arch_vel = graph.GetOrCreateAddTarget<Velocity>(*empty);

        index.RegisterArchetype(arch_pos);
        index.RegisterArchetype(arch_pos_vel);
        index.RegisterArchetype(arch_vel);

        auto result = index.GetArchetypesWithAll<Position, Velocity>();

        larvae::AssertEqual(result.Size(), size_t{1});
        larvae::AssertTrue(result[0] == arch_pos_vel);
    });

    auto test5 = larvae::RegisterTest("QueenComponentIndex", "GetArchetypesWithAllNotFound", []() {
        comb::LinearAllocator alloc{262144};

        queen::ArchetypeGraph<comb::LinearAllocator> graph{alloc};
        queen::ComponentIndex<comb::LinearAllocator> index{alloc};

        auto* empty = graph.GetEmptyArchetype();
        auto* arch_pos = graph.GetOrCreateAddTarget<Position>(*empty);
        auto* arch_vel = graph.GetOrCreateAddTarget<Velocity>(*empty);

        index.RegisterArchetype(arch_pos);
        index.RegisterArchetype(arch_vel);

        auto result = index.GetArchetypesWithAll<Position, Velocity>();

        larvae::AssertEqual(result.Size(), size_t{0});
    });

    auto test6 = larvae::RegisterTest("QueenComponentIndex", "GetArchetypesWithAllSingleType", []() {
        comb::LinearAllocator alloc{262144};

        queen::ArchetypeGraph<comb::LinearAllocator> graph{alloc};
        queen::ComponentIndex<comb::LinearAllocator> index{alloc};

        auto* empty = graph.GetEmptyArchetype();
        auto* arch_pos = graph.GetOrCreateAddTarget<Position>(*empty);
        auto* arch_pos_vel = graph.GetOrCreateAddTarget<Velocity>(*arch_pos);

        index.RegisterArchetype(arch_pos);
        index.RegisterArchetype(arch_pos_vel);

        auto result = index.GetArchetypesWithAll<Position>();

        larvae::AssertEqual(result.Size(), size_t{2});
    });

    auto test7 = larvae::RegisterTest("QueenComponentIndex", "GetArchetypesWithAllThreeTypes", []() {
        comb::LinearAllocator alloc{524288};

        queen::ArchetypeGraph<comb::LinearAllocator> graph{alloc};
        queen::ComponentIndex<comb::LinearAllocator> index{alloc};

        auto* empty = graph.GetEmptyArchetype();
        auto* arch1 = graph.GetOrCreateAddTarget<Position>(*empty);
        auto* arch2 = graph.GetOrCreateAddTarget<Velocity>(*arch1);
        auto* arch3 = graph.GetOrCreateAddTarget<Health>(*arch2);

        auto* arch4 = graph.GetOrCreateAddTarget<Health>(*empty);
        auto* arch5 = graph.GetOrCreateAddTarget<Position>(*arch4);

        index.RegisterArchetype(arch1);
        index.RegisterArchetype(arch2);
        index.RegisterArchetype(arch3);
        index.RegisterArchetype(arch4);
        index.RegisterArchetype(arch5);

        auto result = index.GetArchetypesWithAll<Position, Velocity, Health>();

        larvae::AssertEqual(result.Size(), size_t{1});
        larvae::AssertTrue(result[0] == arch3);
    });

    auto test8 = larvae::RegisterTest("QueenComponentIndex", "GetArchetypesWithUnregisteredType", []() {
        comb::LinearAllocator alloc{131072};

        queen::ArchetypeGraph<comb::LinearAllocator> graph{alloc};
        queen::ComponentIndex<comb::LinearAllocator> index{alloc};

        auto* empty = graph.GetEmptyArchetype();
        auto* arch = graph.GetOrCreateAddTarget<Position>(*empty);

        index.RegisterArchetype(arch);

        auto result = index.GetArchetypesWithAll<Position, Velocity>();

        larvae::AssertEqual(result.Size(), size_t{0});
    });

    auto test9 = larvae::RegisterTest("QueenComponentIndex", "EmptyArchetypeNotIndexed", []() {
        comb::LinearAllocator alloc{65536};

        queen::ArchetypeGraph<comb::LinearAllocator> graph{alloc};
        queen::ComponentIndex<comb::LinearAllocator> index{alloc};

        auto* empty = graph.GetEmptyArchetype();
        index.RegisterArchetype(empty);

        larvae::AssertEqual(index.ComponentTypeCount(), size_t{0});
    });

    auto test10 = larvae::RegisterTest("QueenComponentIndex", "RuntimeTypeIdLookup", []() {
        comb::LinearAllocator alloc{262144};

        queen::ArchetypeGraph<comb::LinearAllocator> graph{alloc};
        queen::ComponentIndex<comb::LinearAllocator> index{alloc};

        auto* empty = graph.GetEmptyArchetype();
        auto* arch = graph.GetOrCreateAddTarget<Position>(*empty);

        index.RegisterArchetype(arch);

        queen::TypeId pos_id = queen::TypeIdOf<Position>();
        const auto* list = index.GetArchetypesWith(pos_id);

        larvae::AssertNotNull(list);
        larvae::AssertEqual(list->Size(), size_t{1});
    });

    auto test11 = larvae::RegisterTest("QueenComponentIndex", "RuntimeGetArchetypesWithAll", []() {
        comb::LinearAllocator alloc{262144};

        queen::ArchetypeGraph<comb::LinearAllocator> graph{alloc};
        queen::ComponentIndex<comb::LinearAllocator> index{alloc};

        auto* empty = graph.GetEmptyArchetype();
        auto* arch_pos = graph.GetOrCreateAddTarget<Position>(*empty);
        auto* arch_pos_vel = graph.GetOrCreateAddTarget<Velocity>(*arch_pos);

        index.RegisterArchetype(arch_pos);
        index.RegisterArchetype(arch_pos_vel);

        queen::TypeId type_ids[] = {queen::TypeIdOf<Position>(), queen::TypeIdOf<Velocity>()};
        auto result = index.GetArchetypesWithAll(type_ids, 2);

        larvae::AssertEqual(result.Size(), size_t{1});
        larvae::AssertTrue(result[0] == arch_pos_vel);
    });
}
