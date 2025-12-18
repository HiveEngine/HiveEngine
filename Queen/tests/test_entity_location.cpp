#include <larvae/larvae.h>
#include <queen/core/entity_location.h>
#include <comb/linear_allocator.h>

namespace
{
    struct MockArchetype
    {
        int id;
    };

    using Record = queen::EntityRecordT<MockArchetype>;
    using LocationMap = queen::EntityLocationMap<comb::LinearAllocator, MockArchetype>;

    auto test1 = larvae::RegisterTest("QueenEntityLocation", "DefaultRecordIsInvalid", []() {
        Record record;

        larvae::AssertFalse(record.IsValid());
        larvae::AssertNull(record.archetype);
        larvae::AssertEqual(record.row, Record::kInvalidRow);
    });

    auto test2 = larvae::RegisterTest("QueenEntityLocation", "SetAndGet", []() {
        comb::LinearAllocator alloc{4096};
        LocationMap map{alloc, 100};

        MockArchetype arch{42};
        queen::Entity e{5, 0};

        map.Set(e, Record{&arch, 10});

        Record* record = map.Get(e);
        larvae::AssertNotNull(record);
        larvae::AssertTrue(record->IsValid());
        larvae::AssertEqual(record->archetype, &arch);
        larvae::AssertEqual(record->row, 10u);
    });

    auto test3 = larvae::RegisterTest("QueenEntityLocation", "GetNonExistentReturnsNull", []() {
        comb::LinearAllocator alloc{4096};
        LocationMap map{alloc, 100};

        queen::Entity e{99, 0};

        Record* record = map.Get(e);
        larvae::AssertNull(record);
    });

    auto test4 = larvae::RegisterTest("QueenEntityLocation", "GetNullEntityReturnsNull", []() {
        comb::LinearAllocator alloc{4096};
        LocationMap map{alloc, 100};

        queen::Entity null_entity;

        Record* record = map.Get(null_entity);
        larvae::AssertNull(record);
    });

    auto test5 = larvae::RegisterTest("QueenEntityLocation", "RemoveMarksAsInvalid", []() {
        comb::LinearAllocator alloc{4096};
        LocationMap map{alloc, 100};

        MockArchetype arch{42};
        queen::Entity e{5, 0};
        map.Set(e, Record{&arch, 10});

        larvae::AssertTrue(map.HasValidLocation(e));

        map.Remove(e);

        Record* record = map.Get(e);
        larvae::AssertNotNull(record);
        larvae::AssertFalse(record->IsValid());
        larvae::AssertFalse(map.HasValidLocation(e));
    });

    auto test6 = larvae::RegisterTest("QueenEntityLocation", "UpdateExistingLocation", []() {
        comb::LinearAllocator alloc{4096};
        LocationMap map{alloc, 100};

        MockArchetype arch1{1};
        MockArchetype arch2{2};
        queen::Entity e{5, 0};

        map.Set(e, Record{&arch1, 10});
        map.Set(e, Record{&arch2, 20});

        Record* record = map.Get(e);
        larvae::AssertNotNull(record);
        larvae::AssertEqual(record->archetype, &arch2);
        larvae::AssertEqual(record->row, 20u);
    });

    auto test7 = larvae::RegisterTest("QueenEntityLocation", "MultipleEntities", []() {
        comb::LinearAllocator alloc{4096};
        LocationMap map{alloc, 100};

        MockArchetype arch1{100};
        MockArchetype arch2{200};
        MockArchetype arch3{300};

        queen::Entity e1{0, 0};
        queen::Entity e2{5, 0};
        queen::Entity e3{10, 0};

        map.Set(e1, Record{&arch1, 0});
        map.Set(e2, Record{&arch2, 1});
        map.Set(e3, Record{&arch3, 2});

        larvae::AssertEqual(map.Get(e1)->archetype->id, 100);
        larvae::AssertEqual(map.Get(e2)->archetype->id, 200);
        larvae::AssertEqual(map.Get(e3)->archetype->id, 300);
    });

    auto test8 = larvae::RegisterTest("QueenEntityLocation", "Clear", []() {
        comb::LinearAllocator alloc{4096};
        LocationMap map{alloc, 100};

        MockArchetype arch{42};
        map.Set(queen::Entity{0, 0}, Record{&arch, 0});
        map.Set(queen::Entity{5, 0}, Record{&arch, 1});

        map.Clear();

        larvae::AssertEqual(map.Size(), size_t{0});
        larvae::AssertNull(map.Get(queen::Entity{0, 0}));
    });

    auto test9 = larvae::RegisterTest("QueenEntityLocation", "SparseIndices", []() {
        comb::LinearAllocator alloc{8192};
        LocationMap map{alloc, 10};

        MockArchetype arch{999};
        queen::Entity e{100, 0};
        map.Set(e, Record{&arch, 50});

        larvae::AssertGreaterEqual(map.Size(), size_t{101});

        Record* record = map.Get(e);
        larvae::AssertNotNull(record);
        larvae::AssertEqual(record->archetype->id, 999);
        larvae::AssertEqual(record->row, 50u);
    });
}
