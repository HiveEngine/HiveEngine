#include <larvae/larvae.h>
#include <queen/core/entity.h>
#include <wax/containers/hash_set.h>
#include <comb/linear_allocator.h>

namespace
{
    auto test1 = larvae::RegisterTest("QueenEntity", "DefaultIsNull", []() {
        queen::Entity e;
        larvae::AssertTrue(e.IsNull());
        larvae::AssertFalse(e.IsAlive());
    });

    auto test2 = larvae::RegisterTest("QueenEntity", "InvalidIsNull", []() {
        queen::Entity e = queen::Entity::Invalid();
        larvae::AssertTrue(e.IsNull());
    });

    auto test3 = larvae::RegisterTest("QueenEntity", "ConstructorStoresValues", []() {
        queen::Entity e{42, 7, queen::Entity::Flags::kAlive};

        larvae::AssertEqual(e.Index(), 42u);
        larvae::AssertEqual(e.Generation(), static_cast<uint16_t>(7));
        larvae::AssertTrue(e.IsAlive());
        larvae::AssertFalse(e.IsNull());
    });

    auto test4 = larvae::RegisterTest("QueenEntity", "EqualityComparison", []() {
        queen::Entity e1{10, 5, queen::Entity::Flags::kAlive};
        queen::Entity e2{10, 5, queen::Entity::Flags::kAlive};
        queen::Entity e3{10, 6, queen::Entity::Flags::kAlive};
        queen::Entity e4{11, 5, queen::Entity::Flags::kAlive};

        larvae::AssertTrue(e1 == e2);
        larvae::AssertFalse(e1 != e2);
        larvae::AssertFalse(e1 == e3);
        larvae::AssertFalse(e1 == e4);
    });

    auto test5 = larvae::RegisterTest("QueenEntity", "LessThanComparison", []() {
        queen::Entity e1{10, 5};
        queen::Entity e2{11, 5};
        queen::Entity e3{10, 6};

        larvae::AssertTrue(e1 < e2);
        larvae::AssertTrue(e1 < e3);
        larvae::AssertFalse(e2 < e1);
    });

    auto test6 = larvae::RegisterTest("QueenEntity", "FlagsOperations", []() {
        queen::Entity e{0, 0, queen::Entity::Flags::kNone};

        larvae::AssertFalse(e.IsAlive());
        larvae::AssertFalse(e.IsDisabled());
        larvae::AssertFalse(e.IsPendingDelete());

        e.SetFlag(queen::Entity::Flags::kAlive);
        larvae::AssertTrue(e.IsAlive());

        e.SetFlag(queen::Entity::Flags::kDisabled);
        larvae::AssertTrue(e.IsDisabled());
        larvae::AssertTrue(e.IsAlive());

        e.ClearFlag(queen::Entity::Flags::kAlive);
        larvae::AssertFalse(e.IsAlive());
        larvae::AssertTrue(e.IsDisabled());
    });

    auto test7 = larvae::RegisterTest("QueenEntity", "ToAndFromU64", []() {
        queen::Entity original{12345, 67, queen::Entity::Flags::kAlive | queen::Entity::Flags::kDisabled};

        uint64_t packed = original.ToU64();
        queen::Entity restored = queen::Entity::FromU64(packed);

        larvae::AssertEqual(restored.Index(), original.Index());
        larvae::AssertEqual(restored.Generation(), original.Generation());
        larvae::AssertEqual(restored.GetFlags(), original.GetFlags());
    });

    auto test8 = larvae::RegisterTest("QueenEntity", "HashWorks", []() {
        comb::LinearAllocator alloc{4096};
        wax::HashSet<queen::Entity, comb::LinearAllocator> set{alloc, 16};

        queen::Entity e1{1, 0};
        queen::Entity e2{2, 0};
        queen::Entity e3{1, 0};

        set.Insert(e1);
        set.Insert(e2);
        set.Insert(e3);

        larvae::AssertEqual(set.Count(), size_t{2});
        larvae::AssertTrue(set.Contains(e1));
        larvae::AssertTrue(set.Contains(e2));
    });

    auto test9 = larvae::RegisterTest("QueenEntity", "SizeIs8Bytes", []() {
        larvae::AssertEqual(sizeof(queen::Entity), size_t{8});
    });

    auto test10 = larvae::RegisterTest("QueenEntity", "MaxValues", []() {
        queen::Entity e{queen::Entity::kMaxIndex, queen::Entity::kMaxGeneration};

        larvae::AssertEqual(e.Index(), queen::Entity::kMaxIndex);
        larvae::AssertEqual(e.Generation(), queen::Entity::kMaxGeneration);
    });
}
