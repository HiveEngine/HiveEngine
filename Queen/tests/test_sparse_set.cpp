#include <larvae/larvae.h>
#include <queen/storage/sparse_set.h>
#include <comb/linear_allocator.h>

namespace
{
    struct Position
    {
        float x, y, z;

        bool operator==(const Position& other) const
        {
            return x == other.x && y == other.y && z == other.z;
        }
    };

    auto test1 = larvae::RegisterTest("QueenSparseSet", "InsertAndContains", []() {
        comb::LinearAllocator alloc{4096};
        queen::SparseSet<Position, comb::LinearAllocator> set{alloc, 100, 50};

        queen::Entity e1{0, 0};
        queen::Entity e2{5, 0};

        larvae::AssertTrue(set.Insert(e1, Position{1, 2, 3}));
        larvae::AssertTrue(set.Insert(e2, Position{4, 5, 6}));

        larvae::AssertTrue(set.Contains(e1));
        larvae::AssertTrue(set.Contains(e2));
        larvae::AssertEqual(set.Count(), size_t{2});
    });

    auto test2 = larvae::RegisterTest("QueenSparseSet", "InsertDuplicateFails", []() {
        comb::LinearAllocator alloc{4096};
        queen::SparseSet<Position, comb::LinearAllocator> set{alloc, 100, 50};

        queen::Entity e{0, 0};

        larvae::AssertTrue(set.Insert(e, Position{1, 2, 3}));
        larvae::AssertFalse(set.Insert(e, Position{4, 5, 6}));
        larvae::AssertEqual(set.Count(), size_t{1});
    });

    auto test3 = larvae::RegisterTest("QueenSparseSet", "GetReturnsCorrectData", []() {
        comb::LinearAllocator alloc{4096};
        queen::SparseSet<Position, comb::LinearAllocator> set{alloc, 100, 50};

        queen::Entity e{10, 5};
        Position expected{1, 2, 3};

        set.Insert(e, expected);

        Position* found = set.Get(e);
        larvae::AssertNotNull(found);
        larvae::AssertTrue(*found == expected);
    });

    auto test4 = larvae::RegisterTest("QueenSparseSet", "GetNonExistentReturnsNull", []() {
        comb::LinearAllocator alloc{4096};
        queen::SparseSet<Position, comb::LinearAllocator> set{alloc, 100, 50};

        queen::Entity e{10, 5};

        Position* found = set.Get(e);
        larvae::AssertNull(found);
    });

    auto test5 = larvae::RegisterTest("QueenSparseSet", "RemoveWorks", []() {
        comb::LinearAllocator alloc{4096};
        queen::SparseSet<Position, comb::LinearAllocator> set{alloc, 100, 50};

        queen::Entity e{0, 0};
        set.Insert(e, Position{1, 2, 3});

        larvae::AssertTrue(set.Contains(e));
        larvae::AssertTrue(set.Remove(e));
        larvae::AssertFalse(set.Contains(e));
        larvae::AssertEqual(set.Count(), size_t{0});
    });

    auto test6 = larvae::RegisterTest("QueenSparseSet", "RemoveNonExistentFails", []() {
        comb::LinearAllocator alloc{4096};
        queen::SparseSet<Position, comb::LinearAllocator> set{alloc, 100, 50};

        queen::Entity e{0, 0};

        larvae::AssertFalse(set.Remove(e));
    });

    auto test7 = larvae::RegisterTest("QueenSparseSet", "SwapAndPopMaintainsDensity", []() {
        comb::LinearAllocator alloc{4096};
        queen::SparseSet<Position, comb::LinearAllocator> set{alloc, 100, 50};

        queen::Entity e1{0, 0};
        queen::Entity e2{5, 0};
        queen::Entity e3{10, 0};

        set.Insert(e1, Position{1, 0, 0});
        set.Insert(e2, Position{2, 0, 0});
        set.Insert(e3, Position{3, 0, 0});

        set.Remove(e1);

        larvae::AssertEqual(set.Count(), size_t{2});
        larvae::AssertFalse(set.Contains(e1));
        larvae::AssertTrue(set.Contains(e2));
        larvae::AssertTrue(set.Contains(e3));

        larvae::AssertEqual(set.Get(e2)->x, 2.0f);
        larvae::AssertEqual(set.Get(e3)->x, 3.0f);
    });

    auto test8 = larvae::RegisterTest("QueenSparseSet", "GenerationMismatchNotContained", []() {
        comb::LinearAllocator alloc{4096};
        queen::SparseSet<Position, comb::LinearAllocator> set{alloc, 100, 50};

        queen::Entity e1{5, 0};
        queen::Entity e2{5, 1};

        set.Insert(e1, Position{1, 2, 3});

        larvae::AssertTrue(set.Contains(e1));
        larvae::AssertFalse(set.Contains(e2));
    });

    auto test9 = larvae::RegisterTest("QueenSparseSet", "ClearRemovesAll", []() {
        comb::LinearAllocator alloc{4096};
        queen::SparseSet<Position, comb::LinearAllocator> set{alloc, 100, 50};

        for (uint32_t i = 0; i < 10; ++i)
        {
            set.Insert(queen::Entity{i, 0}, Position{static_cast<float>(i), 0, 0});
        }

        larvae::AssertEqual(set.Count(), size_t{10});

        set.Clear();

        larvae::AssertEqual(set.Count(), size_t{0});
        larvae::AssertTrue(set.IsEmpty());

        for (uint32_t i = 0; i < 10; ++i)
        {
            larvae::AssertFalse(set.Contains(queen::Entity{i, 0}));
        }
    });

    auto test10 = larvae::RegisterTest("QueenSparseSet", "DenseIteration", []() {
        comb::LinearAllocator alloc{4096};
        queen::SparseSet<Position, comb::LinearAllocator> set{alloc, 100, 50};

        set.Insert(queen::Entity{0, 0}, Position{1, 0, 0});
        set.Insert(queen::Entity{5, 0}, Position{2, 0, 0});
        set.Insert(queen::Entity{10, 0}, Position{3, 0, 0});

        float sum = 0;
        for (size_t i = 0; i < set.Count(); ++i)
        {
            sum += set.DataAt(i).x;
        }

        larvae::AssertEqual(sum, 6.0f);
    });

    auto test11 = larvae::RegisterTest("QueenSparseSet", "EmplaceWorks", []() {
        comb::LinearAllocator alloc{4096};
        queen::SparseSet<Position, comb::LinearAllocator> set{alloc, 100, 50};

        queen::Entity e{0, 0};
        set.Emplace(e, 1.0f, 2.0f, 3.0f);

        larvae::AssertTrue(set.Contains(e));
        Position* pos = set.Get(e);
        larvae::AssertNotNull(pos);
        larvae::AssertEqual(pos->x, 1.0f);
        larvae::AssertEqual(pos->y, 2.0f);
        larvae::AssertEqual(pos->z, 3.0f);
    });

    struct NonTrivial
    {
        static int destructor_count;
        int value;

        NonTrivial(int v) : value{v} {}
        ~NonTrivial() { ++destructor_count; }

        NonTrivial(const NonTrivial& other) : value{other.value} {}
        NonTrivial(NonTrivial&& other) noexcept : value{other.value} { other.value = 0; }
    };

    int NonTrivial::destructor_count = 0;

    auto test12 = larvae::RegisterTest("QueenSparseSet", "DestructorCalledOnRemove", []() {
        NonTrivial::destructor_count = 0;

        {
            comb::LinearAllocator alloc{4096};
            queen::SparseSet<NonTrivial, comb::LinearAllocator> set{alloc, 100, 50};

            set.Emplace(queen::Entity{0, 0}, 1);
            set.Emplace(queen::Entity{1, 0}, 2);

            set.Remove(queen::Entity{0, 0});
            larvae::AssertGreaterEqual(NonTrivial::destructor_count, 1);
        }
    });

    auto test13 = larvae::RegisterTest("QueenSparseSet", "CanReinsertAfterRemove", []() {
        comb::LinearAllocator alloc{4096};
        queen::SparseSet<Position, comb::LinearAllocator> set{alloc, 100, 50};

        queen::Entity e{0, 0};

        set.Insert(e, Position{1, 2, 3});
        set.Remove(e);
        set.Insert(e, Position{4, 5, 6});

        larvae::AssertTrue(set.Contains(e));
        larvae::AssertEqual(set.Get(e)->x, 4.0f);
    });
}
