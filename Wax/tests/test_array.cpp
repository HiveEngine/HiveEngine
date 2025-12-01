#include <larvae/larvae.h>
#include <wax/containers/array.h>

namespace {
    // =============================================================================
    // Basic Construction and Access
    // =============================================================================

    auto test1 = larvae::RegisterTest("WaxArray", "AggregateInitialization", []() {
        wax::Array<int, 4> arr = {1, 2, 3, 4};

        larvae::AssertEqual(arr.Size(), 4u);
        larvae::AssertEqual(arr[0], 1);
        larvae::AssertEqual(arr[1], 2);
        larvae::AssertEqual(arr[2], 3);
        larvae::AssertEqual(arr[3], 4);
    });

    auto test2 = larvae::RegisterTest("WaxArray", "DefaultInitialization", []() {
        wax::Array<int, 3> arr = {};

        larvae::AssertEqual(arr.Size(), 3u);
        larvae::AssertFalse(arr.IsEmpty());
    });

    auto test3 = larvae::RegisterTest("WaxArray", "IndexOperatorReadAccess", []() {
        wax::Array<int, 3> arr = {10, 20, 30};

        larvae::AssertEqual(arr[0], 10);
        larvae::AssertEqual(arr[1], 20);
        larvae::AssertEqual(arr[2], 30);
    });

    auto test4 = larvae::RegisterTest("WaxArray", "IndexOperatorWriteAccess", []() {
        wax::Array<int, 3> arr = {0, 0, 0};

        arr[0] = 10;
        arr[1] = 20;
        arr[2] = 30;

        larvae::AssertEqual(arr[0], 10);
        larvae::AssertEqual(arr[1], 20);
        larvae::AssertEqual(arr[2], 30);
    });

    auto test5 = larvae::RegisterTest("WaxArray", "AtMethodBoundsChecked", []() {
        wax::Array<int, 3> arr = {1, 2, 3};

        larvae::AssertEqual(arr.At(0), 1);
        larvae::AssertEqual(arr.At(1), 2);
        larvae::AssertEqual(arr.At(2), 3);

        arr.At(1) = 42;
        larvae::AssertEqual(arr.At(1), 42);
    });

    // =============================================================================
    // Front and Back Access
    // =============================================================================

    auto test6 = larvae::RegisterTest("WaxArray", "FrontReturnsFirstElement", []() {
        wax::Array<int, 4> arr = {5, 10, 15, 20};

        larvae::AssertEqual(arr.Front(), 5);

        arr.Front() = 100;
        larvae::AssertEqual(arr.Front(), 100);
        larvae::AssertEqual(arr[0], 100);
    });

    auto test7 = larvae::RegisterTest("WaxArray", "BackReturnsLastElement", []() {
        wax::Array<int, 4> arr = {5, 10, 15, 20};

        larvae::AssertEqual(arr.Back(), 20);

        arr.Back() = 200;
        larvae::AssertEqual(arr.Back(), 200);
        larvae::AssertEqual(arr[3], 200);
    });

    auto test8 = larvae::RegisterTest("WaxArray", "FrontAndBackModifyCorrectElements", []() {
        wax::Array<int, 5> arr = {1, 2, 3, 4, 5};

        arr.Front() = 10;
        arr.Back() = 50;

        larvae::AssertEqual(arr[0], 10);
        larvae::AssertEqual(arr[1], 2);
        larvae::AssertEqual(arr[2], 3);
        larvae::AssertEqual(arr[3], 4);
        larvae::AssertEqual(arr[4], 50);
    });

    // =============================================================================
    // Data Pointer Access
    // =============================================================================

    auto test9 = larvae::RegisterTest("WaxArray", "DataReturnsPointerToFirstElement", []() {
        wax::Array<int, 3> arr = {7, 8, 9};

        int* data = arr.Data();

        larvae::AssertNotNull(data);
        larvae::AssertEqual(data[0], 7);
        larvae::AssertEqual(data[1], 8);
        larvae::AssertEqual(data[2], 9);
    });

    auto test10 = larvae::RegisterTest("WaxArray", "DataPointerAllowsModification", []() {
        wax::Array<int, 3> arr = {1, 2, 3};

        int* data = arr.Data();
        data[1] = 42;

        larvae::AssertEqual(arr[1], 42);
    });

    auto test11 = larvae::RegisterTest("WaxArray", "DataPointerIsContiguous", []() {
        wax::Array<int, 5> arr = {10, 20, 30, 40, 50};

        int* data = arr.Data();

        for (size_t i = 0; i < arr.Size(); ++i)
        {
            larvae::AssertEqual(data[i], arr[i]);
        }
    });

    // =============================================================================
    // Iterators and Range-For
    // =============================================================================

    auto test12 = larvae::RegisterTest("WaxArray", "BeginEndIteratorRange", []() {
        wax::Array<int, 4> arr = {10, 20, 30, 40};

        auto it = arr.begin();
        larvae::AssertEqual(*it, 10);

        ++it;
        larvae::AssertEqual(*it, 20);

        ++it;
        larvae::AssertEqual(*it, 30);

        ++it;
        larvae::AssertEqual(*it, 40);

        ++it;
        larvae::AssertTrue(it == arr.end());
    });

    auto test13 = larvae::RegisterTest("WaxArray", "RangeForLoopSum", []() {
        wax::Array<int, 5> arr = {1, 2, 3, 4, 5};

        int sum = 0;
        for (int val : arr)
        {
            sum += val;
        }

        larvae::AssertEqual(sum, 15);
    });

    auto test14 = larvae::RegisterTest("WaxArray", "RangeForLoopModification", []() {
        wax::Array<int, 3> arr = {1, 2, 3};

        for (int& val : arr)
        {
            val *= 2;
        }

        larvae::AssertEqual(arr[0], 2);
        larvae::AssertEqual(arr[1], 4);
        larvae::AssertEqual(arr[2], 6);
    });

    auto test15 = larvae::RegisterTest("WaxArray", "ConstIteratorReadOnly", []() {
        const wax::Array<int, 3> arr = {10, 20, 30};

        int sum = 0;
        for (int val : arr)
        {
            sum += val;
        }

        larvae::AssertEqual(sum, 60);
    });

    // =============================================================================
    // Fill Operation
    // =============================================================================

    auto test16 = larvae::RegisterTest("WaxArray", "FillSetsAllElements", []() {
        wax::Array<int, 5> arr = {};
        arr.Fill(42);

        for (size_t i = 0; i < arr.Size(); ++i)
        {
            larvae::AssertEqual(arr[i], 42);
        }
    });

    auto test17 = larvae::RegisterTest("WaxArray", "FillOverwritesExistingValues", []() {
        wax::Array<int, 4> arr = {1, 2, 3, 4};

        arr.Fill(99);

        larvae::AssertEqual(arr[0], 99);
        larvae::AssertEqual(arr[1], 99);
        larvae::AssertEqual(arr[2], 99);
        larvae::AssertEqual(arr[3], 99);
    });

    // =============================================================================
    // Size and Emptiness
    // =============================================================================

    auto test18 = larvae::RegisterTest("WaxArray", "SizeReturnsCorrectValue", []() {
        wax::Array<int, 1> arr1 = {0};
        wax::Array<int, 10> arr10 = {};
        wax::Array<int, 100> arr100 = {};

        larvae::AssertEqual(arr1.Size(), 1u);
        larvae::AssertEqual(arr10.Size(), 10u);
        larvae::AssertEqual(arr100.Size(), 100u);
    });

    auto test19 = larvae::RegisterTest("WaxArray", "IsEmptyAlwaysFalse", []() {
        wax::Array<int, 1> arr1 = {0};
        wax::Array<int, 100> arr100 = {};

        larvae::AssertFalse(arr1.IsEmpty());
        larvae::AssertFalse(arr100.IsEmpty());
    });

    // =============================================================================
    // Const Correctness
    // =============================================================================

    auto test20 = larvae::RegisterTest("WaxArray", "ConstArrayReadAccess", []() {
        const wax::Array<int, 3> arr = {1, 2, 3};

        larvae::AssertEqual(arr[0], 1);
        larvae::AssertEqual(arr.At(1), 2);
        larvae::AssertEqual(arr.Front(), 1);
        larvae::AssertEqual(arr.Back(), 3);
        larvae::AssertEqual(arr.Size(), 3u);
    });

    auto test21 = larvae::RegisterTest("WaxArray", "ConstArrayDataPointer", []() {
        const wax::Array<int, 3> arr = {10, 20, 30};

        const int* data = arr.Data();

        larvae::AssertNotNull(data);
        larvae::AssertEqual(data[0], 10);
        larvae::AssertEqual(data[1], 20);
        larvae::AssertEqual(data[2], 30);
    });

    // =============================================================================
    // Different Types
    // =============================================================================

    auto test22 = larvae::RegisterTest("WaxArray", "FloatArray", []() {
        wax::Array<float, 3> arr = {1.5f, 2.5f, 3.5f};

        larvae::AssertEqual(arr[0], 1.5f);
        larvae::AssertEqual(arr[1], 2.5f);
        larvae::AssertEqual(arr[2], 3.5f);
    });

    auto test23 = larvae::RegisterTest("WaxArray", "StructArray", []() {
        struct Point
        {
            int x;
            int y;
        };

        wax::Array<Point, 2> arr = {{{10, 20}, {30, 40}}};

        larvae::AssertEqual(arr[0].x, 10);
        larvae::AssertEqual(arr[0].y, 20);
        larvae::AssertEqual(arr[1].x, 30);
        larvae::AssertEqual(arr[1].y, 40);
    });

    auto test24 = larvae::RegisterTest("WaxArray", "PointerArray", []() {
        int a = 1, b = 2, c = 3;
        wax::Array<int*, 3> arr = {&a, &b, &c};

        larvae::AssertEqual(*arr[0], 1);
        larvae::AssertEqual(*arr[1], 2);
        larvae::AssertEqual(*arr[2], 3);

        *arr[1] = 42;
        larvae::AssertEqual(b, 42);
    });

    // =============================================================================
    // Edge Cases
    // =============================================================================

    auto test25 = larvae::RegisterTest("WaxArray", "SingleElementArray", []() {
        wax::Array<int, 1> arr = {42};

        larvae::AssertEqual(arr.Size(), 1u);
        larvae::AssertEqual(arr.Front(), 42);
        larvae::AssertEqual(arr.Back(), 42);
        larvae::AssertEqual(arr[0], 42);
    });

    auto test26 = larvae::RegisterTest("WaxArray", "LargeArray", []() {
        wax::Array<int, 1000> arr = {};
        arr.Fill(123);

        for (size_t i = 0; i < arr.Size(); ++i)
        {
            larvae::AssertEqual(arr[i], 123);
        }

        larvae::AssertEqual(arr.Size(), 1000u);
    });
}
