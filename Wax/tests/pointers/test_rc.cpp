#include <larvae/larvae.h>
#include <wax/pointers/rc.h>
#include <comb/linear_allocator.h>

namespace {
    struct TestStruct {
        int value;
        float data;
        static int construct_count;
        static int destruct_count;

        TestStruct(int v = 0, float d = 0.0f) : value{v}, data{d} {
            ++construct_count;
        }

        ~TestStruct() {
            ++destruct_count;
        }
    };

    int TestStruct::construct_count = 0;
    int TestStruct::destruct_count = 0;

    // =============================================================================
    // Construction
    // =============================================================================

    auto test1 = larvae::RegisterTest("WaxRc", "DefaultConstructor", []() {
        wax::Rc<int, comb::LinearAllocator> rc;

        larvae::AssertTrue(rc.IsNull());
        larvae::AssertFalse(rc.IsValid());
        larvae::AssertEqual(rc.GetRefCount(), 0u);
    });

    auto test2 = larvae::RegisterTest("WaxRc", "MakeRc", []() {
        comb::LinearAllocator alloc{1024};

        auto rc = wax::MakeRc<int>(alloc, 42);

        larvae::AssertTrue(rc.IsValid());
        larvae::AssertEqual(*rc, 42);
        larvae::AssertEqual(rc.GetRefCount(), 1u);
    });

    auto test3 = larvae::RegisterTest("WaxRc", "MakeRcWithStruct", []() {
        comb::LinearAllocator alloc{1024};
        TestStruct::construct_count = 0;

        auto rc = wax::MakeRc<TestStruct>(alloc, 10, 3.14f);

        larvae::AssertEqual(TestStruct::construct_count, 1);
        larvae::AssertEqual(rc->value, 10);
        larvae::AssertEqual(rc->data, 3.14f);
        larvae::AssertEqual(rc.GetRefCount(), 1u);
    });

    // =============================================================================
    // Copy Semantics (Ref Counting)
    // =============================================================================

    auto test4 = larvae::RegisterTest("WaxRc", "CopyConstructor", []() {
        comb::LinearAllocator alloc{1024};

        auto rc1 = wax::MakeRc<int>(alloc, 42);
        larvae::AssertEqual(rc1.GetRefCount(), 1u);

        auto rc2 = rc1;
        larvae::AssertEqual(rc1.GetRefCount(), 2u);
        larvae::AssertEqual(rc2.GetRefCount(), 2u);
        larvae::AssertEqual(*rc2, 42);
    });

    auto test5 = larvae::RegisterTest("WaxRc", "CopyAssignment", []() {
        comb::LinearAllocator alloc{1024};

        auto rc1 = wax::MakeRc<int>(alloc, 42);
        auto rc2 = wax::MakeRc<int>(alloc, 99);

        larvae::AssertEqual(rc1.GetRefCount(), 1u);
        larvae::AssertEqual(rc2.GetRefCount(), 1u);

        rc2 = rc1;

        larvae::AssertEqual(rc1.GetRefCount(), 2u);
        larvae::AssertEqual(rc2.GetRefCount(), 2u);
        larvae::AssertEqual(*rc2, 42);
    });

    auto test6 = larvae::RegisterTest("WaxRc", "MultipleRefs", []() {
        comb::LinearAllocator alloc{1024};

        auto rc1 = wax::MakeRc<int>(alloc, 42);
        auto rc2 = rc1;
        auto rc3 = rc1;
        auto rc4 = rc2;

        larvae::AssertEqual(rc1.GetRefCount(), 4u);
        larvae::AssertEqual(rc2.GetRefCount(), 4u);
        larvae::AssertEqual(rc3.GetRefCount(), 4u);
        larvae::AssertEqual(rc4.GetRefCount(), 4u);
    });

    // =============================================================================
    // Move Semantics
    // =============================================================================

    auto test7 = larvae::RegisterTest("WaxRc", "MoveConstructor", []() {
        comb::LinearAllocator alloc{1024};

        auto rc1 = wax::MakeRc<int>(alloc, 42);
        auto rc2 = std::move(rc1);

        larvae::AssertTrue(rc1.IsNull());
        larvae::AssertTrue(rc2.IsValid());
        larvae::AssertEqual(*rc2, 42);
        larvae::AssertEqual(rc2.GetRefCount(), 1u);
    });

    auto test8 = larvae::RegisterTest("WaxRc", "MoveAssignment", []() {
        comb::LinearAllocator alloc{1024};

        auto rc1 = wax::MakeRc<int>(alloc, 42);
        auto rc2 = wax::MakeRc<int>(alloc, 99);

        rc2 = std::move(rc1);

        larvae::AssertTrue(rc1.IsNull());
        larvae::AssertEqual(*rc2, 42);
        larvae::AssertEqual(rc2.GetRefCount(), 1u);
    });

    // =============================================================================
    // Dereference
    // =============================================================================

    auto test9 = larvae::RegisterTest("WaxRc", "DereferenceOperator", []() {
        comb::LinearAllocator alloc{1024};

        auto rc = wax::MakeRc<int>(alloc, 42);

        larvae::AssertEqual(*rc, 42);

        *rc = 99;
        larvae::AssertEqual(*rc, 99);
    });

    auto test10 = larvae::RegisterTest("WaxRc", "ArrowOperator", []() {
        comb::LinearAllocator alloc{1024};

        auto rc = wax::MakeRc<TestStruct>(alloc, 10, 3.14f);

        larvae::AssertEqual(rc->value, 10);

        rc->value = 20;
        larvae::AssertEqual(rc->value, 20);
    });

    auto test11 = larvae::RegisterTest("WaxRc", "Get", []() {
        comb::LinearAllocator alloc{1024};

        auto rc = wax::MakeRc<int>(alloc, 42);

        int* ptr = rc.Get();
        larvae::AssertNotNull(ptr);
        larvae::AssertEqual(*ptr, 42);
    });

    // =============================================================================
    // Bool Conversion
    // =============================================================================

    auto test12 = larvae::RegisterTest("WaxRc", "BoolConversionValid", []() {
        comb::LinearAllocator alloc{1024};

        auto rc = wax::MakeRc<int>(alloc, 42);

        if (rc) {
            larvae::AssertEqual(*rc, 42);
        } else {
            larvae::AssertTrue(false); // Rc should be valid
        }
    });

    auto test13 = larvae::RegisterTest("WaxRc", "BoolConversionNull", []() {
        wax::Rc<int, comb::LinearAllocator> rc;

        if (rc) {
            larvae::AssertTrue(false); // Rc should be null
        } else {
            larvae::AssertTrue(true);
        }
    });

    // =============================================================================
    // Reset
    // =============================================================================

    auto test14 = larvae::RegisterTest("WaxRc", "Reset", []() {
        comb::LinearAllocator alloc{1024};
        TestStruct::destruct_count = 0;

        auto rc = wax::MakeRc<TestStruct>(alloc, 10, 3.14f);
        rc.Reset();

        larvae::AssertTrue(rc.IsNull());
        larvae::AssertEqual(TestStruct::destruct_count, 1);
    });

    auto test15 = larvae::RegisterTest("WaxRc", "ResetWithMultipleRefs", []() {
        comb::LinearAllocator alloc{1024};
        TestStruct::destruct_count = 0;

        auto rc1 = wax::MakeRc<TestStruct>(alloc, 10, 3.14f);
        auto rc2 = rc1;

        larvae::AssertEqual(rc1.GetRefCount(), 2u);

        rc1.Reset();

        larvae::AssertTrue(rc1.IsNull());
        larvae::AssertTrue(rc2.IsValid());
        larvae::AssertEqual(rc2.GetRefCount(), 1u);
        larvae::AssertEqual(TestStruct::destruct_count, 0);  // Not destroyed yet

        rc2.Reset();
        larvae::AssertEqual(TestStruct::destruct_count, 1);  // Now destroyed
    });

    // =============================================================================
    // Unique Check
    // =============================================================================

    auto test16 = larvae::RegisterTest("WaxRc", "IsUnique", []() {
        comb::LinearAllocator alloc{1024};

        auto rc1 = wax::MakeRc<int>(alloc, 42);
        larvae::AssertTrue(rc1.IsUnique());

        auto rc2 = rc1;
        larvae::AssertFalse(rc1.IsUnique());
        larvae::AssertFalse(rc2.IsUnique());

        rc2.Reset();
        larvae::AssertTrue(rc1.IsUnique());
    });

    // =============================================================================
    // RAII Lifetime
    // =============================================================================

    auto test17 = larvae::RegisterTest("WaxRc", "AutomaticDestruction", []() {
        comb::LinearAllocator alloc{1024};
        TestStruct::construct_count = 0;
        TestStruct::destruct_count = 0;

        {
            auto rc = wax::MakeRc<TestStruct>(alloc, 10, 3.14f);
            larvae::AssertEqual(TestStruct::construct_count, 1);
            larvae::AssertEqual(TestStruct::destruct_count, 0);
        }

        larvae::AssertEqual(TestStruct::destruct_count, 1);
    });

    auto test18 = larvae::RegisterTest("WaxRc", "NestedScopes", []() {
        comb::LinearAllocator alloc{1024};
        TestStruct::destruct_count = 0;

        {
            auto rc1 = wax::MakeRc<TestStruct>(alloc, 10, 3.14f);
            {
                auto rc2 = rc1;
                larvae::AssertEqual(rc1.GetRefCount(), 2u);
            }
            larvae::AssertEqual(rc1.GetRefCount(), 1u);
            larvae::AssertEqual(TestStruct::destruct_count, 0);
        }

        larvae::AssertEqual(TestStruct::destruct_count, 1);
    });

    auto test19 = larvae::RegisterTest("WaxRc", "SharedAcrossScopes", []() {
        comb::LinearAllocator alloc{1024};
        TestStruct::construct_count = 0;
        TestStruct::destruct_count = 0;

        auto rc_outer = wax::MakeRc<TestStruct>(alloc, 10, 3.14f);
        {
            auto rc_inner = rc_outer;
            larvae::AssertEqual(rc_outer.GetRefCount(), 2u);
        }
        larvae::AssertEqual(rc_outer.GetRefCount(), 1u);
        larvae::AssertEqual(TestStruct::destruct_count, 0);  // Still alive

        rc_outer.Reset();
        larvae::AssertEqual(TestStruct::destruct_count, 1);
    });

    // =============================================================================
    // Comparison
    // =============================================================================

    auto test20 = larvae::RegisterTest("WaxRc", "CompareEqual", []() {
        comb::LinearAllocator alloc{1024};

        auto rc1 = wax::MakeRc<int>(alloc, 42);
        auto rc2 = rc1;

        larvae::AssertTrue(rc1 == rc2);
    });

    auto test21 = larvae::RegisterTest("WaxRc", "CompareDifferent", []() {
        comb::LinearAllocator alloc{1024};

        auto rc1 = wax::MakeRc<int>(alloc, 42);
        auto rc2 = wax::MakeRc<int>(alloc, 42);

        larvae::AssertTrue(rc1 != rc2);  // Different objects
    });

    auto test22 = larvae::RegisterTest("WaxRc", "CompareWithNullptr", []() {
        comb::LinearAllocator alloc{1024};

        auto rc1 = wax::MakeRc<int>(alloc, 42);
        wax::Rc<int, comb::LinearAllocator> rc2;

        larvae::AssertFalse(rc1 == nullptr);
        larvae::AssertTrue(rc1 != nullptr);

        larvae::AssertTrue(rc2 == nullptr);
        larvae::AssertFalse(rc2 != nullptr);
    });

    // =============================================================================
    // Stress Test
    // =============================================================================

    auto test23 = larvae::RegisterTest("WaxRc", "ManyReferences", []() {
        comb::LinearAllocator alloc{1024};

        auto rc = wax::MakeRc<int>(alloc, 42);
        wax::Rc<int, comb::LinearAllocator> refs[100];

        for (int i = 0; i < 100; ++i) {
            refs[i] = rc;
        }

        larvae::AssertEqual(rc.GetRefCount(), 101u);  // 1 original + 100 copies

        for (int i = 0; i < 100; ++i) {
            refs[i].Reset();
        }

        larvae::AssertEqual(rc.GetRefCount(), 1u);
    });
}
