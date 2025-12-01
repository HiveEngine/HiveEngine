#include <larvae/larvae.h>
#include <wax/containers/span.h>
#include <wax/containers/array.h>

namespace {
    // =============================================================================
    // Construction and Basic Access
    // =============================================================================

    auto test1 = larvae::RegisterTest("WaxSpan", "DefaultConstructor", []() {
        wax::Span<int> span;

        larvae::AssertEqual(span.Size(), 0u);
        larvae::AssertTrue(span.IsEmpty());
        larvae::AssertNull(span.Data());
    });

    auto test2 = larvae::RegisterTest("WaxSpan", "PointerSizeConstructor", []() {
        int data[] = {1, 2, 3, 4, 5};
        wax::Span<int> span{data, 5};

        larvae::AssertEqual(span.Size(), 5u);
        larvae::AssertFalse(span.IsEmpty());
        larvae::AssertEqual(span[0], 1);
        larvae::AssertEqual(span[4], 5);
    });

    auto test3 = larvae::RegisterTest("WaxSpan", "CArrayConstructor", []() {
        int data[] = {10, 20, 30};
        wax::Span<int> span{data};

        larvae::AssertEqual(span.Size(), 3u);
        larvae::AssertEqual(span[0], 10);
        larvae::AssertEqual(span[1], 20);
        larvae::AssertEqual(span[2], 30);
    });

    auto test4 = larvae::RegisterTest("WaxSpan", "WaxArrayConstructor", []() {
        wax::Array<int, 4> arr = {1, 2, 3, 4};
        wax::Span<int> span{arr};

        larvae::AssertEqual(span.Size(), 4u);
        larvae::AssertEqual(span[0], 1);
        larvae::AssertEqual(span[3], 4);
    });

    auto test5 = larvae::RegisterTest("WaxSpan", "ConstWaxArrayConstructor", []() {
        const wax::Array<int, 3> arr = {5, 6, 7};
        wax::Span<const int> span{arr};

        larvae::AssertEqual(span.Size(), 3u);
        larvae::AssertEqual(span[0], 5);
        larvae::AssertEqual(span[2], 7);
    });

    auto test6 = larvae::RegisterTest("WaxSpan", "IteratorConstructor", []() {
        int data[] = {1, 2, 3, 4};
        wax::Span<int> span{data, data + 4};

        larvae::AssertEqual(span.Size(), 4u);
        larvae::AssertEqual(span[0], 1);
        larvae::AssertEqual(span[3], 4);
    });

    // =============================================================================
    // Element Access
    // =============================================================================

    auto test7 = larvae::RegisterTest("WaxSpan", "IndexOperatorRead", []() {
        int data[] = {10, 20, 30, 40};
        wax::Span<int> span{data};

        larvae::AssertEqual(span[0], 10);
        larvae::AssertEqual(span[1], 20);
        larvae::AssertEqual(span[2], 30);
        larvae::AssertEqual(span[3], 40);
    });

    auto test8 = larvae::RegisterTest("WaxSpan", "IndexOperatorWrite", []() {
        int data[] = {1, 2, 3};
        wax::Span<int> span{data};

        span[0] = 100;
        span[1] = 200;
        span[2] = 300;

        larvae::AssertEqual(data[0], 100);
        larvae::AssertEqual(data[1], 200);
        larvae::AssertEqual(data[2], 300);
    });

    auto test9 = larvae::RegisterTest("WaxSpan", "AtMethod", []() {
        int data[] = {1, 2, 3};
        wax::Span<int> span{data};

        larvae::AssertEqual(span.At(0), 1);
        larvae::AssertEqual(span.At(1), 2);
        larvae::AssertEqual(span.At(2), 3);

        span.At(1) = 42;
        larvae::AssertEqual(span.At(1), 42);
    });

    auto test10 = larvae::RegisterTest("WaxSpan", "FrontBackAccess", []() {
        int data[] = {5, 10, 15, 20};
        wax::Span<int> span{data};

        larvae::AssertEqual(span.Front(), 5);
        larvae::AssertEqual(span.Back(), 20);

        span.Front() = 100;
        span.Back() = 200;

        larvae::AssertEqual(data[0], 100);
        larvae::AssertEqual(data[3], 200);
    });

    auto test11 = larvae::RegisterTest("WaxSpan", "DataPointer", []() {
        int data[] = {1, 2, 3};
        wax::Span<int> span{data};

        int* ptr = span.Data();
        larvae::AssertEqual(ptr, data);
        larvae::AssertEqual(ptr[0], 1);
        larvae::AssertEqual(ptr[2], 3);
    });

    // =============================================================================
    // Size Information
    // =============================================================================

    auto test12 = larvae::RegisterTest("WaxSpan", "SizeAndSizeBytes", []() {
        int data[] = {1, 2, 3, 4, 5};
        wax::Span<int> span{data};

        larvae::AssertEqual(span.Size(), 5u);
        larvae::AssertEqual(span.SizeBytes(), 5u * sizeof(int));
    });

    auto test13 = larvae::RegisterTest("WaxSpan", "IsEmpty", []() {
        wax::Span<int> empty;
        larvae::AssertTrue(empty.IsEmpty());

        int data[] = {1};
        wax::Span<int> nonEmpty{data};
        larvae::AssertFalse(nonEmpty.IsEmpty());
    });

    // =============================================================================
    // Iterators and Range-For
    // =============================================================================

    auto test14 = larvae::RegisterTest("WaxSpan", "BeginEndIterators", []() {
        int data[] = {10, 20, 30, 40};
        wax::Span<int> span{data};

        auto it = span.begin();
        larvae::AssertEqual(*it, 10);

        ++it;
        larvae::AssertEqual(*it, 20);

        ++it;
        larvae::AssertEqual(*it, 30);

        ++it;
        larvae::AssertEqual(*it, 40);

        ++it;
        larvae::AssertTrue(it == span.end());
    });

    auto test15 = larvae::RegisterTest("WaxSpan", "RangeForLoop", []() {
        int data[] = {1, 2, 3, 4, 5};
        wax::Span<int> span{data};

        int sum = 0;
        for (int val : span)
        {
            sum += val;
        }

        larvae::AssertEqual(sum, 15);
    });

    auto test16 = larvae::RegisterTest("WaxSpan", "RangeForLoopModification", []() {
        int data[] = {1, 2, 3};
        wax::Span<int> span{data};

        for (int& val : span)
        {
            val *= 2;
        }

        larvae::AssertEqual(data[0], 2);
        larvae::AssertEqual(data[1], 4);
        larvae::AssertEqual(data[2], 6);
    });

    // =============================================================================
    // Subspan Operations
    // =============================================================================

    auto test17 = larvae::RegisterTest("WaxSpan", "FirstSubspan", []() {
        int data[] = {1, 2, 3, 4, 5};
        wax::Span<int> span{data};

        auto first3 = span.First(3);

        larvae::AssertEqual(first3.Size(), 3u);
        larvae::AssertEqual(first3[0], 1);
        larvae::AssertEqual(first3[1], 2);
        larvae::AssertEqual(first3[2], 3);
    });

    auto test18 = larvae::RegisterTest("WaxSpan", "LastSubspan", []() {
        int data[] = {1, 2, 3, 4, 5};
        wax::Span<int> span{data};

        auto last2 = span.Last(2);

        larvae::AssertEqual(last2.Size(), 2u);
        larvae::AssertEqual(last2[0], 4);
        larvae::AssertEqual(last2[1], 5);
    });

    auto test19 = larvae::RegisterTest("WaxSpan", "SubspanWithOffsetAndCount", []() {
        int data[] = {1, 2, 3, 4, 5, 6};
        wax::Span<int> span{data};

        auto sub = span.Subspan(2, 3);

        larvae::AssertEqual(sub.Size(), 3u);
        larvae::AssertEqual(sub[0], 3);
        larvae::AssertEqual(sub[1], 4);
        larvae::AssertEqual(sub[2], 5);
    });

    auto test20 = larvae::RegisterTest("WaxSpan", "SubspanWithOffsetOnly", []() {
        int data[] = {1, 2, 3, 4, 5};
        wax::Span<int> span{data};

        auto sub = span.Subspan(2);

        larvae::AssertEqual(sub.Size(), 3u);
        larvae::AssertEqual(sub[0], 3);
        larvae::AssertEqual(sub[1], 4);
        larvae::AssertEqual(sub[2], 5);
    });

    // =============================================================================
    // Copy Semantics
    // =============================================================================

    auto test21 = larvae::RegisterTest("WaxSpan", "CopyConstructor", []() {
        int data[] = {1, 2, 3};
        wax::Span<int> span1{data};
        wax::Span<int> span2 = span1;

        larvae::AssertEqual(span2.Size(), 3u);
        larvae::AssertEqual(span2[0], 1);
        larvae::AssertEqual(span2.Data(), span1.Data());
    });

    auto test22 = larvae::RegisterTest("WaxSpan", "CopyAssignment", []() {
        int data1[] = {1, 2, 3};
        int data2[] = {4, 5};

        wax::Span<int> span1{data1};
        wax::Span<int> span2{data2};

        span2 = span1;

        larvae::AssertEqual(span2.Size(), 3u);
        larvae::AssertEqual(span2[0], 1);
        larvae::AssertEqual(span2.Data(), data1);
    });

    // =============================================================================
    // Const Correctness
    // =============================================================================

    auto test23 = larvae::RegisterTest("WaxSpan", "ConstSpanReadOnly", []() {
        int data[] = {1, 2, 3};
        wax::Span<const int> span{data};

        larvae::AssertEqual(span.Size(), 3u);
        larvae::AssertEqual(span[0], 1);
        larvae::AssertEqual(span[2], 3);

        int sum = 0;
        for (int val : span)
        {
            sum += val;
        }
        larvae::AssertEqual(sum, 6);
    });

    auto test24 = larvae::RegisterTest("WaxSpan", "ConstSpanFromConstArray", []() {
        const int data[] = {10, 20, 30};
        wax::Span<const int> span{data};

        larvae::AssertEqual(span.Size(), 3u);
        larvae::AssertEqual(span.Front(), 10);
        larvae::AssertEqual(span.Back(), 30);
    });

    // =============================================================================
    // Different Types
    // =============================================================================

    auto test25 = larvae::RegisterTest("WaxSpan", "FloatSpan", []() {
        float data[] = {1.5f, 2.5f, 3.5f};
        wax::Span<float> span{data};

        larvae::AssertEqual(span.Size(), 3u);
        larvae::AssertEqual(span[0], 1.5f);
        larvae::AssertEqual(span[2], 3.5f);
    });

    auto test26 = larvae::RegisterTest("WaxSpan", "StructSpan", []() {
        struct Point { int x, y; };
        Point data[] = {{1, 2}, {3, 4}, {5, 6}};
        wax::Span<Point> span{data};

        larvae::AssertEqual(span.Size(), 3u);
        larvae::AssertEqual(span[0].x, 1);
        larvae::AssertEqual(span[1].y, 4);
        larvae::AssertEqual(span[2].x, 5);
    });

    // =============================================================================
    // Edge Cases
    // =============================================================================

    auto test27 = larvae::RegisterTest("WaxSpan", "SingleElementSpan", []() {
        int data[] = {42};
        wax::Span<int> span{data};

        larvae::AssertEqual(span.Size(), 1u);
        larvae::AssertEqual(span.Front(), 42);
        larvae::AssertEqual(span.Back(), 42);
        larvae::AssertEqual(span[0], 42);
    });

    auto test28 = larvae::RegisterTest("WaxSpan", "LargeSpan", []() {
        int data[1000];
        for (int i = 0; i < 1000; ++i)
        {
            data[i] = i;
        }

        wax::Span<int> span{data};

        larvae::AssertEqual(span.Size(), 1000u);
        larvae::AssertEqual(span[0], 0);
        larvae::AssertEqual(span[999], 999);
    });

    auto test29 = larvae::RegisterTest("WaxSpan", "ModifyThroughSpan", []() {
        int data[] = {1, 2, 3, 4};
        wax::Span<int> span{data};

        for (size_t i = 0; i < span.Size(); ++i)
        {
            span[i] *= 10;
        }

        larvae::AssertEqual(data[0], 10);
        larvae::AssertEqual(data[1], 20);
        larvae::AssertEqual(data[2], 30);
        larvae::AssertEqual(data[3], 40);
    });
}
