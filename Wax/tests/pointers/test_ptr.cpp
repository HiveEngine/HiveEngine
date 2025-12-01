#include <larvae/larvae.h>
#include <wax/pointers/ptr.h>

namespace {
    struct TestStruct {
        int value;
        float data;

        TestStruct(int v = 0, float d = 0.0f) : value{v}, data{d} {}
    };

    // =============================================================================
    // Construction
    // =============================================================================

    auto test1 = larvae::RegisterTest("WaxPtr", "DefaultConstructor", []() {
        wax::Ptr<int> ptr;

        larvae::AssertTrue(ptr.IsNull());
        larvae::AssertFalse(ptr.IsValid());
        larvae::AssertNull(ptr.Get());
    });

    auto test2 = larvae::RegisterTest("WaxPtr", "ConstructFromNullptr", []() {
        wax::Ptr<int> ptr{nullptr};

        larvae::AssertTrue(ptr.IsNull());
        larvae::AssertEqual(ptr.Get(), nullptr);
    });

    auto test3 = larvae::RegisterTest("WaxPtr", "ConstructFromPointer", []() {
        int x = 42;
        wax::Ptr<int> ptr{&x};

        larvae::AssertFalse(ptr.IsNull());
        larvae::AssertTrue(ptr.IsValid());
        larvae::AssertEqual(*ptr, 42);
    });

    auto test4 = larvae::RegisterTest("WaxPtr", "ConstructFromReference", []() {
        int x = 99;
        wax::Ptr<int> ptr{x};

        larvae::AssertEqual(*ptr, 99);
        larvae::AssertEqual(ptr.Get(), &x);
    });

    auto test5 = larvae::RegisterTest("WaxPtr", "DeductionGuide", []() {
        int x = 123;
        wax::Ptr ptr{&x};  // Type deduced as Ptr<int>

        larvae::AssertEqual(*ptr, 123);
    });

    // =============================================================================
    // Copy and Assignment
    // =============================================================================

    auto test6 = larvae::RegisterTest("WaxPtr", "CopyConstructor", []() {
        int x = 42;
        wax::Ptr<int> ptr1{&x};
        wax::Ptr<int> ptr2{ptr1};

        larvae::AssertEqual(*ptr2, 42);
        larvae::AssertEqual(ptr1.Get(), ptr2.Get());
    });

    auto test7 = larvae::RegisterTest("WaxPtr", "CopyAssignment", []() {
        int x = 42, y = 99;
        wax::Ptr<int> ptr1{&x};
        wax::Ptr<int> ptr2{&y};

        ptr2 = ptr1;

        larvae::AssertEqual(*ptr2, 42);
        larvae::AssertEqual(ptr1.Get(), ptr2.Get());
    });

    auto test8 = larvae::RegisterTest("WaxPtr", "AssignFromNullptr", []() {
        int x = 42;
        wax::Ptr<int> ptr{&x};

        ptr = nullptr;

        larvae::AssertTrue(ptr.IsNull());
    });

    auto test9 = larvae::RegisterTest("WaxPtr", "AssignFromPointer", []() {
        int x = 42, y = 99;
        wax::Ptr<int> ptr{&x};

        ptr = &y;

        larvae::AssertEqual(*ptr, 99);
    });

    // =============================================================================
    // Dereference
    // =============================================================================

    auto test10 = larvae::RegisterTest("WaxPtr", "DereferenceOperator", []() {
        int x = 42;
        wax::Ptr<int> ptr{&x};

        larvae::AssertEqual(*ptr, 42);

        *ptr = 99;
        larvae::AssertEqual(x, 99);
    });

    auto test11 = larvae::RegisterTest("WaxPtr", "ArrowOperator", []() {
        TestStruct obj{10, 3.14f};
        wax::Ptr<TestStruct> ptr{&obj};

        larvae::AssertEqual(ptr->value, 10);

        ptr->value = 20;
        larvae::AssertEqual(obj.value, 20);
    });

    auto test12 = larvae::RegisterTest("WaxPtr", "Get", []() {
        int x = 42;
        wax::Ptr<int> ptr{&x};

        int* raw = ptr.Get();
        larvae::AssertEqual(raw, &x);
        larvae::AssertEqual(*raw, 42);
    });

    // =============================================================================
    // Bool Conversion
    // =============================================================================

    auto test13 = larvae::RegisterTest("WaxPtr", "BoolConversionTrue", []() {
        int x = 42;
        wax::Ptr<int> ptr{&x};

        if (ptr) {
            larvae::AssertEqual(*ptr, 42);
        } else {
            larvae::AssertTrue(false); // Ptr should be valid
        }
    });

    auto test14 = larvae::RegisterTest("WaxPtr", "BoolConversionFalse", []() {
        wax::Ptr<int> ptr{nullptr};

        if (ptr) {
            larvae::AssertTrue(false); // Ptr should be null
        } else {
            // Expected path
            larvae::AssertTrue(true);
        }
    });

    auto test15 = larvae::RegisterTest("WaxPtr", "IsNullIsValid", []() {
        int x = 42;
        wax::Ptr<int> ptr1{&x};
        wax::Ptr<int> ptr2{nullptr};

        larvae::AssertFalse(ptr1.IsNull());
        larvae::AssertTrue(ptr1.IsValid());

        larvae::AssertTrue(ptr2.IsNull());
        larvae::AssertFalse(ptr2.IsValid());
    });

    // =============================================================================
    // Reset and Rebind
    // =============================================================================

    auto test16 = larvae::RegisterTest("WaxPtr", "Reset", []() {
        int x = 42;
        wax::Ptr<int> ptr{&x};

        larvae::AssertTrue(ptr.IsValid());

        ptr.Reset();

        larvae::AssertTrue(ptr.IsNull());
    });

    auto test17 = larvae::RegisterTest("WaxPtr", "RebindToPointer", []() {
        int x = 42, y = 99;
        wax::Ptr<int> ptr{&x};

        ptr.Rebind(&y);

        larvae::AssertEqual(*ptr, 99);
    });

    auto test18 = larvae::RegisterTest("WaxPtr", "RebindToReference", []() {
        int x = 42, y = 99;
        wax::Ptr<int> ptr{&x};

        ptr.Rebind(y);

        larvae::AssertEqual(*ptr, 99);
    });

    // =============================================================================
    // Comparison
    // =============================================================================

    auto test19 = larvae::RegisterTest("WaxPtr", "EqualityOperator", []() {
        int x = 42;
        wax::Ptr<int> ptr1{&x};
        wax::Ptr<int> ptr2{&x};

        larvae::AssertTrue(ptr1 == ptr2);
    });

    auto test20 = larvae::RegisterTest("WaxPtr", "InequalityOperator", []() {
        int x = 42, y = 99;
        wax::Ptr<int> ptr1{&x};
        wax::Ptr<int> ptr2{&y};

        larvae::AssertTrue(ptr1 != ptr2);
    });

    auto test21 = larvae::RegisterTest("WaxPtr", "CompareWithNullptr", []() {
        int x = 42;
        wax::Ptr<int> ptr1{&x};
        wax::Ptr<int> ptr2{nullptr};

        larvae::AssertFalse(ptr1 == nullptr);
        larvae::AssertTrue(ptr1 != nullptr);

        larvae::AssertTrue(ptr2 == nullptr);
        larvae::AssertFalse(ptr2 != nullptr);

        // Reversed
        larvae::AssertFalse(nullptr == ptr1);
        larvae::AssertTrue(nullptr != ptr1);
    });

    auto test22 = larvae::RegisterTest("WaxPtr", "ComparisonOperators", []() {
        int arr[3] = {1, 2, 3};
        wax::Ptr<int> ptr1{&arr[0]};
        wax::Ptr<int> ptr2{&arr[1]};
        wax::Ptr<int> ptr3{&arr[2]};

        larvae::AssertTrue(ptr1 < ptr2);
        larvae::AssertTrue(ptr1 <= ptr2);
        larvae::AssertTrue(ptr3 > ptr1);
        larvae::AssertTrue(ptr3 >= ptr1);
    });

    // =============================================================================
    // Const Correctness
    // =============================================================================

    auto test23 = larvae::RegisterTest("WaxPtr", "ConstPtr", []() {
        int x = 42;
        wax::Ptr<const int> ptr{&x};

        larvae::AssertEqual(*ptr, 42);
        // *ptr = 99;  // Should not compile (const)
    });

    auto test24 = larvae::RegisterTest("WaxPtr", "NullConstPtr", []() {
        wax::Ptr<const int> ptr{nullptr};

        larvae::AssertTrue(ptr.IsNull());
    });
}
