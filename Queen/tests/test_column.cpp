#include <larvae/larvae.h>
#include <queen/storage/column.h>
#include <comb/linear_allocator.h>

namespace
{
    struct Position
    {
        float x, y, z;
    };

    struct NonTrivial
    {
        int value;
        static int construct_count;
        static int destruct_count;

        NonTrivial() : value{0} { ++construct_count; }
        explicit NonTrivial(int v) : value{v} { ++construct_count; }
        ~NonTrivial() { ++destruct_count; }
        NonTrivial(const NonTrivial& other) : value{other.value} { ++construct_count; }
        NonTrivial(NonTrivial&& other) noexcept : value{other.value} { other.value = 0; ++construct_count; }

        static void ResetCounts()
        {
            construct_count = 0;
            destruct_count = 0;
        }
    };

    int NonTrivial::construct_count = 0;
    int NonTrivial::destruct_count = 0;

    auto test1 = larvae::RegisterTest("QueenColumn", "PushAndGet", []() {
        comb::LinearAllocator alloc{4096};
        queen::Column<comb::LinearAllocator> column{alloc, queen::ComponentMeta::Of<Position>(), 10};

        Position pos{1.0f, 2.0f, 3.0f};
        column.PushCopy(&pos);

        larvae::AssertEqual(column.Size(), size_t{1});

        Position* result = column.Get<Position>(0);
        larvae::AssertNotNull(result);
        larvae::AssertEqual(result->x, 1.0f);
        larvae::AssertEqual(result->y, 2.0f);
        larvae::AssertEqual(result->z, 3.0f);
    });

    auto test2 = larvae::RegisterTest("QueenColumn", "PushDefault", []() {
        comb::LinearAllocator alloc{4096};
        queen::Column<comb::LinearAllocator> column{alloc, queen::ComponentMeta::Of<Position>(), 10};

        column.PushDefault();

        larvae::AssertEqual(column.Size(), size_t{1});

        Position* result = column.Get<Position>(0);
        larvae::AssertEqual(result->x, 0.0f);
        larvae::AssertEqual(result->y, 0.0f);
        larvae::AssertEqual(result->z, 0.0f);
    });

    auto test3 = larvae::RegisterTest("QueenColumn", "PushMultiple", []() {
        comb::LinearAllocator alloc{4096};
        queen::Column<comb::LinearAllocator> column{alloc, queen::ComponentMeta::Of<Position>(), 10};

        for (int i = 0; i < 5; ++i)
        {
            Position pos{static_cast<float>(i), static_cast<float>(i * 2), static_cast<float>(i * 3)};
            column.PushCopy(&pos);
        }

        larvae::AssertEqual(column.Size(), size_t{5});

        for (int i = 0; i < 5; ++i)
        {
            Position* result = column.Get<Position>(static_cast<size_t>(i));
            larvae::AssertEqual(result->x, static_cast<float>(i));
            larvae::AssertEqual(result->y, static_cast<float>(i * 2));
            larvae::AssertEqual(result->z, static_cast<float>(i * 3));
        }
    });

    auto test4 = larvae::RegisterTest("QueenColumn", "Pop", []() {
        comb::LinearAllocator alloc{4096};
        queen::Column<comb::LinearAllocator> column{alloc, queen::ComponentMeta::Of<Position>(), 10};

        Position pos1{1.0f, 2.0f, 3.0f};
        Position pos2{4.0f, 5.0f, 6.0f};
        column.PushCopy(&pos1);
        column.PushCopy(&pos2);

        larvae::AssertEqual(column.Size(), size_t{2});

        column.Pop();

        larvae::AssertEqual(column.Size(), size_t{1});
        larvae::AssertEqual(column.Get<Position>(0)->x, 1.0f);
    });

    auto test5 = larvae::RegisterTest("QueenColumn", "SwapRemove", []() {
        comb::LinearAllocator alloc{4096};
        queen::Column<comb::LinearAllocator> column{alloc, queen::ComponentMeta::Of<Position>(), 10};

        Position pos1{1.0f, 0.0f, 0.0f};
        Position pos2{2.0f, 0.0f, 0.0f};
        Position pos3{3.0f, 0.0f, 0.0f};
        column.PushCopy(&pos1);
        column.PushCopy(&pos2);
        column.PushCopy(&pos3);

        column.SwapRemove(0);

        larvae::AssertEqual(column.Size(), size_t{2});
        larvae::AssertEqual(column.Get<Position>(0)->x, 3.0f);
        larvae::AssertEqual(column.Get<Position>(1)->x, 2.0f);
    });

    auto test6 = larvae::RegisterTest("QueenColumn", "SwapRemoveLast", []() {
        comb::LinearAllocator alloc{4096};
        queen::Column<comb::LinearAllocator> column{alloc, queen::ComponentMeta::Of<Position>(), 10};

        Position pos1{1.0f, 0.0f, 0.0f};
        Position pos2{2.0f, 0.0f, 0.0f};
        column.PushCopy(&pos1);
        column.PushCopy(&pos2);

        column.SwapRemove(1);

        larvae::AssertEqual(column.Size(), size_t{1});
        larvae::AssertEqual(column.Get<Position>(0)->x, 1.0f);
    });

    auto test7 = larvae::RegisterTest("QueenColumn", "Clear", []() {
        comb::LinearAllocator alloc{4096};
        queen::Column<comb::LinearAllocator> column{alloc, queen::ComponentMeta::Of<Position>(), 10};

        for (int i = 0; i < 5; ++i)
        {
            Position pos{static_cast<float>(i), 0.0f, 0.0f};
            column.PushCopy(&pos);
        }

        column.Clear();

        larvae::AssertEqual(column.Size(), size_t{0});
        larvae::AssertTrue(column.IsEmpty());
    });

    auto test8 = larvae::RegisterTest("QueenColumn", "GrowsAutomatically", []() {
        comb::LinearAllocator alloc{65536};
        queen::Column<comb::LinearAllocator> column{alloc, queen::ComponentMeta::Of<Position>(), 2};

        larvae::AssertEqual(column.Capacity(), size_t{2});

        for (int i = 0; i < 100; ++i)
        {
            Position pos{static_cast<float>(i), 0.0f, 0.0f};
            column.PushCopy(&pos);
        }

        larvae::AssertEqual(column.Size(), size_t{100});
        larvae::AssertGreaterEqual(column.Capacity(), size_t{100});

        for (int i = 0; i < 100; ++i)
        {
            larvae::AssertEqual(column.Get<Position>(static_cast<size_t>(i))->x, static_cast<float>(i));
        }
    });

    auto test9 = larvae::RegisterTest("QueenColumn", "NonTrivialConstruction", []() {
        NonTrivial::ResetCounts();

        {
            comb::LinearAllocator alloc{4096};
            queen::Column<comb::LinearAllocator> column{alloc, queen::ComponentMeta::Of<NonTrivial>(), 10};

            column.PushDefault();
            column.PushDefault();

            larvae::AssertEqual(NonTrivial::construct_count, 2);
        }

        larvae::AssertEqual(NonTrivial::destruct_count, 2);
    });

    auto test10 = larvae::RegisterTest("QueenColumn", "NonTrivialSwapRemove", []() {
        NonTrivial::ResetCounts();

        comb::LinearAllocator alloc{4096};
        queen::Column<comb::LinearAllocator> column{alloc, queen::ComponentMeta::Of<NonTrivial>(), 10};

        NonTrivial a{1};
        NonTrivial b{2};
        NonTrivial c{3};
        column.PushCopy(&a);
        column.PushCopy(&b);
        column.PushCopy(&c);

        int pre_destruct = NonTrivial::destruct_count;
        column.SwapRemove(0);

        larvae::AssertGreaterThan(NonTrivial::destruct_count, pre_destruct);
        larvae::AssertEqual(column.Size(), size_t{2});
        larvae::AssertEqual(column.Get<NonTrivial>(0)->value, 3);
    });

    auto test11 = larvae::RegisterTest("QueenColumn", "TypeId", []() {
        comb::LinearAllocator alloc{4096};
        queen::Column<comb::LinearAllocator> column{alloc, queen::ComponentMeta::Of<Position>(), 10};

        larvae::AssertEqual(column.GetTypeId(), queen::TypeIdOf<Position>());
    });

    auto test12 = larvae::RegisterTest("QueenColumn", "PushMove", []() {
        comb::LinearAllocator alloc{4096};
        queen::Column<comb::LinearAllocator> column{alloc, queen::ComponentMeta::Of<Position>(), 10};

        Position pos{1.0f, 2.0f, 3.0f};
        column.PushMove(&pos);

        larvae::AssertEqual(column.Size(), size_t{1});
        larvae::AssertEqual(column.Get<Position>(0)->x, 1.0f);
    });

    auto test13 = larvae::RegisterTest("QueenColumn", "DataPointer", []() {
        comb::LinearAllocator alloc{4096};
        queen::Column<comb::LinearAllocator> column{alloc, queen::ComponentMeta::Of<Position>(), 10};

        Position pos{1.0f, 2.0f, 3.0f};
        column.PushCopy(&pos);

        Position* data = column.Data<Position>();
        larvae::AssertNotNull(data);
        larvae::AssertEqual(data[0].x, 1.0f);
    });

    auto test14 = larvae::RegisterTest("QueenColumn", "Alignment", []() {
        struct alignas(32) AlignedComponent
        {
            float data[8];
        };

        comb::LinearAllocator alloc{8192};
        queen::Column<comb::LinearAllocator> column{alloc, queen::ComponentMeta::Of<AlignedComponent>(), 10};

        AlignedComponent comp{};
        column.PushCopy(&comp);

        void* ptr = column.GetRaw(0);
        larvae::AssertEqual(reinterpret_cast<uintptr_t>(ptr) % 32, uintptr_t{0});
    });
}
