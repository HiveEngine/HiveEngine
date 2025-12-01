#include <larvae/larvae.h>
#include <wax/pointers/ref.h>

namespace {
    struct TestStruct {
        int value;
        float data;

        TestStruct(int v = 0, float d = 0.0f) : value{v}, data{d} {}
    };

    // =============================================================================
    // Construction
    // =============================================================================

    auto test1 = larvae::RegisterTest("WaxRef", "ConstructFromReference", []() {
        int x = 42;
        wax::Ref<int> ref{x};

        larvae::AssertEqual(*ref, 42);
        larvae::AssertEqual(ref.Get(), &x);
    });

    auto test2 = larvae::RegisterTest("WaxRef", "ConstructFromPointer", []() {
        int x = 99;
        wax::Ref<int> ref{&x};

        larvae::AssertEqual(*ref, 99);
        larvae::AssertEqual(ref.Get(), &x);
    });

    auto test3 = larvae::RegisterTest("WaxRef", "ConstructFromStruct", []() {
        TestStruct obj{10, 3.14f};
        wax::Ref<TestStruct> ref{obj};

        larvae::AssertEqual(ref->value, 10);
        larvae::AssertEqual(ref->data, 3.14f);
    });

    auto test4 = larvae::RegisterTest("WaxRef", "DeductionGuide", []() {
        int x = 123;
        wax::Ref ref{x};  // Type deduced as Ref<int>

        larvae::AssertEqual(*ref, 123);
    });

    // =============================================================================
    // Copy and Assignment
    // =============================================================================

    auto test5 = larvae::RegisterTest("WaxRef", "CopyConstructor", []() {
        int x = 42;
        wax::Ref<int> ref1{x};
        wax::Ref<int> ref2{ref1};

        larvae::AssertEqual(*ref2, 42);
        larvae::AssertEqual(ref1.Get(), ref2.Get());
    });

    auto test6 = larvae::RegisterTest("WaxRef", "CopyAssignment", []() {
        int x = 42, y = 99;
        wax::Ref<int> ref1{x};
        wax::Ref<int> ref2{y};

        ref2 = ref1;

        larvae::AssertEqual(*ref2, 42);
        larvae::AssertEqual(ref1.Get(), ref2.Get());
    });

    // =============================================================================
    // Dereference
    // =============================================================================

    auto test7 = larvae::RegisterTest("WaxRef", "DereferenceOperator", []() {
        int x = 42;
        wax::Ref<int> ref{x};

        larvae::AssertEqual(*ref, 42);

        *ref = 99;
        larvae::AssertEqual(x, 99);
    });

    auto test8 = larvae::RegisterTest("WaxRef", "ArrowOperator", []() {
        TestStruct obj{10, 3.14f};
        wax::Ref<TestStruct> ref{obj};

        larvae::AssertEqual(ref->value, 10);

        ref->value = 20;
        larvae::AssertEqual(obj.value, 20);
    });

    auto test9 = larvae::RegisterTest("WaxRef", "Get", []() {
        int x = 42;
        wax::Ref<int> ref{x};

        int* ptr = ref.Get();
        larvae::AssertEqual(ptr, &x);
        larvae::AssertEqual(*ptr, 42);
    });

    auto test10 = larvae::RegisterTest("WaxRef", "ImplicitConversionToReference", []() {
        int x = 42;
        wax::Ref<int> ref{x};

        int& y = ref;  // Implicit conversion
        larvae::AssertEqual(y, 42);
        larvae::AssertEqual(&y, &x);
    });

    // =============================================================================
    // Rebind
    // =============================================================================

    auto test11 = larvae::RegisterTest("WaxRef", "RebindToReference", []() {
        int x = 42, y = 99;
        wax::Ref<int> ref{x};

        larvae::AssertEqual(*ref, 42);

        ref.Rebind(y);
        larvae::AssertEqual(*ref, 99);
        larvae::AssertEqual(ref.Get(), &y);
    });

    auto test12 = larvae::RegisterTest("WaxRef", "RebindToPointer", []() {
        int x = 42, y = 99;
        wax::Ref<int> ref{x};

        ref.Rebind(&y);
        larvae::AssertEqual(*ref, 99);
    });

    // =============================================================================
    // Comparison
    // =============================================================================

    auto test13 = larvae::RegisterTest("WaxRef", "EqualityOperator", []() {
        int x = 42;
        wax::Ref<int> ref1{x};
        wax::Ref<int> ref2{x};

        larvae::AssertTrue(ref1 == ref2);
    });

    auto test14 = larvae::RegisterTest("WaxRef", "InequalityOperator", []() {
        int x = 42, y = 99;
        wax::Ref<int> ref1{x};
        wax::Ref<int> ref2{y};

        larvae::AssertTrue(ref1 != ref2);
    });

    auto test15 = larvae::RegisterTest("WaxRef", "ComparisonOperators", []() {
        int arr[3] = {1, 2, 3};
        wax::Ref<int> ref1{arr[0]};
        wax::Ref<int> ref2{arr[1]};
        wax::Ref<int> ref3{arr[2]};

        larvae::AssertTrue(ref1 < ref2);
        larvae::AssertTrue(ref1 <= ref2);
        larvae::AssertTrue(ref3 > ref1);
        larvae::AssertTrue(ref3 >= ref1);
    });

    // =============================================================================
    // Const Correctness
    // =============================================================================

    auto test16 = larvae::RegisterTest("WaxRef", "ConstRef", []() {
        int x = 42;
        wax::Ref<const int> ref{x};

        larvae::AssertEqual(*ref, 42);
        // *ref = 99;  // Should not compile (const)
    });

    auto test17 = larvae::RegisterTest("WaxRef", "ConstRefArrow", []() {
        TestStruct obj{10, 3.14f};
        wax::Ref<const TestStruct> ref{obj};

        larvae::AssertEqual(ref->value, 10);
        // ref->value = 20;  // Should not compile (const)
    });
}
