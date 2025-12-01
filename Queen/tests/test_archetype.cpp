#include <larvae/larvae.h>
#include <queen/storage/archetype.h>
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

    auto test1 = larvae::RegisterTest("QueenArchetype", "Creation", []() {
        comb::LinearAllocator alloc{65536};

        wax::Vector<queen::ComponentMeta, comb::LinearAllocator> metas{alloc};
        metas.PushBack(queen::ComponentMeta::Of<Position>());
        metas.PushBack(queen::ComponentMeta::Of<Velocity>());

        queen::Archetype<comb::LinearAllocator> arch{alloc, std::move(metas)};

        larvae::AssertTrue(arch.HasComponent<Position>());
        larvae::AssertTrue(arch.HasComponent<Velocity>());
        larvae::AssertFalse(arch.HasComponent<Health>());
        larvae::AssertEqual(arch.ComponentCount(), size_t{2});
    });

    auto test2 = larvae::RegisterTest("QueenArchetype", "UniqueId", []() {
        comb::LinearAllocator alloc{131072};

        wax::Vector<queen::ComponentMeta, comb::LinearAllocator> metas1{alloc};
        metas1.PushBack(queen::ComponentMeta::Of<Position>());
        metas1.PushBack(queen::ComponentMeta::Of<Velocity>());

        wax::Vector<queen::ComponentMeta, comb::LinearAllocator> metas2{alloc};
        metas2.PushBack(queen::ComponentMeta::Of<Velocity>());
        metas2.PushBack(queen::ComponentMeta::Of<Position>());

        wax::Vector<queen::ComponentMeta, comb::LinearAllocator> metas3{alloc};
        metas3.PushBack(queen::ComponentMeta::Of<Position>());

        queen::Archetype<comb::LinearAllocator> arch1{alloc, std::move(metas1)};
        queen::Archetype<comb::LinearAllocator> arch2{alloc, std::move(metas2)};
        queen::Archetype<comb::LinearAllocator> arch3{alloc, std::move(metas3)};

        larvae::AssertEqual(arch1.GetId(), arch2.GetId());
        larvae::AssertNotEqual(arch1.GetId(), arch3.GetId());
    });

    auto test3 = larvae::RegisterTest("QueenArchetype", "AllocateAndSetComponent", []() {
        comb::LinearAllocator alloc{65536};

        wax::Vector<queen::ComponentMeta, comb::LinearAllocator> metas{alloc};
        metas.PushBack(queen::ComponentMeta::Of<Position>());

        queen::Archetype<comb::LinearAllocator> arch{alloc, std::move(metas)};

        queen::Entity e{0, 0};
        uint32_t row = arch.AllocateRow(e);

        arch.SetComponent<Position>(row, Position{1.0f, 2.0f, 3.0f});

        Position* pos = arch.GetComponent<Position>(row);
        larvae::AssertNotNull(pos);
        larvae::AssertEqual(pos->x, 1.0f);
        larvae::AssertEqual(pos->y, 2.0f);
        larvae::AssertEqual(pos->z, 3.0f);
    });

    auto test4 = larvae::RegisterTest("QueenArchetype", "FreeRow", []() {
        comb::LinearAllocator alloc{65536};

        wax::Vector<queen::ComponentMeta, comb::LinearAllocator> metas{alloc};
        metas.PushBack(queen::ComponentMeta::Of<Position>());

        queen::Archetype<comb::LinearAllocator> arch{alloc, std::move(metas)};

        queen::Entity e1{0, 0};
        queen::Entity e2{1, 0};
        queen::Entity e3{2, 0};

        arch.AllocateRow(e1);
        arch.AllocateRow(e2);
        arch.AllocateRow(e3);

        arch.SetComponent<Position>(0, Position{1.0f, 0.0f, 0.0f});
        arch.SetComponent<Position>(1, Position{2.0f, 0.0f, 0.0f});
        arch.SetComponent<Position>(2, Position{3.0f, 0.0f, 0.0f});

        queen::Entity moved = arch.FreeRow(0);

        larvae::AssertEqual(arch.EntityCount(), size_t{2});
        larvae::AssertTrue(moved == e3);
        larvae::AssertEqual(arch.GetComponent<Position>(0)->x, 3.0f);
    });

    auto test5 = larvae::RegisterTest("QueenArchetype", "GetEntity", []() {
        comb::LinearAllocator alloc{65536};

        wax::Vector<queen::ComponentMeta, comb::LinearAllocator> metas{alloc};
        metas.PushBack(queen::ComponentMeta::Of<Position>());

        queen::Archetype<comb::LinearAllocator> arch{alloc, std::move(metas)};

        queen::Entity e{42, 7};
        uint32_t row = arch.AllocateRow(e);

        larvae::AssertTrue(arch.GetEntity(row) == e);
    });

    auto test6 = larvae::RegisterTest("QueenArchetype", "GetColumnIndex", []() {
        comb::LinearAllocator alloc{65536};

        wax::Vector<queen::ComponentMeta, comb::LinearAllocator> metas{alloc};
        metas.PushBack(queen::ComponentMeta::Of<Position>());
        metas.PushBack(queen::ComponentMeta::Of<Velocity>());

        queen::Archetype<comb::LinearAllocator> arch{alloc, std::move(metas)};

        size_t pos_idx = arch.GetColumnIndex<Position>();
        size_t vel_idx = arch.GetColumnIndex<Velocity>();
        size_t health_idx = arch.GetColumnIndex<Health>();

        larvae::AssertNotEqual(pos_idx, SIZE_MAX);
        larvae::AssertNotEqual(vel_idx, SIZE_MAX);
        larvae::AssertEqual(health_idx, SIZE_MAX);
    });

    auto test7 = larvae::RegisterTest("QueenArchetype", "ComponentTypesSorted", []() {
        comb::LinearAllocator alloc{65536};

        wax::Vector<queen::ComponentMeta, comb::LinearAllocator> metas{alloc};
        metas.PushBack(queen::ComponentMeta::Of<Position>());
        metas.PushBack(queen::ComponentMeta::Of<Velocity>());
        metas.PushBack(queen::ComponentMeta::Of<Health>());

        queen::Archetype<comb::LinearAllocator> arch{alloc, std::move(metas)};

        const auto& types = arch.GetComponentTypes();
        for (size_t i = 1; i < types.Size(); ++i)
        {
            larvae::AssertLessThan(types[i - 1], types[i]);
        }
    });

    auto test8 = larvae::RegisterTest("QueenArchetype", "EdgeCache", []() {
        comb::LinearAllocator alloc{131072};

        wax::Vector<queen::ComponentMeta, comb::LinearAllocator> metas1{alloc};
        metas1.PushBack(queen::ComponentMeta::Of<Position>());

        wax::Vector<queen::ComponentMeta, comb::LinearAllocator> metas2{alloc};
        metas2.PushBack(queen::ComponentMeta::Of<Position>());
        metas2.PushBack(queen::ComponentMeta::Of<Velocity>());

        queen::Archetype<comb::LinearAllocator> arch1{alloc, std::move(metas1)};
        queen::Archetype<comb::LinearAllocator> arch2{alloc, std::move(metas2)};

        arch1.SetAddEdge(queen::TypeIdOf<Velocity>(), &arch2);
        arch2.SetRemoveEdge(queen::TypeIdOf<Velocity>(), &arch1);

        larvae::AssertEqual(arch1.GetAddEdge(queen::TypeIdOf<Velocity>()), &arch2);
        larvae::AssertEqual(arch2.GetRemoveEdge(queen::TypeIdOf<Velocity>()), &arch1);
        larvae::AssertNull(arch1.GetAddEdge(queen::TypeIdOf<Health>()));
    });

    auto test9 = larvae::RegisterTest("QueenArchetype", "GetColumn", []() {
        comb::LinearAllocator alloc{65536};

        wax::Vector<queen::ComponentMeta, comb::LinearAllocator> metas{alloc};
        metas.PushBack(queen::ComponentMeta::Of<Position>());

        queen::Archetype<comb::LinearAllocator> arch{alloc, std::move(metas)};

        auto* column = arch.GetColumn<Position>();
        larvae::AssertNotNull(column);
        larvae::AssertEqual(column->GetTypeId(), queen::TypeIdOf<Position>());

        auto* invalid = arch.GetColumn<Velocity>();
        larvae::AssertNull(invalid);
    });

    auto test10 = larvae::RegisterTest("QueenArchetype", "MultipleEntities", []() {
        comb::LinearAllocator alloc{65536};

        wax::Vector<queen::ComponentMeta, comb::LinearAllocator> metas{alloc};
        metas.PushBack(queen::ComponentMeta::Of<Position>());
        metas.PushBack(queen::ComponentMeta::Of<Velocity>());

        queen::Archetype<comb::LinearAllocator> arch{alloc, std::move(metas)};

        for (uint32_t i = 0; i < 100; ++i)
        {
            queen::Entity e{i, 0};
            uint32_t row = arch.AllocateRow(e);
            arch.SetComponent<Position>(row, Position{static_cast<float>(i), 0.0f, 0.0f});
            arch.SetComponent<Velocity>(row, Velocity{static_cast<float>(i) * 0.1f, 0.0f, 0.0f});
        }

        larvae::AssertEqual(arch.EntityCount(), size_t{100});

        for (uint32_t i = 0; i < 100; ++i)
        {
            larvae::AssertEqual(arch.GetComponent<Position>(i)->x, static_cast<float>(i));
            larvae::AssertEqual(arch.GetComponent<Velocity>(i)->dx, static_cast<float>(i) * 0.1f);
        }
    });

    auto test11 = larvae::RegisterTest("QueenArchetype", "EmptyArchetype", []() {
        comb::LinearAllocator alloc{65536};

        wax::Vector<queen::ComponentMeta, comb::LinearAllocator> metas{alloc};

        queen::Archetype<comb::LinearAllocator> arch{alloc, std::move(metas)};

        larvae::AssertEqual(arch.ComponentCount(), size_t{0});
        larvae::AssertTrue(arch.IsEmpty());

        queen::Entity e{0, 0};
        arch.AllocateRow(e);

        larvae::AssertEqual(arch.EntityCount(), size_t{1});
        larvae::AssertFalse(arch.IsEmpty());
    });
}
