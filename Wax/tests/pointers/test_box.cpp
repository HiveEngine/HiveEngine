#include <larvae/larvae.h>
#include <wax/pointers/box.h>
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

    auto test1 = larvae::RegisterTest("WaxBox", "DefaultConstructor", []() {
        wax::Box<int, comb::LinearAllocator> box;

        larvae::AssertTrue(box.IsNull());
        larvae::AssertFalse(box.IsValid());
    });

    auto test2 = larvae::RegisterTest("WaxBox", "MakeBox", []() {
        comb::LinearAllocator alloc{1024};

        auto box = wax::MakeBox<int>(alloc, 42);

        larvae::AssertTrue(box.IsValid());
        larvae::AssertEqual(*box, 42);
    });

    auto test3 = larvae::RegisterTest("WaxBox", "MakeBoxWithStruct", []() {
        comb::LinearAllocator alloc{1024};
        TestStruct::construct_count = 0;
        TestStruct::destruct_count = 0;

        {
            auto box = wax::MakeBox<TestStruct>(alloc, 10, 3.14f);

            larvae::AssertEqual(TestStruct::construct_count, 1);
            larvae::AssertEqual(box->value, 10);
            larvae::AssertEqual(box->data, 3.14f);
        }

        larvae::AssertEqual(TestStruct::destruct_count, 1);
    });

    // =============================================================================
    // Move Semantics
    // =============================================================================

    auto test4 = larvae::RegisterTest("WaxBox", "MoveConstructor", []() {
        comb::LinearAllocator alloc{1024};

        auto box1 = wax::MakeBox<int>(alloc, 42);
        auto box2 = std::move(box1);

        larvae::AssertTrue(box1.IsNull());
        larvae::AssertTrue(box2.IsValid());
        larvae::AssertEqual(*box2, 42);
    });

    auto test5 = larvae::RegisterTest("WaxBox", "MoveAssignment", []() {
        comb::LinearAllocator alloc{1024};

        auto box1 = wax::MakeBox<int>(alloc, 42);
        auto box2 = wax::MakeBox<int>(alloc, 99);

        box2 = std::move(box1);

        larvae::AssertTrue(box1.IsNull());
        larvae::AssertEqual(*box2, 42);
    });

    // =============================================================================
    // Dereference
    // =============================================================================

    auto test6 = larvae::RegisterTest("WaxBox", "DereferenceOperator", []() {
        comb::LinearAllocator alloc{1024};

        auto box = wax::MakeBox<int>(alloc, 42);

        larvae::AssertEqual(*box, 42);

        *box = 99;
        larvae::AssertEqual(*box, 99);
    });

    auto test7 = larvae::RegisterTest("WaxBox", "ArrowOperator", []() {
        comb::LinearAllocator alloc{1024};

        auto box = wax::MakeBox<TestStruct>(alloc, 10, 3.14f);

        larvae::AssertEqual(box->value, 10);

        box->value = 20;
        larvae::AssertEqual(box->value, 20);
    });

    auto test8 = larvae::RegisterTest("WaxBox", "Get", []() {
        comb::LinearAllocator alloc{1024};

        auto box = wax::MakeBox<int>(alloc, 42);

        int* ptr = box.Get();
        larvae::AssertNotNull(ptr);
        larvae::AssertEqual(*ptr, 42);
    });

    // =============================================================================
    // Bool Conversion
    // =============================================================================

    auto test9 = larvae::RegisterTest("WaxBox", "BoolConversionValid", []() {
        comb::LinearAllocator alloc{1024};

        auto box = wax::MakeBox<int>(alloc, 42);

        if (box) {
            larvae::AssertEqual(*box, 42);
        } else {
            larvae::AssertTrue(false); // Box should be valid
        }
    });

    auto test10 = larvae::RegisterTest("WaxBox", "BoolConversionNull", []() {
        wax::Box<int, comb::LinearAllocator> box;

        if (box) {
            larvae::AssertTrue(false); // Box should be null
        } else {
            larvae::AssertTrue(true);
        }
    });

    // =============================================================================
    // Release and Reset
    // =============================================================================

    auto test11 = larvae::RegisterTest("WaxBox", "Release", []() {
        comb::LinearAllocator alloc{1024};
        TestStruct::destruct_count = 0;

        auto box = wax::MakeBox<TestStruct>(alloc, 10, 3.14f);
        TestStruct* raw = box.Release();

        larvae::AssertTrue(box.IsNull());
        larvae::AssertNotNull(raw);
        larvae::AssertEqual(raw->value, 10);
        larvae::AssertEqual(TestStruct::destruct_count, 0);  // Not destroyed yet

        // Manual cleanup
        comb::Delete(alloc, raw);
    });

    auto test12 = larvae::RegisterTest("WaxBox", "Reset", []() {
        comb::LinearAllocator alloc{1024};
        TestStruct::destruct_count = 0;

        auto box = wax::MakeBox<TestStruct>(alloc, 10, 3.14f);
        box.Reset();

        larvae::AssertTrue(box.IsNull());
        larvae::AssertEqual(TestStruct::destruct_count, 1);
    });

    auto test13 = larvae::RegisterTest("WaxBox", "ResetWithNewPointer", []() {
        comb::LinearAllocator alloc{1024};

        auto box = wax::MakeBox<int>(alloc, 42);
        int* new_ptr = comb::New<int>(alloc, 99);

        box.Reset(alloc, new_ptr);

        larvae::AssertTrue(box.IsValid());
        larvae::AssertEqual(*box, 99);
    });

    // =============================================================================
    // RAII Lifetime
    // =============================================================================

    auto test14 = larvae::RegisterTest("WaxBox", "AutomaticDestruction", []() {
        comb::LinearAllocator alloc{1024};
        TestStruct::construct_count = 0;
        TestStruct::destruct_count = 0;

        {
            auto box = wax::MakeBox<TestStruct>(alloc, 10, 3.14f);
            larvae::AssertEqual(TestStruct::construct_count, 1);
            larvae::AssertEqual(TestStruct::destruct_count, 0);
        }

        // Box destroyed, object should be destroyed too
        larvae::AssertEqual(TestStruct::destruct_count, 1);
    });

    auto test15 = larvae::RegisterTest("WaxBox", "MultipleBoxes", []() {
        comb::LinearAllocator alloc{1024};
        TestStruct::construct_count = 0;
        TestStruct::destruct_count = 0;

        {
            auto box1 = wax::MakeBox<TestStruct>(alloc, 1, 1.0f);
            auto box2 = wax::MakeBox<TestStruct>(alloc, 2, 2.0f);
            auto box3 = wax::MakeBox<TestStruct>(alloc, 3, 3.0f);

            larvae::AssertEqual(TestStruct::construct_count, 3);
        }

        larvae::AssertEqual(TestStruct::destruct_count, 3);
    });

    // =============================================================================
    // Comparison
    // =============================================================================

    auto test16 = larvae::RegisterTest("WaxBox", "CompareEqual", []() {
        comb::LinearAllocator alloc{1024};

        auto box1 = wax::MakeBox<int>(alloc, 42);
        auto box2 = wax::MakeBox<int>(alloc, 42);

        larvae::AssertTrue(box1 != box2);  // Different objects
    });

    auto test17 = larvae::RegisterTest("WaxBox", "CompareWithNullptr", []() {
        comb::LinearAllocator alloc{1024};

        auto box1 = wax::MakeBox<int>(alloc, 42);
        wax::Box<int, comb::LinearAllocator> box2;

        larvae::AssertFalse(box1 == nullptr);
        larvae::AssertTrue(box1 != nullptr);

        larvae::AssertTrue(box2 == nullptr);
        larvae::AssertFalse(box2 != nullptr);
    });
}
