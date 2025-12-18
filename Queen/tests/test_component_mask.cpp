#include <larvae/larvae.h>
#include <queen/core/component_mask.h>
#include <comb/linear_allocator.h>

namespace
{
    using namespace queen;

    // ─────────────────────────────────────────────────────────────
    // Basic Operations
    // ─────────────────────────────────────────────────────────────

    auto test1 = larvae::RegisterTest("QueenComponentMask", "DefaultEmpty", []()
    {
        comb::LinearAllocator alloc{1024};
        ComponentMask<comb::LinearAllocator> mask{alloc};

        larvae::AssertTrue(mask.None());
        larvae::AssertFalse(mask.Any());
        larvae::AssertEqual(mask.Count(), size_t{0});
    });

    auto test2 = larvae::RegisterTest("QueenComponentMask", "SetSingle", []()
    {
        comb::LinearAllocator alloc{1024};
        ComponentMask<comb::LinearAllocator> mask{alloc};

        mask.Set(42);

        larvae::AssertTrue(mask.Test(42));
        larvae::AssertFalse(mask.Test(0));
        larvae::AssertFalse(mask.Test(41));
        larvae::AssertFalse(mask.Test(43));
        larvae::AssertEqual(mask.Count(), size_t{1});
        larvae::AssertTrue(mask.Any());
        larvae::AssertFalse(mask.None());
    });

    auto test3 = larvae::RegisterTest("QueenComponentMask", "SetMultiple", []()
    {
        comb::LinearAllocator alloc{1024};
        ComponentMask<comb::LinearAllocator> mask{alloc};

        mask.Set(0);
        mask.Set(63);
        mask.Set(64);
        mask.Set(127);
        mask.Set(200);

        larvae::AssertTrue(mask.Test(0));
        larvae::AssertTrue(mask.Test(63));
        larvae::AssertTrue(mask.Test(64));
        larvae::AssertTrue(mask.Test(127));
        larvae::AssertTrue(mask.Test(200));
        larvae::AssertFalse(mask.Test(1));
        larvae::AssertFalse(mask.Test(128));
        larvae::AssertEqual(mask.Count(), size_t{5});
    });

    auto test4 = larvae::RegisterTest("QueenComponentMask", "Clear", []()
    {
        comb::LinearAllocator alloc{1024};
        ComponentMask<comb::LinearAllocator> mask{alloc};

        mask.Set(10);
        mask.Set(20);
        mask.Set(30);

        larvae::AssertEqual(mask.Count(), size_t{3});

        mask.Clear(20);

        larvae::AssertTrue(mask.Test(10));
        larvae::AssertFalse(mask.Test(20));
        larvae::AssertTrue(mask.Test(30));
        larvae::AssertEqual(mask.Count(), size_t{2});
    });

    auto test5 = larvae::RegisterTest("QueenComponentMask", "ClearNonexistent", []()
    {
        comb::LinearAllocator alloc{1024};
        ComponentMask<comb::LinearAllocator> mask{alloc};

        mask.Set(10);
        mask.Clear(999);  // Should not crash

        larvae::AssertTrue(mask.Test(10));
        larvae::AssertEqual(mask.Count(), size_t{1});
    });

    auto test6 = larvae::RegisterTest("QueenComponentMask", "Toggle", []()
    {
        comb::LinearAllocator alloc{1024};
        ComponentMask<comb::LinearAllocator> mask{alloc};

        mask.Toggle(5);
        larvae::AssertTrue(mask.Test(5));

        mask.Toggle(5);
        larvae::AssertFalse(mask.Test(5));

        mask.Toggle(5);
        larvae::AssertTrue(mask.Test(5));
    });

    auto test7 = larvae::RegisterTest("QueenComponentMask", "ClearAll", []()
    {
        comb::LinearAllocator alloc{1024};
        ComponentMask<comb::LinearAllocator> mask{alloc};

        mask.Set(0);
        mask.Set(50);
        mask.Set(100);
        mask.Set(150);

        larvae::AssertEqual(mask.Count(), size_t{4});

        mask.ClearAll();

        larvae::AssertTrue(mask.None());
        larvae::AssertEqual(mask.Count(), size_t{0});
    });

    auto test8 = larvae::RegisterTest("QueenComponentMask", "SetAll", []()
    {
        comb::LinearAllocator alloc{1024};
        ComponentMask<comb::LinearAllocator> mask{alloc};

        mask.SetAll(10);

        for (size_t i = 0; i < 10; ++i)
        {
            larvae::AssertTrue(mask.Test(i));
        }
        larvae::AssertFalse(mask.Test(10));
        larvae::AssertEqual(mask.Count(), size_t{10});
    });

    auto test9 = larvae::RegisterTest("QueenComponentMask", "SetAll64Aligned", []()
    {
        comb::LinearAllocator alloc{1024};
        ComponentMask<comb::LinearAllocator> mask{alloc};

        mask.SetAll(64);

        for (size_t i = 0; i < 64; ++i)
        {
            larvae::AssertTrue(mask.Test(i));
        }
        larvae::AssertFalse(mask.Test(64));
        larvae::AssertEqual(mask.Count(), size_t{64});
    });

    // ─────────────────────────────────────────────────────────────
    // Logical Operations
    // ─────────────────────────────────────────────────────────────

    auto test10 = larvae::RegisterTest("QueenComponentMask", "Intersects", []()
    {
        comb::LinearAllocator alloc{1024};
        ComponentMask<comb::LinearAllocator> a{alloc};
        ComponentMask<comb::LinearAllocator> b{alloc};

        a.Set(1);
        a.Set(2);
        a.Set(3);

        b.Set(3);
        b.Set(4);
        b.Set(5);

        larvae::AssertTrue(a.Intersects(b));
        larvae::AssertTrue(b.Intersects(a));
    });

    auto test11 = larvae::RegisterTest("QueenComponentMask", "Disjoint", []()
    {
        comb::LinearAllocator alloc{1024};
        ComponentMask<comb::LinearAllocator> a{alloc};
        ComponentMask<comb::LinearAllocator> b{alloc};

        a.Set(1);
        a.Set(2);

        b.Set(3);
        b.Set(4);

        larvae::AssertTrue(a.Disjoint(b));
        larvae::AssertTrue(b.Disjoint(a));
        larvae::AssertFalse(a.Intersects(b));
    });

    auto test12 = larvae::RegisterTest("QueenComponentMask", "ContainsAll", []()
    {
        comb::LinearAllocator alloc{1024};
        ComponentMask<comb::LinearAllocator> superset{alloc};
        ComponentMask<comb::LinearAllocator> subset{alloc};

        superset.Set(1);
        superset.Set(2);
        superset.Set(3);
        superset.Set(4);

        subset.Set(2);
        subset.Set(3);

        larvae::AssertTrue(superset.ContainsAll(subset));
        larvae::AssertFalse(subset.ContainsAll(superset));
    });

    auto test13 = larvae::RegisterTest("QueenComponentMask", "AndOperator", []()
    {
        comb::LinearAllocator alloc{1024};
        ComponentMask<comb::LinearAllocator> a{alloc};
        ComponentMask<comb::LinearAllocator> b{alloc};

        a.Set(1);
        a.Set(2);
        a.Set(3);

        b.Set(2);
        b.Set(3);
        b.Set(4);

        auto result = a & b;

        larvae::AssertFalse(result.Test(1));
        larvae::AssertTrue(result.Test(2));
        larvae::AssertTrue(result.Test(3));
        larvae::AssertFalse(result.Test(4));
        larvae::AssertEqual(result.Count(), size_t{2});
    });

    auto test14 = larvae::RegisterTest("QueenComponentMask", "OrOperator", []()
    {
        comb::LinearAllocator alloc{1024};
        ComponentMask<comb::LinearAllocator> a{alloc};
        ComponentMask<comb::LinearAllocator> b{alloc};

        a.Set(1);
        a.Set(2);

        b.Set(3);
        b.Set(4);

        auto result = a | b;

        larvae::AssertTrue(result.Test(1));
        larvae::AssertTrue(result.Test(2));
        larvae::AssertTrue(result.Test(3));
        larvae::AssertTrue(result.Test(4));
        larvae::AssertEqual(result.Count(), size_t{4});
    });

    auto test15 = larvae::RegisterTest("QueenComponentMask", "XorOperator", []()
    {
        comb::LinearAllocator alloc{1024};
        ComponentMask<comb::LinearAllocator> a{alloc};
        ComponentMask<comb::LinearAllocator> b{alloc};

        a.Set(1);
        a.Set(2);
        a.Set(3);

        b.Set(2);
        b.Set(3);
        b.Set(4);

        auto result = a ^ b;

        larvae::AssertTrue(result.Test(1));
        larvae::AssertFalse(result.Test(2));
        larvae::AssertFalse(result.Test(3));
        larvae::AssertTrue(result.Test(4));
        larvae::AssertEqual(result.Count(), size_t{2});
    });

    auto test16 = larvae::RegisterTest("QueenComponentMask", "Invert", []()
    {
        comb::LinearAllocator alloc{1024};
        ComponentMask<comb::LinearAllocator> mask{alloc};

        mask.SetAll(4);  // Set bits 0,1,2,3
        mask.Invert();

        larvae::AssertFalse(mask.Test(0));
        larvae::AssertFalse(mask.Test(1));
        larvae::AssertFalse(mask.Test(2));
        larvae::AssertFalse(mask.Test(3));
        larvae::AssertTrue(mask.Test(4));
        larvae::AssertTrue(mask.Test(63));  // All other bits in block are now set
    });

    // ─────────────────────────────────────────────────────────────
    // Equality
    // ─────────────────────────────────────────────────────────────

    auto test17 = larvae::RegisterTest("QueenComponentMask", "Equality", []()
    {
        comb::LinearAllocator alloc{1024};
        ComponentMask<comb::LinearAllocator> a{alloc};
        ComponentMask<comb::LinearAllocator> b{alloc};

        a.Set(1);
        a.Set(100);

        b.Set(1);
        b.Set(100);

        larvae::AssertTrue(a == b);
        larvae::AssertFalse(a != b);
    });

    auto test18 = larvae::RegisterTest("QueenComponentMask", "Inequality", []()
    {
        comb::LinearAllocator alloc{1024};
        ComponentMask<comb::LinearAllocator> a{alloc};
        ComponentMask<comb::LinearAllocator> b{alloc};

        a.Set(1);
        b.Set(2);

        larvae::AssertFalse(a == b);
        larvae::AssertTrue(a != b);
    });

    auto test19 = larvae::RegisterTest("QueenComponentMask", "EqualityDifferentSizes", []()
    {
        comb::LinearAllocator alloc{1024};
        ComponentMask<comb::LinearAllocator> a{alloc};
        ComponentMask<comb::LinearAllocator> b{alloc};

        a.Set(1);
        b.Set(1);
        b.Set(100);
        b.Clear(100);  // b now has more blocks but same bits set

        larvae::AssertTrue(a == b);
    });

    // ─────────────────────────────────────────────────────────────
    // First/Last Set Bit
    // ─────────────────────────────────────────────────────────────

    auto test20 = larvae::RegisterTest("QueenComponentMask", "FirstSetBit", []()
    {
        comb::LinearAllocator alloc{1024};
        ComponentMask<comb::LinearAllocator> mask{alloc};

        mask.Set(50);
        mask.Set(100);
        mask.Set(150);

        larvae::AssertEqual(mask.FirstSetBit(), size_t{50});
    });

    auto test21 = larvae::RegisterTest("QueenComponentMask", "FirstSetBitEmpty", []()
    {
        comb::LinearAllocator alloc{1024};
        ComponentMask<comb::LinearAllocator> mask{alloc};

        larvae::AssertEqual(mask.FirstSetBit(), static_cast<size_t>(-1));
    });

    auto test22 = larvae::RegisterTest("QueenComponentMask", "LastSetBit", []()
    {
        comb::LinearAllocator alloc{1024};
        ComponentMask<comb::LinearAllocator> mask{alloc};

        mask.Set(50);
        mask.Set(100);
        mask.Set(150);

        larvae::AssertEqual(mask.LastSetBit(), size_t{150});
    });

    auto test23 = larvae::RegisterTest("QueenComponentMask", "LastSetBitEmpty", []()
    {
        comb::LinearAllocator alloc{1024};
        ComponentMask<comb::LinearAllocator> mask{alloc};

        larvae::AssertEqual(mask.LastSetBit(), static_cast<size_t>(-1));
    });

    // ─────────────────────────────────────────────────────────────
    // Capacity and Reserve
    // ─────────────────────────────────────────────────────────────

    auto test24 = larvae::RegisterTest("QueenComponentMask", "Capacity", []()
    {
        comb::LinearAllocator alloc{1024};
        ComponentMask<comb::LinearAllocator> mask{alloc};

        larvae::AssertEqual(mask.Capacity(), size_t{0});

        mask.Set(0);
        larvae::AssertTrue(mask.Capacity() >= 64);

        mask.Set(100);
        larvae::AssertTrue(mask.Capacity() >= 128);
    });

    auto test25 = larvae::RegisterTest("QueenComponentMask", "Reserve", []()
    {
        comb::LinearAllocator alloc{1024};
        ComponentMask<comb::LinearAllocator> mask{alloc};

        mask.Reserve(256);

        // Reserve pre-allocates but doesn't change Size/Capacity
        // It just reserves underlying vector capacity
        larvae::AssertTrue(mask.None());

        // After setting a bit, capacity should be sufficient
        mask.Set(200);
        larvae::AssertTrue(mask.Capacity() >= 200);
    });

    // ─────────────────────────────────────────────────────────────
    // Copy/Move
    // ─────────────────────────────────────────────────────────────

    auto test26 = larvae::RegisterTest("QueenComponentMask", "Copy", []()
    {
        comb::LinearAllocator alloc{1024};
        ComponentMask<comb::LinearAllocator> original{alloc};

        original.Set(10);
        original.Set(20);
        original.Set(30);

        ComponentMask<comb::LinearAllocator> copy{original};

        larvae::AssertTrue(copy.Test(10));
        larvae::AssertTrue(copy.Test(20));
        larvae::AssertTrue(copy.Test(30));
        larvae::AssertEqual(copy.Count(), size_t{3});

        // Modify original, copy should not change
        original.Clear(20);
        larvae::AssertTrue(copy.Test(20));
    });

    auto test27 = larvae::RegisterTest("QueenComponentMask", "CopyAssignment", []()
    {
        comb::LinearAllocator alloc{1024};
        ComponentMask<comb::LinearAllocator> original{alloc};
        ComponentMask<comb::LinearAllocator> copy{alloc};

        original.Set(10);
        original.Set(20);

        copy.Set(99);  // Should be overwritten

        copy = original;

        larvae::AssertTrue(copy.Test(10));
        larvae::AssertTrue(copy.Test(20));
        larvae::AssertFalse(copy.Test(99));
        larvae::AssertEqual(copy.Count(), size_t{2});
    });

    // ─────────────────────────────────────────────────────────────
    // Edge Cases
    // ─────────────────────────────────────────────────────────────

    auto test28 = larvae::RegisterTest("QueenComponentMask", "Bit63", []()
    {
        comb::LinearAllocator alloc{1024};
        ComponentMask<comb::LinearAllocator> mask{alloc};

        mask.Set(63);

        larvae::AssertTrue(mask.Test(63));
        larvae::AssertFalse(mask.Test(62));
        larvae::AssertFalse(mask.Test(64));
        larvae::AssertEqual(mask.Count(), size_t{1});
    });

    auto test29 = larvae::RegisterTest("QueenComponentMask", "Bit64", []()
    {
        comb::LinearAllocator alloc{1024};
        ComponentMask<comb::LinearAllocator> mask{alloc};

        mask.Set(64);

        larvae::AssertTrue(mask.Test(64));
        larvae::AssertFalse(mask.Test(63));
        larvae::AssertFalse(mask.Test(65));
        larvae::AssertEqual(mask.Count(), size_t{1});
        larvae::AssertTrue(mask.BlockCount() >= 2);
    });

    auto test30 = larvae::RegisterTest("QueenComponentMask", "LargeIndex", []()
    {
        comb::LinearAllocator alloc{4096};
        ComponentMask<comb::LinearAllocator> mask{alloc};

        mask.Set(1000);

        larvae::AssertTrue(mask.Test(1000));
        larvae::AssertFalse(mask.Test(999));
        larvae::AssertFalse(mask.Test(1001));
        larvae::AssertEqual(mask.Count(), size_t{1});
    });

    auto test31 = larvae::RegisterTest("QueenComponentMask", "StressManyBits", []()
    {
        comb::LinearAllocator alloc{8192};
        ComponentMask<comb::LinearAllocator> mask{alloc};

        // Set every 7th bit
        for (size_t i = 0; i < 1000; i += 7)
        {
            mask.Set(i);
        }

        size_t expected_count = (1000 + 6) / 7;
        larvae::AssertEqual(mask.Count(), expected_count);

        // Verify all bits
        for (size_t i = 0; i < 1000; i += 7)
        {
            larvae::AssertTrue(mask.Test(i));
        }
    });

}  // namespace
