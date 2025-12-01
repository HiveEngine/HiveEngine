#include <larvae/larvae.h>
#include <queen/storage/table.h>
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

    auto test1 = larvae::RegisterTest("QueenTable", "AllocateRow", []() {
        comb::LinearAllocator alloc{65536};

        wax::Vector<queen::ComponentMeta, comb::LinearAllocator> metas{alloc};
        metas.PushBack(queen::ComponentMeta::Of<Position>());
        metas.PushBack(queen::ComponentMeta::Of<Velocity>());

        queen::Table<comb::LinearAllocator> table{alloc, metas, 100};

        queen::Entity e{0, 0};
        uint32_t row = table.AllocateRow(e);

        larvae::AssertEqual(row, 0u);
        larvae::AssertEqual(table.RowCount(), size_t{1});
        larvae::AssertTrue(table.GetEntity(row) == e);
    });

    auto test2 = larvae::RegisterTest("QueenTable", "AllocateMultipleRows", []() {
        comb::LinearAllocator alloc{65536};

        wax::Vector<queen::ComponentMeta, comb::LinearAllocator> metas{alloc};
        metas.PushBack(queen::ComponentMeta::Of<Position>());

        queen::Table<comb::LinearAllocator> table{alloc, metas, 100};

        queen::Entity e1{0, 0};
        queen::Entity e2{1, 0};
        queen::Entity e3{2, 0};

        uint32_t row1 = table.AllocateRow(e1);
        uint32_t row2 = table.AllocateRow(e2);
        uint32_t row3 = table.AllocateRow(e3);

        larvae::AssertEqual(row1, 0u);
        larvae::AssertEqual(row2, 1u);
        larvae::AssertEqual(row3, 2u);
        larvae::AssertEqual(table.RowCount(), size_t{3});
    });

    auto test3 = larvae::RegisterTest("QueenTable", "GetColumn", []() {
        comb::LinearAllocator alloc{65536};

        wax::Vector<queen::ComponentMeta, comb::LinearAllocator> metas{alloc};
        metas.PushBack(queen::ComponentMeta::Of<Position>());
        metas.PushBack(queen::ComponentMeta::Of<Velocity>());

        queen::Table<comb::LinearAllocator> table{alloc, metas, 100};

        auto* pos_col = table.GetColumn<Position>();
        auto* vel_col = table.GetColumn<Velocity>();
        auto* health_col = table.GetColumn<Health>();

        larvae::AssertNotNull(pos_col);
        larvae::AssertNotNull(vel_col);
        larvae::AssertNull(health_col);
    });

    auto test4 = larvae::RegisterTest("QueenTable", "SetAndGetComponent", []() {
        comb::LinearAllocator alloc{65536};

        wax::Vector<queen::ComponentMeta, comb::LinearAllocator> metas{alloc};
        metas.PushBack(queen::ComponentMeta::Of<Position>());

        queen::Table<comb::LinearAllocator> table{alloc, metas, 100};

        queen::Entity e{0, 0};
        uint32_t row = table.AllocateRow(e);

        Position pos{1.0f, 2.0f, 3.0f};
        table.SetComponent<Position>(row, pos);

        auto* col = table.GetColumn<Position>();
        Position* result = col->Get<Position>(row);

        larvae::AssertEqual(result->x, 1.0f);
        larvae::AssertEqual(result->y, 2.0f);
        larvae::AssertEqual(result->z, 3.0f);
    });

    auto test5 = larvae::RegisterTest("QueenTable", "FreeRowSwapAndPop", []() {
        comb::LinearAllocator alloc{65536};

        wax::Vector<queen::ComponentMeta, comb::LinearAllocator> metas{alloc};
        metas.PushBack(queen::ComponentMeta::Of<Position>());

        queen::Table<comb::LinearAllocator> table{alloc, metas, 100};

        queen::Entity e1{0, 0};
        queen::Entity e2{1, 0};
        queen::Entity e3{2, 0};

        table.AllocateRow(e1);
        table.AllocateRow(e2);
        table.AllocateRow(e3);

        table.SetComponent<Position>(0, Position{1.0f, 0.0f, 0.0f});
        table.SetComponent<Position>(1, Position{2.0f, 0.0f, 0.0f});
        table.SetComponent<Position>(2, Position{3.0f, 0.0f, 0.0f});

        queen::Entity moved = table.FreeRow(0);

        larvae::AssertEqual(table.RowCount(), size_t{2});
        larvae::AssertTrue(moved == e3);
        larvae::AssertTrue(table.GetEntity(0) == e3);

        auto* col = table.GetColumn<Position>();
        larvae::AssertEqual(col->Get<Position>(0)->x, 3.0f);
        larvae::AssertEqual(col->Get<Position>(1)->x, 2.0f);
    });

    auto test6 = larvae::RegisterTest("QueenTable", "FreeLastRow", []() {
        comb::LinearAllocator alloc{65536};

        wax::Vector<queen::ComponentMeta, comb::LinearAllocator> metas{alloc};
        metas.PushBack(queen::ComponentMeta::Of<Position>());

        queen::Table<comb::LinearAllocator> table{alloc, metas, 100};

        queen::Entity e1{0, 0};
        queen::Entity e2{1, 0};

        table.AllocateRow(e1);
        table.AllocateRow(e2);

        queen::Entity moved = table.FreeRow(1);

        larvae::AssertEqual(table.RowCount(), size_t{1});
        larvae::AssertTrue(moved.IsNull());
        larvae::AssertTrue(table.GetEntity(0) == e1);
    });

    auto test7 = larvae::RegisterTest("QueenTable", "HasComponent", []() {
        comb::LinearAllocator alloc{65536};

        wax::Vector<queen::ComponentMeta, comb::LinearAllocator> metas{alloc};
        metas.PushBack(queen::ComponentMeta::Of<Position>());
        metas.PushBack(queen::ComponentMeta::Of<Velocity>());

        queen::Table<comb::LinearAllocator> table{alloc, metas, 100};

        larvae::AssertTrue(table.HasComponent<Position>());
        larvae::AssertTrue(table.HasComponent<Velocity>());
        larvae::AssertFalse(table.HasComponent<Health>());
    });

    auto test8 = larvae::RegisterTest("QueenTable", "MultipleColumns", []() {
        comb::LinearAllocator alloc{65536};

        wax::Vector<queen::ComponentMeta, comb::LinearAllocator> metas{alloc};
        metas.PushBack(queen::ComponentMeta::Of<Position>());
        metas.PushBack(queen::ComponentMeta::Of<Velocity>());
        metas.PushBack(queen::ComponentMeta::Of<Health>());

        queen::Table<comb::LinearAllocator> table{alloc, metas, 100};

        queen::Entity e{0, 0};
        uint32_t row = table.AllocateRow(e);

        table.SetComponent<Position>(row, Position{1.0f, 2.0f, 3.0f});
        table.SetComponent<Velocity>(row, Velocity{0.1f, 0.2f, 0.3f});
        table.SetComponent<Health>(row, Health{100, 100});

        larvae::AssertEqual(table.GetColumn<Position>()->Get<Position>(row)->x, 1.0f);
        larvae::AssertEqual(table.GetColumn<Velocity>()->Get<Velocity>(row)->dx, 0.1f);
        larvae::AssertEqual(table.GetColumn<Health>()->Get<Health>(row)->current, 100);
    });

    auto test9 = larvae::RegisterTest("QueenTable", "ColumnCount", []() {
        comb::LinearAllocator alloc{65536};

        wax::Vector<queen::ComponentMeta, comb::LinearAllocator> metas{alloc};
        metas.PushBack(queen::ComponentMeta::Of<Position>());
        metas.PushBack(queen::ComponentMeta::Of<Velocity>());

        queen::Table<comb::LinearAllocator> table{alloc, metas, 100};

        larvae::AssertEqual(table.ColumnCount(), size_t{2});
    });

    auto test10 = larvae::RegisterTest("QueenTable", "EmptyTable", []() {
        comb::LinearAllocator alloc{65536};

        wax::Vector<queen::ComponentMeta, comb::LinearAllocator> metas{alloc};
        metas.PushBack(queen::ComponentMeta::Of<Position>());

        queen::Table<comb::LinearAllocator> table{alloc, metas, 100};

        larvae::AssertTrue(table.IsEmpty());
        larvae::AssertEqual(table.RowCount(), size_t{0});

        table.AllocateRow(queen::Entity{0, 0});

        larvae::AssertFalse(table.IsEmpty());
    });

    auto test11 = larvae::RegisterTest("QueenTable", "GetEntities", []() {
        comb::LinearAllocator alloc{65536};

        wax::Vector<queen::ComponentMeta, comb::LinearAllocator> metas{alloc};
        metas.PushBack(queen::ComponentMeta::Of<Position>());

        queen::Table<comb::LinearAllocator> table{alloc, metas, 100};

        queen::Entity e1{0, 0};
        queen::Entity e2{1, 0};

        table.AllocateRow(e1);
        table.AllocateRow(e2);

        const queen::Entity* entities = table.GetEntities();
        larvae::AssertNotNull(entities);
        larvae::AssertTrue(entities[0] == e1);
        larvae::AssertTrue(entities[1] == e2);
    });

    auto test12 = larvae::RegisterTest("QueenTable", "GetColumnByTypeId", []() {
        comb::LinearAllocator alloc{65536};

        wax::Vector<queen::ComponentMeta, comb::LinearAllocator> metas{alloc};
        metas.PushBack(queen::ComponentMeta::Of<Position>());

        queen::Table<comb::LinearAllocator> table{alloc, metas, 100};

        auto* col = table.GetColumnByTypeId(queen::TypeIdOf<Position>());
        larvae::AssertNotNull(col);
        larvae::AssertEqual(col->GetTypeId(), queen::TypeIdOf<Position>());

        auto* invalid = table.GetColumnByTypeId(queen::TypeIdOf<Velocity>());
        larvae::AssertNull(invalid);
    });
}
