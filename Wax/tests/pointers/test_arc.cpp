#include <comb/default_allocator.h>

#include <wax/pointers/arc.h>

#include <larvae/larvae.h>

#include <thread>

namespace
{
    struct TestStruct
    {
        int value;
        float data;
        static int construct_count;
        static int destruct_count;

        TestStruct(int v = 0, float d = 0.0f)
            : value{v}
            , data{d}
        {
            ++construct_count;
        }

        ~TestStruct()
        {
            ++destruct_count;
        }
    };

    int TestStruct::construct_count = 0;
    int TestStruct::destruct_count = 0;

    // Construction

    auto test1 = larvae::RegisterTest("WaxArc", "DefaultConstructor", []() {
        wax::Arc<int> arc;

        larvae::AssertTrue(arc.IsNull());
        larvae::AssertFalse(arc.IsValid());
        larvae::AssertEqual(arc.GetRefCount(), 0u);
    });

    auto test2 = larvae::RegisterTest("WaxArc", "CreateAndAccess", []() {
        auto arc = wax::MakeArc<int>(42);

        larvae::AssertTrue(arc.IsValid());
        larvae::AssertEqual(*arc, 42);
        larvae::AssertEqual(arc.GetRefCount(), 1u);
    });

    auto test3 = larvae::RegisterTest("WaxArc", "CreateWithAllocator", []() {
        comb::DefaultAllocator& alloc = comb::GetDefaultAllocator();

        auto arc = wax::MakeArc<int>(alloc, 42);

        larvae::AssertTrue(arc.IsValid());
        larvae::AssertEqual(*arc, 42);
        larvae::AssertEqual(arc.GetRefCount(), 1u);
    });

    auto test4 = larvae::RegisterTest("WaxArc", "GetReturnsValidPointer", []() {
        auto arc = wax::MakeArc<int>(99);

        int* ptr = arc.Get();
        larvae::AssertNotNull(ptr);
        larvae::AssertEqual(*ptr, 99);
    });

    auto test5 = larvae::RegisterTest("WaxArc", "GetReturnsNullForEmpty", []() {
        wax::Arc<int> arc;

        larvae::AssertNull(arc.Get());
    });

    auto test6 = larvae::RegisterTest("WaxArc", "CreateWithStruct", []() {
        TestStruct::construct_count = 0;

        auto arc = wax::MakeArc<TestStruct>(10, 3.14f);

        larvae::AssertEqual(TestStruct::construct_count, 1);
        larvae::AssertEqual(arc->value, 10);
        larvae::AssertEqual(arc->data, 3.14f);
    });

    // Copy Semantics (Ref Counting)

    auto test7 = larvae::RegisterTest("WaxArc", "CopyIncreasesRefCount", []() {
        auto arc1 = wax::MakeArc<int>(42);
        larvae::AssertEqual(arc1.GetRefCount(), 1u);

        auto arc2 = arc1;
        larvae::AssertEqual(arc1.GetRefCount(), 2u);
        larvae::AssertEqual(arc2.GetRefCount(), 2u);
        larvae::AssertEqual(*arc2, 42);
    });

    auto test8 = larvae::RegisterTest("WaxArc", "CopyAssignment", []() {
        auto arc1 = wax::MakeArc<int>(42);
        auto arc2 = wax::MakeArc<int>(99);

        arc2 = arc1;

        larvae::AssertEqual(arc1.GetRefCount(), 2u);
        larvae::AssertEqual(arc2.GetRefCount(), 2u);
        larvae::AssertEqual(*arc2, 42);
    });

    auto test9 = larvae::RegisterTest("WaxArc", "MultipleRefs", []() {
        auto arc1 = wax::MakeArc<int>(42);
        auto arc2 = arc1;
        auto arc3 = arc1;
        auto arc4 = arc2;

        larvae::AssertEqual(arc1.GetRefCount(), 4u);
    });

    // Move Semantics

    auto test10 = larvae::RegisterTest("WaxArc", "MoveTransfersOwnership", []() {
        auto arc1 = wax::MakeArc<int>(42);
        auto arc2 = std::move(arc1);

        larvae::AssertTrue(arc1.IsNull());
        larvae::AssertTrue(arc2.IsValid());
        larvae::AssertEqual(*arc2, 42);
        larvae::AssertEqual(arc2.GetRefCount(), 1u);
    });

    auto test11 = larvae::RegisterTest("WaxArc", "MoveAssignment", []() {
        auto arc1 = wax::MakeArc<int>(42);
        auto arc2 = wax::MakeArc<int>(99);

        arc2 = std::move(arc1);

        larvae::AssertTrue(arc1.IsNull());
        larvae::AssertEqual(*arc2, 42);
        larvae::AssertEqual(arc2.GetRefCount(), 1u);
    });

    // Destruction and Lifetime

    auto test12 = larvae::RegisterTest("WaxArc", "LastOwnerFrees", []() {
        TestStruct::construct_count = 0;
        TestStruct::destruct_count = 0;

        {
            auto arc = wax::MakeArc<TestStruct>(10, 3.14f);
            larvae::AssertEqual(TestStruct::construct_count, 1);
            larvae::AssertEqual(TestStruct::destruct_count, 0);
        }

        larvae::AssertEqual(TestStruct::destruct_count, 1);
    });

    auto test13 = larvae::RegisterTest("WaxArc", "ResetReleasesRef", []() {
        TestStruct::destruct_count = 0;

        auto arc1 = wax::MakeArc<TestStruct>(10, 3.14f);
        auto arc2 = arc1;

        larvae::AssertEqual(arc1.GetRefCount(), 2u);

        arc1.Reset();

        larvae::AssertTrue(arc1.IsNull());
        larvae::AssertTrue(arc2.IsValid());
        larvae::AssertEqual(arc2.GetRefCount(), 1u);
        larvae::AssertEqual(TestStruct::destruct_count, 0);

        arc2.Reset();
        larvae::AssertEqual(TestStruct::destruct_count, 1);
    });

    auto test14 = larvae::RegisterTest("WaxArc", "NestedScopes", []() {
        TestStruct::destruct_count = 0;

        {
            auto arc1 = wax::MakeArc<TestStruct>(10, 3.14f);
            {
                auto arc2 = arc1;
                larvae::AssertEqual(arc1.GetRefCount(), 2u);
            }
            larvae::AssertEqual(arc1.GetRefCount(), 1u);
            larvae::AssertEqual(TestStruct::destruct_count, 0);
        }

        larvae::AssertEqual(TestStruct::destruct_count, 1);
    });

    // Unique / Bool / Comparison

    auto test15 = larvae::RegisterTest("WaxArc", "IsUnique", []() {
        auto arc = wax::MakeArc<int>(42);
        larvae::AssertTrue(arc.IsUnique());

        auto arc2 = arc;
        larvae::AssertFalse(arc.IsUnique());

        arc2.Reset();
        larvae::AssertTrue(arc.IsUnique());
    });

    auto test16 = larvae::RegisterTest("WaxArc", "BoolConversion", []() {
        auto arc = wax::MakeArc<int>(42);
        larvae::AssertTrue(static_cast<bool>(arc));

        wax::Arc<int> null_arc;
        larvae::AssertFalse(static_cast<bool>(null_arc));
    });

    auto test17 = larvae::RegisterTest("WaxArc", "CompareEqual", []() {
        auto arc1 = wax::MakeArc<int>(42);
        auto arc2 = arc1;

        larvae::AssertTrue(arc1 == arc2);
    });

    auto test18 = larvae::RegisterTest("WaxArc", "CompareDifferent", []() {
        auto arc1 = wax::MakeArc<int>(42);
        auto arc2 = wax::MakeArc<int>(42);

        larvae::AssertFalse(arc1 == arc2);
    });

    auto test19 = larvae::RegisterTest("WaxArc", "CompareWithNullptr", []() {
        auto arc = wax::MakeArc<int>(42);
        wax::Arc<int> null_arc;

        larvae::AssertFalse(arc == nullptr);
        larvae::AssertTrue(null_arc == nullptr);
    });

    // Thread Safety

    auto test20 = larvae::RegisterTest("WaxArc", "ConcurrentCopyAndRead", []() {
        auto arc = wax::MakeArc<int>(42);

        // Copy to share with another thread
        auto arc_copy = arc;
        larvae::AssertEqual(arc.GetRefCount(), 2u);

        std::thread t{[moved = std::move(arc_copy)]() {
            larvae::AssertEqual(*moved, 42);
            larvae::AssertTrue(moved.IsValid());
            // arc_copy destroyed at end of lambda
        }};

        // Main thread still holds a valid ref
        larvae::AssertEqual(*arc, 42);

        t.join();

        larvae::AssertEqual(arc.GetRefCount(), 1u);
    });

    auto test21 = larvae::RegisterTest("WaxArc", "ConcurrentRefCountStress", []() {
        auto arc = wax::MakeArc<int>(42);

        constexpr int kThreadCount = 8;
        constexpr int kCopiesPerThread = 100;

        std::thread threads[kThreadCount];

        for (int i = 0; i < kThreadCount; ++i)
        {
            threads[i] = std::thread{[&arc]() {
                wax::Arc<int> copies[kCopiesPerThread];
                for (int j = 0; j < kCopiesPerThread; ++j)
                {
                    copies[j] = arc;
                }
                // All copies destroyed here
            }};
        }

        for (int i = 0; i < kThreadCount; ++i)
        {
            threads[i].join();
        }

        larvae::AssertEqual(arc.GetRefCount(), 1u);
        larvae::AssertEqual(*arc, 42);
    });

    // GetAllocator

    auto test22 = larvae::RegisterTest("WaxArc", "GetAllocator", []() {
        comb::DefaultAllocator& alloc = comb::GetDefaultAllocator();

        auto arc = wax::MakeArc<int>(alloc, 42);
        larvae::AssertNotNull(arc.GetAllocator());
        larvae::AssertEqual(arc.GetAllocator(), &alloc);

        wax::Arc<int> null_arc;
        larvae::AssertNull(null_arc.GetAllocator());
    });

    auto test23 = larvae::RegisterTest("WaxArc", "CopyFromNull", []() {
        wax::Arc<int> null_arc;
        wax::Arc<int> copy{null_arc};

        larvae::AssertTrue(copy.IsNull());
        larvae::AssertEqual(copy.GetRefCount(), 0u);
    });

    auto test24 = larvae::RegisterTest("WaxArc", "SelfCopyAssignment", []() {
        auto arc = wax::MakeArc<int>(42);

        auto& ref = arc;
        arc = ref;

        larvae::AssertTrue(arc.IsValid());
        larvae::AssertEqual(*arc, 42);
        larvae::AssertEqual(arc.GetRefCount(), 1u);
    });
} // namespace
