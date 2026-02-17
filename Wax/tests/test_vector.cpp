#include <larvae/larvae.h>
#include <wax/vector_types.h>

namespace {

    // =============================================================================
    // Construction and Basic Properties
    // =============================================================================

    auto test1 = larvae::RegisterTest("WaxVector", "DefaultConstruction", []() {
        comb::LinearAllocator alloc{1024};
        wax::LinearVector<int> vec{alloc};

        larvae::AssertEqual(vec.Size(), 0u);
        larvae::AssertEqual(vec.Capacity(), 0u);
        larvae::AssertTrue(vec.IsEmpty());
        larvae::AssertNull(vec.Data());
    });

    auto test2 = larvae::RegisterTest("WaxVector", "ConstructionWithCapacity", []() {
        comb::LinearAllocator alloc{1024};
        wax::LinearVector<int> vec{alloc, 10};

        larvae::AssertEqual(vec.Size(), 0u);
        larvae::AssertEqual(vec.Capacity(), 10u);
        larvae::AssertTrue(vec.IsEmpty());
        larvae::AssertNotNull(vec.Data());
    });

    // =============================================================================
    // Push and Pop Operations
    // =============================================================================

    auto test3 = larvae::RegisterTest("WaxVector", "PushBackSingleElement", []() {
        comb::LinearAllocator alloc{1024};
        wax::LinearVector<int> vec{alloc};

        vec.PushBack(42);

        larvae::AssertEqual(vec.Size(), 1u);
        larvae::AssertFalse(vec.IsEmpty());
        larvae::AssertEqual(vec[0], 42);
    });

    auto test4 = larvae::RegisterTest("WaxVector", "PushBackMultipleElements", []() {
        comb::LinearAllocator alloc{1024};
        wax::LinearVector<int> vec{alloc};

        vec.PushBack(1);
        vec.PushBack(2);
        vec.PushBack(3);
        vec.PushBack(4);
        vec.PushBack(5);

        larvae::AssertEqual(vec.Size(), 5u);
        larvae::AssertEqual(vec[0], 1);
        larvae::AssertEqual(vec[1], 2);
        larvae::AssertEqual(vec[2], 3);
        larvae::AssertEqual(vec[3], 4);
        larvae::AssertEqual(vec[4], 5);
    });

    auto test5 = larvae::RegisterTest("WaxVector", "PushBackMove", []() {
        comb::LinearAllocator alloc{1024};
        wax::LinearVector<int> vec{alloc};

        int value = 100;
        vec.PushBack(std::move(value));

        larvae::AssertEqual(vec.Size(), 1u);
        larvae::AssertEqual(vec[0], 100);
    });

    auto test6 = larvae::RegisterTest("WaxVector", "PopBack", []() {
        comb::LinearAllocator alloc{1024};
        wax::LinearVector<int> vec{alloc};

        vec.PushBack(1);
        vec.PushBack(2);
        vec.PushBack(3);

        vec.PopBack();

        larvae::AssertEqual(vec.Size(), 2u);
        larvae::AssertEqual(vec[0], 1);
        larvae::AssertEqual(vec[1], 2);
    });

    auto test7 = larvae::RegisterTest("WaxVector", "PopBackToEmpty", []() {
        comb::LinearAllocator alloc{1024};
        wax::LinearVector<int> vec{alloc};

        vec.PushBack(1);
        vec.PopBack();

        larvae::AssertEqual(vec.Size(), 0u);
        larvae::AssertTrue(vec.IsEmpty());
    });

    // =============================================================================
    // Element Access
    // =============================================================================

    auto test8 = larvae::RegisterTest("WaxVector", "IndexOperatorRead", []() {
        comb::LinearAllocator alloc{1024};
        wax::LinearVector<int> vec{alloc};

        vec.PushBack(10);
        vec.PushBack(20);
        vec.PushBack(30);

        larvae::AssertEqual(vec[0], 10);
        larvae::AssertEqual(vec[1], 20);
        larvae::AssertEqual(vec[2], 30);
    });

    auto test9 = larvae::RegisterTest("WaxVector", "IndexOperatorWrite", []() {
        comb::LinearAllocator alloc{1024};
        wax::LinearVector<int> vec{alloc};

        vec.PushBack(1);
        vec.PushBack(2);
        vec.PushBack(3);

        vec[0] = 100;
        vec[1] = 200;
        vec[2] = 300;

        larvae::AssertEqual(vec[0], 100);
        larvae::AssertEqual(vec[1], 200);
        larvae::AssertEqual(vec[2], 300);
    });

    auto test10 = larvae::RegisterTest("WaxVector", "AtMethod", []() {
        comb::LinearAllocator alloc{1024};
        wax::LinearVector<int> vec{alloc};

        vec.PushBack(5);
        vec.PushBack(10);

        larvae::AssertEqual(vec.At(0), 5);
        larvae::AssertEqual(vec.At(1), 10);

        vec.At(0) = 50;
        larvae::AssertEqual(vec.At(0), 50);
    });

    auto test11 = larvae::RegisterTest("WaxVector", "FrontBackAccess", []() {
        comb::LinearAllocator alloc{1024};
        wax::LinearVector<int> vec{alloc};

        vec.PushBack(1);
        vec.PushBack(2);
        vec.PushBack(3);

        larvae::AssertEqual(vec.Front(), 1);
        larvae::AssertEqual(vec.Back(), 3);

        vec.Front() = 10;
        vec.Back() = 30;

        larvae::AssertEqual(vec[0], 10);
        larvae::AssertEqual(vec[2], 30);
    });

    auto test12 = larvae::RegisterTest("WaxVector", "DataPointer", []() {
        comb::LinearAllocator alloc{1024};
        wax::LinearVector<int> vec{alloc};

        vec.PushBack(1);
        vec.PushBack(2);

        int* ptr = vec.Data();
        larvae::AssertNotNull(ptr);
        larvae::AssertEqual(ptr[0], 1);
        larvae::AssertEqual(ptr[1], 2);
    });

    // =============================================================================
    // Capacity Management
    // =============================================================================

    auto test13 = larvae::RegisterTest("WaxVector", "ReserveIncreasesCapacity", []() {
        comb::LinearAllocator alloc{1024};
        wax::LinearVector<int> vec{alloc};

        vec.Reserve(50);

        larvae::AssertEqual(vec.Size(), 0u);
        larvae::AssertEqual(vec.Capacity(), 50u);
    });

    auto test14 = larvae::RegisterTest("WaxVector", "ReservePreservesElements", []() {
        comb::LinearAllocator alloc{1024};
        wax::LinearVector<int> vec{alloc};

        vec.PushBack(1);
        vec.PushBack(2);
        vec.PushBack(3);

        vec.Reserve(100);

        larvae::AssertEqual(vec.Size(), 3u);
        larvae::AssertGreaterEqual(vec.Capacity(), 100u);
        larvae::AssertEqual(vec[0], 1);
        larvae::AssertEqual(vec[1], 2);
        larvae::AssertEqual(vec[2], 3);
    });

    auto test15 = larvae::RegisterTest("WaxVector", "AutomaticGrowth", []() {
        comb::LinearAllocator alloc{1024};
        wax::LinearVector<int> vec{alloc};

        // Push beyond initial capacity
        for (int i = 0; i < 20; ++i)
        {
            vec.PushBack(i);
        }

        larvae::AssertEqual(vec.Size(), 20u);
        for (size_t i = 0; i < 20; ++i)
        {
            larvae::AssertEqual(vec[i], static_cast<int>(i));
        }
    });

    auto test16 = larvae::RegisterTest("WaxVector", "ShrinkToFit", []() {
        comb::LinearAllocator alloc{1024};
        wax::LinearVector<int> vec{alloc, 100};

        vec.PushBack(1);
        vec.PushBack(2);
        vec.PushBack(3);

        vec.ShrinkToFit();

        larvae::AssertEqual(vec.Size(), 3u);
        larvae::AssertEqual(vec.Capacity(), 3u);
        larvae::AssertEqual(vec[0], 1);
        larvae::AssertEqual(vec[1], 2);
        larvae::AssertEqual(vec[2], 3);
    });

    // =============================================================================
    // Resize Operations
    // =============================================================================

    auto test17 = larvae::RegisterTest("WaxVector", "ResizeGrow", []() {
        comb::LinearAllocator alloc{1024};
        wax::LinearVector<int> vec{alloc};

        vec.PushBack(1);
        vec.Resize(5);

        larvae::AssertEqual(vec.Size(), 5u);
        larvae::AssertEqual(vec[0], 1);
        // New elements default-initialized to 0
        larvae::AssertEqual(vec[1], 0);
        larvae::AssertEqual(vec[4], 0);
    });

    auto test18 = larvae::RegisterTest("WaxVector", "ResizeShrink", []() {
        comb::LinearAllocator alloc{1024};
        wax::LinearVector<int> vec{alloc};

        vec.PushBack(1);
        vec.PushBack(2);
        vec.PushBack(3);
        vec.PushBack(4);
        vec.PushBack(5);

        vec.Resize(2);

        larvae::AssertEqual(vec.Size(), 2u);
        larvae::AssertEqual(vec[0], 1);
        larvae::AssertEqual(vec[1], 2);
    });

    auto test19 = larvae::RegisterTest("WaxVector", "ResizeWithValue", []() {
        comb::LinearAllocator alloc{1024};
        wax::LinearVector<int> vec{alloc};

        vec.PushBack(1);
        vec.Resize(5, 42);

        larvae::AssertEqual(vec.Size(), 5u);
        larvae::AssertEqual(vec[0], 1);
        larvae::AssertEqual(vec[1], 42);
        larvae::AssertEqual(vec[4], 42);
    });

    // =============================================================================
    // Clear Operation
    // =============================================================================

    auto test20 = larvae::RegisterTest("WaxVector", "Clear", []() {
        comb::LinearAllocator alloc{1024};
        wax::LinearVector<int> vec{alloc};

        vec.PushBack(1);
        vec.PushBack(2);
        vec.PushBack(3);

        size_t old_capacity = vec.Capacity();
        vec.Clear();

        larvae::AssertEqual(vec.Size(), 0u);
        larvae::AssertTrue(vec.IsEmpty());
        larvae::AssertEqual(vec.Capacity(), old_capacity); // Capacity unchanged
    });

    // =============================================================================
    // EmplaceBack
    // =============================================================================

    auto test21 = larvae::RegisterTest("WaxVector", "EmplaceBackPrimitive", []() {
        comb::LinearAllocator alloc{1024};
        wax::LinearVector<int> vec{alloc};

        vec.EmplaceBack(42);

        larvae::AssertEqual(vec.Size(), 1u);
        larvae::AssertEqual(vec[0], 42);
    });

    auto test22 = larvae::RegisterTest("WaxVector", "EmplaceBackStruct", []() {
        struct Point { int x, y; };

        comb::LinearAllocator alloc{1024};
        wax::LinearVector<Point> vec{alloc};

        vec.EmplaceBack(10, 20);

        larvae::AssertEqual(vec.Size(), 1u);
        larvae::AssertEqual(vec[0].x, 10);
        larvae::AssertEqual(vec[0].y, 20);
    });

    // =============================================================================
    // Iterators and Range-For
    // =============================================================================

    auto test23 = larvae::RegisterTest("WaxVector", "BeginEndIterators", []() {
        comb::LinearAllocator alloc{1024};
        wax::LinearVector<int> vec{alloc};

        vec.PushBack(1);
        vec.PushBack(2);
        vec.PushBack(3);

        auto it = vec.begin();
        larvae::AssertEqual(*it, 1);

        ++it;
        larvae::AssertEqual(*it, 2);

        ++it;
        larvae::AssertEqual(*it, 3);

        ++it;
        larvae::AssertTrue(it == vec.end());
    });

    auto test24 = larvae::RegisterTest("WaxVector", "RangeForLoop", []() {
        comb::LinearAllocator alloc{1024};
        wax::LinearVector<int> vec{alloc};

        vec.PushBack(1);
        vec.PushBack(2);
        vec.PushBack(3);

        int sum = 0;
        for (int val : vec)
        {
            sum += val;
        }

        larvae::AssertEqual(sum, 6);
    });

    auto test25 = larvae::RegisterTest("WaxVector", "RangeForLoopModification", []() {
        comb::LinearAllocator alloc{1024};
        wax::LinearVector<int> vec{alloc};

        vec.PushBack(1);
        vec.PushBack(2);
        vec.PushBack(3);

        for (int& val : vec)
        {
            val *= 2;
        }

        larvae::AssertEqual(vec[0], 2);
        larvae::AssertEqual(vec[1], 4);
        larvae::AssertEqual(vec[2], 6);
    });

    // =============================================================================
    // Move Semantics
    // =============================================================================

    auto test26 = larvae::RegisterTest("WaxVector", "MoveConstructor", []() {
        comb::LinearAllocator alloc{1024};
        wax::LinearVector<int> vec1{alloc};

        vec1.PushBack(1);
        vec1.PushBack(2);
        vec1.PushBack(3);

        wax::LinearVector<int> vec2{std::move(vec1)};

        larvae::AssertEqual(vec2.Size(), 3u);
        larvae::AssertEqual(vec2[0], 1);
        larvae::AssertEqual(vec2[1], 2);
        larvae::AssertEqual(vec2[2], 3);

        larvae::AssertEqual(vec1.Size(), 0u);
        larvae::AssertNull(vec1.Data());
    });

    auto test27 = larvae::RegisterTest("WaxVector", "MoveAssignment", []() {
        comb::LinearAllocator alloc{1024};
        wax::LinearVector<int> vec1{alloc};
        wax::LinearVector<int> vec2{alloc};

        vec1.PushBack(1);
        vec1.PushBack(2);

        vec2.PushBack(10);
        vec2.PushBack(20);
        vec2.PushBack(30);

        vec2 = std::move(vec1);

        larvae::AssertEqual(vec2.Size(), 2u);
        larvae::AssertEqual(vec2[0], 1);
        larvae::AssertEqual(vec2[1], 2);

        larvae::AssertEqual(vec1.Size(), 0u);
    });

    // =============================================================================
    // Different Types
    // =============================================================================

    auto test28 = larvae::RegisterTest("WaxVector", "FloatVector", []() {
        comb::LinearAllocator alloc{1024};
        wax::LinearVector<float> vec{alloc};

        vec.PushBack(1.5f);
        vec.PushBack(2.5f);
        vec.PushBack(3.5f);

        larvae::AssertEqual(vec.Size(), 3u);
        larvae::AssertEqual(vec[0], 1.5f);
        larvae::AssertEqual(vec[2], 3.5f);
    });

    auto test29 = larvae::RegisterTest("WaxVector", "StructVector", []() {
        struct Data { int id; float value; };

        comb::LinearAllocator alloc{1024};
        wax::LinearVector<Data> vec{alloc};

        vec.PushBack({1, 1.5f});
        vec.PushBack({2, 2.5f});

        larvae::AssertEqual(vec.Size(), 2u);
        larvae::AssertEqual(vec[0].id, 1);
        larvae::AssertEqual(vec[0].value, 1.5f);
        larvae::AssertEqual(vec[1].id, 2);
        larvae::AssertEqual(vec[1].value, 2.5f);
    });

    // =============================================================================
    // Edge Cases
    // =============================================================================

    auto test30 = larvae::RegisterTest("WaxVector", "LargeVector", []() {
        comb::LinearAllocator alloc{1024 * 1024}; // 1MB
        wax::LinearVector<int> vec{alloc};

        for (int i = 0; i < 1000; ++i)
        {
            vec.PushBack(i);
        }

        larvae::AssertEqual(vec.Size(), 1000u);
        larvae::AssertEqual(vec[0], 0);
        larvae::AssertEqual(vec[999], 999);
    });

    auto test31 = larvae::RegisterTest("WaxVector", "MultipleResizes", []() {
        comb::LinearAllocator alloc{1024};
        wax::LinearVector<int> vec{alloc};

        vec.Resize(10);
        vec.Resize(5);
        vec.Resize(20);
        vec.Resize(3);

        larvae::AssertEqual(vec.Size(), 3u);
    });

    // =============================================================================
    // Copy Semantics
    // =============================================================================

    auto test32 = larvae::RegisterTest("WaxVector", "CopyConstructor", []() {
        comb::LinearAllocator alloc{1024};
        wax::LinearVector<int> vec1{alloc};

        vec1.PushBack(10);
        vec1.PushBack(20);
        vec1.PushBack(30);

        wax::LinearVector<int> vec2{vec1};

        larvae::AssertEqual(vec2.Size(), 3u);
        larvae::AssertEqual(vec2[0], 10);
        larvae::AssertEqual(vec2[1], 20);
        larvae::AssertEqual(vec2[2], 30);

        // Modify original, copy unaffected
        vec1[0] = 999;
        larvae::AssertEqual(vec2[0], 10);
    });

    auto test33 = larvae::RegisterTest("WaxVector", "CopyAssignment", []() {
        comb::LinearAllocator alloc{1024};
        wax::LinearVector<int> vec1{alloc};
        wax::LinearVector<int> vec2{alloc};

        vec1.PushBack(1);
        vec1.PushBack(2);
        vec1.PushBack(3);

        vec2.PushBack(100);

        vec2 = vec1;

        larvae::AssertEqual(vec2.Size(), 3u);
        larvae::AssertEqual(vec2[0], 1);
        larvae::AssertEqual(vec2[1], 2);
        larvae::AssertEqual(vec2[2], 3);
    });

    auto test34 = larvae::RegisterTest("WaxVector", "CopyEmptyVector", []() {
        comb::LinearAllocator alloc{1024};
        wax::LinearVector<int> vec1{alloc};
        wax::LinearVector<int> vec2{vec1};

        larvae::AssertEqual(vec2.Size(), 0u);
        larvae::AssertTrue(vec2.IsEmpty());
    });

    // =============================================================================
    // Initializer List
    // =============================================================================

    auto test35 = larvae::RegisterTest("WaxVector", "InitializerListWithAllocator", []() {
        comb::LinearAllocator alloc{1024};
        wax::LinearVector<int> vec{alloc, {10, 20, 30, 40, 50}};

        larvae::AssertEqual(vec.Size(), 5u);
        larvae::AssertEqual(vec[0], 10);
        larvae::AssertEqual(vec[1], 20);
        larvae::AssertEqual(vec[2], 30);
        larvae::AssertEqual(vec[3], 40);
        larvae::AssertEqual(vec[4], 50);
    });

    // =============================================================================
    // Non-Trivial Types
    // =============================================================================

    struct Tracked
    {
        static int alive_count;
        int value;

        Tracked(int v = 0) : value{v} { ++alive_count; }
        ~Tracked() { --alive_count; }
        Tracked(const Tracked& o) : value{o.value} { ++alive_count; }
        Tracked(Tracked&& o) noexcept : value{o.value} { o.value = -1; ++alive_count; }
        Tracked& operator=(const Tracked& o) { value = o.value; return *this; }
        Tracked& operator=(Tracked&& o) noexcept { value = o.value; o.value = -1; return *this; }
    };

    int Tracked::alive_count = 0;

    auto test36 = larvae::RegisterTest("WaxVector", "NonTrivialDestructors", []() {
        Tracked::alive_count = 0;

        {
            comb::LinearAllocator alloc{4096};
            wax::Vector<Tracked, comb::LinearAllocator> vec{alloc};

            vec.EmplaceBack(1);
            vec.EmplaceBack(2);
            vec.EmplaceBack(3);

            larvae::AssertTrue(Tracked::alive_count >= 3);

            vec.Clear();
            larvae::AssertEqual(vec.Size(), 0u);
        }

        larvae::AssertEqual(Tracked::alive_count, 0);
    });

    auto test37 = larvae::RegisterTest("WaxVector", "NonTrivialResizeShrink", []() {
        Tracked::alive_count = 0;

        comb::LinearAllocator alloc{4096};
        wax::Vector<Tracked, comb::LinearAllocator> vec{alloc};

        vec.EmplaceBack(1);
        vec.EmplaceBack(2);
        vec.EmplaceBack(3);
        vec.EmplaceBack(4);
        vec.EmplaceBack(5);

        int before = Tracked::alive_count;
        vec.Resize(2);
        // 3 elements destroyed
        larvae::AssertEqual(Tracked::alive_count, before - 3);
        larvae::AssertEqual(vec.Size(), 2u);
        larvae::AssertEqual(vec[0].value, 1);
        larvae::AssertEqual(vec[1].value, 2);
    });

    auto test38 = larvae::RegisterTest("WaxVector", "NonTrivialCopy", []() {
        Tracked::alive_count = 0;

        comb::LinearAllocator alloc{4096};
        wax::Vector<Tracked, comb::LinearAllocator> vec1{alloc};

        vec1.EmplaceBack(10);
        vec1.EmplaceBack(20);

        wax::Vector<Tracked, comb::LinearAllocator> vec2{vec1};

        larvae::AssertEqual(vec2.Size(), 2u);
        larvae::AssertEqual(vec2[0].value, 10);
        larvae::AssertEqual(vec2[1].value, 20);

        // Both vectors have live objects
        larvae::AssertTrue(Tracked::alive_count >= 4);
    });

    auto test39 = larvae::RegisterTest("WaxVector", "NonTrivialPopBack", []() {
        Tracked::alive_count = 0;

        comb::LinearAllocator alloc{4096};
        wax::Vector<Tracked, comb::LinearAllocator> vec{alloc};

        vec.EmplaceBack(1);
        vec.EmplaceBack(2);
        vec.EmplaceBack(3);

        int before = Tracked::alive_count;
        vec.PopBack();
        larvae::AssertEqual(Tracked::alive_count, before - 1);
        larvae::AssertEqual(vec.Size(), 2u);
    });
}
