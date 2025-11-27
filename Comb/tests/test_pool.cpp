#include <larvae/larvae.h>
#include <comb/pool_allocator.h>
#include <cstring>

namespace
{
    struct TestObject
    {
        int value;
        float data;

        TestObject(int v = 0, float d = 0.0f) : value{v}, data{d} {}
    };

    struct LargeObject
    {
        char buffer[256];
    };

    auto test1 = larvae::RegisterTest("PoolAllocator", "ConstructorInitializesCorrectly", []() {
        comb::PoolAllocator<TestObject> pool{100};

        larvae::AssertEqual(pool.GetCapacity(), 100u);
        larvae::AssertEqual(pool.GetUsedCount(), 0u);
        larvae::AssertEqual(pool.GetFreeCount(), 100u);
        larvae::AssertTrue(pool.GetTotalMemory() > 0);
    });

    auto test2 = larvae::RegisterTest("PoolAllocator", "AllocateReturnsValidPointer", []() {
        comb::PoolAllocator<TestObject> pool{10};

        void* ptr = pool.Allocate(sizeof(TestObject), alignof(TestObject));
        larvae::AssertNotNull(ptr);
        larvae::AssertEqual(pool.GetUsedCount(), 1u);
        larvae::AssertEqual(pool.GetFreeCount(), 9u);

        pool.Deallocate(ptr);
    });

    auto test3 = larvae::RegisterTest("PoolAllocator", "AllocateMultipleObjects", []() {
        comb::PoolAllocator<TestObject> pool{10};

        void* ptrs[5];
        for (int i = 0; i < 5; ++i)
        {
            ptrs[i] = pool.Allocate(sizeof(TestObject), alignof(TestObject));
            larvae::AssertNotNull(ptrs[i]);
        }

        larvae::AssertEqual(pool.GetUsedCount(), 5u);
        larvae::AssertEqual(pool.GetFreeCount(), 5u);

        // All pointers should be different
        for (int i = 0; i < 4; ++i)
        {
            larvae::AssertNotEqual(ptrs[i], ptrs[i + 1]);
        }

        // Clean up
        for (int i = 0; i < 5; ++i)
        {
            pool.Deallocate(ptrs[i]);
        }
    });

    auto test4 = larvae::RegisterTest("PoolAllocator", "AllocateWhenPoolExhaustedReturnsNull", []() {
        comb::PoolAllocator<TestObject> pool{2};

        void* ptr1 = pool.Allocate(sizeof(TestObject), alignof(TestObject));
        void* ptr2 = pool.Allocate(sizeof(TestObject), alignof(TestObject));
        void* ptr3 = pool.Allocate(sizeof(TestObject), alignof(TestObject));

        larvae::AssertNotNull(ptr1);
        larvae::AssertNotNull(ptr2);
        larvae::AssertTrue(ptr3 == nullptr);
        larvae::AssertEqual(pool.GetUsedCount(), 2u);
        larvae::AssertEqual(pool.GetFreeCount(), 0u);

        // Clean up
        pool.Deallocate(ptr1);
        pool.Deallocate(ptr2);
    });

    auto test5 = larvae::RegisterTest("PoolAllocator", "DeallocateWithNullptrIsSafe", []() {
        comb::PoolAllocator<TestObject> pool{10};

        pool.Deallocate(nullptr);
        larvae::AssertEqual(pool.GetUsedCount(), 0u);
    });

    auto test6 = larvae::RegisterTest("PoolAllocator", "DeallocateReturnsToFreeList", []() {
        comb::PoolAllocator<TestObject> pool{10};

        void* ptr = pool.Allocate(sizeof(TestObject), alignof(TestObject));
        larvae::AssertEqual(pool.GetUsedCount(), 1u);

        pool.Deallocate(ptr);
        larvae::AssertEqual(pool.GetUsedCount(), 0u);
        larvae::AssertEqual(pool.GetFreeCount(), 10u);
    });

    auto test7 = larvae::RegisterTest("PoolAllocator", "DeallocatedMemoryCanBeReused", []() {
        comb::PoolAllocator<TestObject> pool{10};

        void* ptr1 = pool.Allocate(sizeof(TestObject), alignof(TestObject));
        pool.Deallocate(ptr1);

        void* ptr2 = pool.Allocate(sizeof(TestObject), alignof(TestObject));

        larvae::AssertEqual(ptr1, ptr2);

        pool.Deallocate(ptr2);
    });

    auto test8 = larvae::RegisterTest("PoolAllocator", "AllocateAndDeallocateCycle", []() {
        comb::PoolAllocator<TestObject> pool{5};

        void* ptrs[5];
        for (int i = 0; i < 5; ++i)
        {
            ptrs[i] = pool.Allocate(sizeof(TestObject), alignof(TestObject));
        }
        larvae::AssertEqual(pool.GetUsedCount(), 5u);

        for (int i = 0; i < 5; ++i)
        {
            pool.Deallocate(ptrs[i]);
        }
        larvae::AssertEqual(pool.GetUsedCount(), 0u);

        for (int i = 0; i < 5; ++i)
        {
            ptrs[i] = pool.Allocate(sizeof(TestObject), alignof(TestObject));
            larvae::AssertNotNull(ptrs[i]);
        }
        larvae::AssertEqual(pool.GetUsedCount(), 5u);

        // Clean up second batch
        for (int i = 0; i < 5; ++i)
        {
            pool.Deallocate(ptrs[i]);
        }
    });

    auto test9 = larvae::RegisterTest("PoolAllocator", "ResetClearsAllAllocations", []() {
        comb::PoolAllocator<TestObject> pool{10};

        for (int i = 0; i < 5; ++i)
        {
            static_cast<void>(pool.Allocate(sizeof(TestObject), alignof(TestObject)));
        }
        larvae::AssertEqual(pool.GetUsedCount(), 5u);

        pool.Reset();

        larvae::AssertEqual(pool.GetUsedCount(), 0u);
        larvae::AssertEqual(pool.GetFreeCount(), 10u);
    });

    auto test10 = larvae::RegisterTest("PoolAllocator", "AllocatedMemoryIsWritable", []() {
        comb::PoolAllocator<TestObject> pool{10};

        void* ptr = pool.Allocate(sizeof(TestObject), alignof(TestObject));
        std::memset(ptr, 0xAA, sizeof(TestObject));

        auto* bytes = static_cast<unsigned char*>(ptr);
        for (size_t i = 0; i < sizeof(TestObject); ++i)
        {
            larvae::AssertEqual(bytes[i], static_cast<unsigned char>(0xAA));
        }

        pool.Deallocate(ptr);
    });

    auto test11 = larvae::RegisterTest("PoolAllocator", "AllocateAndConstructObject", []() {
        comb::PoolAllocator<TestObject> pool{10};

        void* mem = pool.Allocate(sizeof(TestObject), alignof(TestObject));
        auto* obj = new (mem) TestObject(42, 3.14f);

        larvae::AssertNotNull(obj);
        larvae::AssertEqual(obj->value, 42);
        larvae::AssertEqual(obj->data, 3.14f);

        obj->~TestObject();
        pool.Deallocate(mem);
    });

    auto test12 = larvae::RegisterTest("PoolAllocator", "UseCombNewAndDelete", []() {
        comb::PoolAllocator<TestObject> pool{10};

        TestObject* obj = comb::New<TestObject>(pool, 99, 2.71f);

        larvae::AssertNotNull(obj);
        larvae::AssertEqual(obj->value, 99);
        larvae::AssertEqual(obj->data, 2.71f);
        larvae::AssertEqual(pool.GetUsedCount(), 1u);

        comb::Delete(pool, obj);
        larvae::AssertEqual(pool.GetUsedCount(), 0u);
    });

    auto test13 = larvae::RegisterTest("PoolAllocator", "GetNameReturnsCorrectName", []() {
        comb::PoolAllocator<TestObject> pool{10};

        larvae::AssertStringEqual(pool.GetName(), "PoolAllocator");
    });

    auto test14 = larvae::RegisterTest("PoolAllocator", "GetTotalMemoryReturnsCorrectValue", []() {
        constexpr size_t N = 100;
        comb::PoolAllocator<TestObject> pool{N};

        larvae::AssertEqual(pool.GetTotalMemory(), N * sizeof(TestObject));
    });

    auto test15 = larvae::RegisterTest("PoolAllocator", "LargeObjectsWork", []() {
        comb::PoolAllocator<LargeObject> pool{10};

        LargeObject* obj = comb::New<LargeObject>(pool);
        larvae::AssertNotNull(obj);

        std::memset(obj->buffer, 0xFF, 256);
        larvae::AssertEqual(obj->buffer[0], static_cast<char>(0xFF));
        larvae::AssertEqual(obj->buffer[255], static_cast<char>(0xFF));

        comb::Delete(pool, obj);
    });

    auto test16 = larvae::RegisterTest("PoolAllocator", "ManyAllocationsAndDeallocations", []() {
        comb::PoolAllocator<TestObject> pool{100};

        for (int cycle = 0; cycle < 10; ++cycle)
        {
            TestObject* objects[50];
            for (int i = 0; i < 50; ++i)
            {
                objects[i] = comb::New<TestObject>(pool, i, static_cast<float>(i) * 1.5f);
                larvae::AssertNotNull(objects[i]);
            }

            larvae::AssertEqual(pool.GetUsedCount(), 50u);

            for (int i = 0; i < 50; ++i)
            {
                comb::Delete(pool, objects[i]);
            }

            larvae::AssertEqual(pool.GetUsedCount(), 0u);
        }
    });

    auto test17 = larvae::RegisterTest("PoolAllocator", "PartialDeallocation", []() {
        comb::PoolAllocator<TestObject> pool{10};

        TestObject* objects[10];
        for (int i = 0; i < 10; ++i)
        {
            objects[i] = comb::New<TestObject>(pool, i, 0.0f);
        }

        // Deallocate some objects
        comb::Delete(pool, objects[2]);
        comb::Delete(pool, objects[5]);
        comb::Delete(pool, objects[8]);

        larvae::AssertEqual(pool.GetUsedCount(), 7u);
        larvae::AssertEqual(pool.GetFreeCount(), 3u);

        TestObject* new1 = comb::New<TestObject>(pool, 100, 0.0f);
        TestObject* new2 = comb::New<TestObject>(pool, 200, 0.0f);

        larvae::AssertNotNull(new1);
        larvae::AssertNotNull(new2);
        larvae::AssertEqual(pool.GetUsedCount(), 9u);

        // Clean up remaining objects
        for (int i = 0; i < 10; ++i)
        {
            if (i != 2 && i != 5 && i != 8) // Skip already deallocated
            {
                comb::Delete(pool, objects[i]);
            }
        }
        comb::Delete(pool, new1);
        comb::Delete(pool, new2);
    });
}
