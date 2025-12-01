#include <larvae/larvae.h>
#include <queen/core/entity_location.h>
#include <comb/linear_allocator.h>

namespace
{
    auto test1 = larvae::RegisterTest("QueenEntityLocation", "DefaultRecordIsInvalid", []() {
        queen::EntityRecord record;

        larvae::AssertFalse(record.IsValid());
        larvae::AssertEqual(record.archetype_id, queen::EntityRecord::kInvalidArchetype);
        larvae::AssertEqual(record.row, queen::EntityRecord::kInvalidRow);
    });

    auto test2 = larvae::RegisterTest("QueenEntityLocation", "SetAndGet", []() {
        comb::LinearAllocator alloc{4096};
        queen::EntityLocationMap<comb::LinearAllocator> map{alloc, 100};

        queen::Entity e{5, 0};
        queen::TypeId archetype_id = 12345;
        uint32_t row = 42;

        map.Set(e, archetype_id, row);

        queen::EntityRecord* record = map.Get(e);
        larvae::AssertNotNull(record);
        larvae::AssertTrue(record->IsValid());
        larvae::AssertEqual(record->archetype_id, archetype_id);
        larvae::AssertEqual(record->row, row);
    });

    auto test3 = larvae::RegisterTest("QueenEntityLocation", "GetNonExistentReturnsNull", []() {
        comb::LinearAllocator alloc{4096};
        queen::EntityLocationMap<comb::LinearAllocator> map{alloc, 100};

        queen::Entity e{99, 0};

        queen::EntityRecord* record = map.Get(e);
        larvae::AssertNull(record);
    });

    auto test4 = larvae::RegisterTest("QueenEntityLocation", "GetNullEntityReturnsNull", []() {
        comb::LinearAllocator alloc{4096};
        queen::EntityLocationMap<comb::LinearAllocator> map{alloc, 100};

        queen::Entity null_entity;

        queen::EntityRecord* record = map.Get(null_entity);
        larvae::AssertNull(record);
    });

    auto test5 = larvae::RegisterTest("QueenEntityLocation", "InvalidateMarksAsInvalid", []() {
        comb::LinearAllocator alloc{4096};
        queen::EntityLocationMap<comb::LinearAllocator> map{alloc, 100};

        queen::Entity e{5, 0};
        map.Set(e, 12345, 42);

        larvae::AssertTrue(map.HasValidLocation(e));

        map.Invalidate(e);

        queen::EntityRecord* record = map.Get(e);
        larvae::AssertNotNull(record);
        larvae::AssertFalse(record->IsValid());
        larvae::AssertFalse(map.HasValidLocation(e));
    });

    auto test6 = larvae::RegisterTest("QueenEntityLocation", "UpdateExistingLocation", []() {
        comb::LinearAllocator alloc{4096};
        queen::EntityLocationMap<comb::LinearAllocator> map{alloc, 100};

        queen::Entity e{5, 0};

        map.Set(e, 111, 10);
        map.Set(e, 222, 20);

        queen::EntityRecord* record = map.Get(e);
        larvae::AssertNotNull(record);
        larvae::AssertEqual(record->archetype_id, queen::TypeId{222});
        larvae::AssertEqual(record->row, 20u);
    });

    auto test7 = larvae::RegisterTest("QueenEntityLocation", "MultipleEntities", []() {
        comb::LinearAllocator alloc{4096};
        queen::EntityLocationMap<comb::LinearAllocator> map{alloc, 100};

        queen::Entity e1{0, 0};
        queen::Entity e2{5, 0};
        queen::Entity e3{10, 0};

        map.Set(e1, 100, 0);
        map.Set(e2, 200, 1);
        map.Set(e3, 300, 2);

        larvae::AssertEqual(map.Get(e1)->archetype_id, queen::TypeId{100});
        larvae::AssertEqual(map.Get(e2)->archetype_id, queen::TypeId{200});
        larvae::AssertEqual(map.Get(e3)->archetype_id, queen::TypeId{300});
    });

    auto test8 = larvae::RegisterTest("QueenEntityLocation", "Clear", []() {
        comb::LinearAllocator alloc{4096};
        queen::EntityLocationMap<comb::LinearAllocator> map{alloc, 100};

        map.Set(queen::Entity{0, 0}, 100, 0);
        map.Set(queen::Entity{5, 0}, 200, 1);

        map.Clear();

        larvae::AssertEqual(map.Size(), size_t{0});
        larvae::AssertNull(map.Get(queen::Entity{0, 0}));
    });

    auto test9 = larvae::RegisterTest("QueenEntityLocation", "SparseIndices", []() {
        comb::LinearAllocator alloc{8192};
        queen::EntityLocationMap<comb::LinearAllocator> map{alloc, 10};

        queen::Entity e{100, 0};
        map.Set(e, 999, 50);

        larvae::AssertGreaterEqual(map.Size(), size_t{101});

        queen::EntityRecord* record = map.Get(e);
        larvae::AssertNotNull(record);
        larvae::AssertEqual(record->archetype_id, queen::TypeId{999});
        larvae::AssertEqual(record->row, 50u);
    });
}
