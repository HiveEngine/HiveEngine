#include <larvae/larvae.h>
#include <comb/slab_allocator.h>
#include <comb/allocator_concepts.h>

namespace
{
    auto test1 = larvae::RegisterTest("SlabAllocator", "ConceptSatisfaction", []() {
        using TestAllocator = comb::SlabAllocator<100, 32, 64, 128>;
        larvae::AssertTrue((comb::Allocator<TestAllocator>));
    });

    auto test2 = larvae::RegisterTest("SlabAllocator", "Construction", []() {
        comb::SlabAllocator<100, 32, 64, 128> slabs;

        larvae::AssertEqual(slabs.GetSlabCount(), 3u);
        larvae::AssertEqual(slabs.GetUsedMemory(), 0u);
        larvae::AssertEqual(slabs.GetTotalMemory(), 100u * (32u + 64u + 128u));
    });

    auto test3 = larvae::RegisterTest("SlabAllocator", "BasicAllocation", []() {
        comb::SlabAllocator<100, 32, 64, 128> slabs;

        void* ptr = slabs.Allocate(32, 8);
        larvae::AssertNotNull(ptr);
        larvae::AssertEqual(slabs.GetUsedMemory(), 32u);

        slabs.Deallocate(ptr);
    });

    auto test4 = larvae::RegisterTest("SlabAllocator", "SlabSelection", []() {
        comb::SlabAllocator<100, 32, 64, 128> slabs;

        void* ptr1 = slabs.Allocate(20, 8);
        larvae::AssertNotNull(ptr1);
        larvae::AssertEqual(slabs.GetSlabUsedCount(0), 1u);

        void* ptr2 = slabs.Allocate(50, 8);
        larvae::AssertNotNull(ptr2);
        larvae::AssertEqual(slabs.GetSlabUsedCount(1), 1u);

        void* ptr3 = slabs.Allocate(100, 8);
        larvae::AssertNotNull(ptr3);
        larvae::AssertEqual(slabs.GetSlabUsedCount(2), 1u);

        slabs.Deallocate(ptr1);
        slabs.Deallocate(ptr2);
        slabs.Deallocate(ptr3);
    });

    auto test5 = larvae::RegisterTest("SlabAllocator", "RoundUpToSizeClass", []() {
        comb::SlabAllocator<100, 32, 64, 128> slabs;

        void* ptr1 = slabs.Allocate(1, 8);
        larvae::AssertNotNull(ptr1);
        larvae::AssertEqual(slabs.GetSlabUsedCount(0), 1u);

        void* ptr2 = slabs.Allocate(33, 8);
        larvae::AssertNotNull(ptr2);
        larvae::AssertEqual(slabs.GetSlabUsedCount(1), 1u);

        slabs.Deallocate(ptr1);
        slabs.Deallocate(ptr2);
    });

    auto test6 = larvae::RegisterTest("SlabAllocator", "MultipleAllocations", []() {
        comb::SlabAllocator<10, 32, 64> slabs;

        void* ptrs[20];

        for (int i = 0; i < 10; ++i)
        {
            ptrs[i] = slabs.Allocate(32, 8);
            larvae::AssertNotNull(ptrs[i]);
        }

        for (int i = 10; i < 20; ++i)
        {
            ptrs[i] = slabs.Allocate(64, 8);
            larvae::AssertNotNull(ptrs[i]);
        }

        larvae::AssertEqual(slabs.GetSlabUsedCount(0), 10u);
        larvae::AssertEqual(slabs.GetSlabUsedCount(1), 10u);

        for (int i = 0; i < 20; ++i)
        {
            slabs.Deallocate(ptrs[i]);
        }

        larvae::AssertEqual(slabs.GetSlabUsedCount(0), 0u);
        larvae::AssertEqual(slabs.GetSlabUsedCount(1), 0u);
    });

    auto test7 = larvae::RegisterTest("SlabAllocator", "SlabExhaustion", []() {
        comb::SlabAllocator<5, 32> slabs;

        void* ptrs[5];

        for (int i = 0; i < 5; ++i)
        {
            ptrs[i] = slabs.Allocate(32, 8);
            larvae::AssertNotNull(ptrs[i]);
        }

        void* overflow = slabs.Allocate(32, 8);
        larvae::AssertNull(overflow);

        slabs.Deallocate(ptrs[0]);

        void* reused = slabs.Allocate(32, 8);
        larvae::AssertNotNull(reused);
        larvae::AssertEqual(reused, ptrs[0]);

        for (int i = 0; i < 5; ++i)
        {
            slabs.Deallocate(ptrs[i]);
        }
    });

    auto test8 = larvae::RegisterTest("SlabAllocator", "TooLargeAllocation", []() {
        comb::SlabAllocator<100, 32, 64, 128> slabs;

        void* ptr = slabs.Allocate(256, 8);
        larvae::AssertNull(ptr);

        larvae::AssertEqual(slabs.GetUsedMemory(), 0u);
    });

    auto test9 = larvae::RegisterTest("SlabAllocator", "Deallocation", []() {
        comb::SlabAllocator<100, 32, 64> slabs;

        void* ptr1 = slabs.Allocate(32, 8);
        void* ptr2 = slabs.Allocate(64, 8);

        larvae::AssertNotNull(ptr1);
        larvae::AssertNotNull(ptr2);

        slabs.Deallocate(ptr1);
        larvae::AssertEqual(slabs.GetSlabUsedCount(0), 0u);
        larvae::AssertEqual(slabs.GetSlabUsedCount(1), 1u);

        slabs.Deallocate(ptr2);
        larvae::AssertEqual(slabs.GetSlabUsedCount(1), 0u);

        larvae::AssertEqual(slabs.GetUsedMemory(), 0u);
    });

    auto test10 = larvae::RegisterTest("SlabAllocator", "DeallocateNullptr", []() {
        comb::SlabAllocator<100, 32> slabs;

        slabs.Deallocate(nullptr);

        larvae::AssertEqual(slabs.GetUsedMemory(), 0u);
    });

    auto test11 = larvae::RegisterTest("SlabAllocator", "Reset", []() {
        comb::SlabAllocator<100, 32, 64> slabs;

        void* ptr1 = slabs.Allocate(32, 8);
        void* ptr2 = slabs.Allocate(64, 8);
        void* ptr3 = slabs.Allocate(32, 8);

        larvae::AssertNotNull(ptr1);
        larvae::AssertNotNull(ptr2);
        larvae::AssertNotNull(ptr3);

        larvae::AssertEqual(slabs.GetSlabUsedCount(0), 2u);
        larvae::AssertEqual(slabs.GetSlabUsedCount(1), 1u);

        slabs.Reset();

        larvae::AssertEqual(slabs.GetSlabUsedCount(0), 0u);
        larvae::AssertEqual(slabs.GetSlabUsedCount(1), 0u);
        larvae::AssertEqual(slabs.GetUsedMemory(), 0u);

        void* new_ptr = slabs.Allocate(32, 8);
        larvae::AssertNotNull(new_ptr);

        slabs.Deallocate(new_ptr);
    });

    auto test12 = larvae::RegisterTest("SlabAllocator", "MemoryRecycling", []() {
        comb::SlabAllocator<100, 64> slabs;

        void* ptr1 = slabs.Allocate(64, 8);
        larvae::AssertNotNull(ptr1);

        slabs.Deallocate(ptr1);

        void* ptr2 = slabs.Allocate(64, 8);
        larvae::AssertNotNull(ptr2);

        larvae::AssertEqual(ptr1, ptr2);

        slabs.Deallocate(ptr2);
    });

    auto test13 = larvae::RegisterTest("SlabAllocator", "GetName", []() {
        comb::SlabAllocator<100, 32> slabs;

        const char* name = slabs.GetName();
        larvae::AssertNotNull(name);
    });

    auto test14 = larvae::RegisterTest("SlabAllocator", "GetSizeClasses", []() {
        comb::SlabAllocator<100, 32, 64, 128> slabs;

        auto sizes = slabs.GetSizeClasses();
        larvae::AssertEqual(sizes[0], 32u);
        larvae::AssertEqual(sizes[1], 64u);
        larvae::AssertEqual(sizes[2], 128u);
    });

    auto test15 = larvae::RegisterTest("SlabAllocator", "GetSlabFreeCount", []() {
        comb::SlabAllocator<10, 32> slabs;

        larvae::AssertEqual(slabs.GetSlabFreeCount(0), 10u);

        void* ptr = slabs.Allocate(32, 8);
        larvae::AssertNotNull(ptr);

        larvae::AssertEqual(slabs.GetSlabFreeCount(0), 9u);

        slabs.Deallocate(ptr);

        larvae::AssertEqual(slabs.GetSlabFreeCount(0), 10u);
    });

    auto test16 = larvae::RegisterTest("SlabAllocator", "MoveConstruction", []() {
        comb::SlabAllocator<100, 32> slabs1;

        void* ptr1 = slabs1.Allocate(32, 8);
        larvae::AssertNotNull(ptr1);

        comb::SlabAllocator<100, 32> slabs2{std::move(slabs1)};

        larvae::AssertEqual(slabs2.GetSlabUsedCount(0), 1u);

        // Debug tracking is transferred with move - can deallocate normally
        slabs2.Deallocate(ptr1);

        larvae::AssertEqual(slabs2.GetUsedMemory(), 0u);
    });

    auto test17 = larvae::RegisterTest("SlabAllocator", "MoveAssignment", []() {
        comb::SlabAllocator<100, 32> slabs1;
        comb::SlabAllocator<100, 32> slabs2;

        void* ptr1 = slabs1.Allocate(32, 8);
        larvae::AssertNotNull(ptr1);

        slabs2 = std::move(slabs1);

        larvae::AssertEqual(slabs2.GetSlabUsedCount(0), 1u);

        // Debug tracking is transferred with move - can deallocate normally
        slabs2.Deallocate(ptr1);

        larvae::AssertEqual(slabs2.GetUsedMemory(), 0u);
    });

    auto test18 = larvae::RegisterTest("SlabAllocator", "PowerOfTwoSizeClasses", []() {
        comb::SlabAllocator<100, 30, 60, 120> slabs;

        auto sizes = slabs.GetSizeClasses();

        larvae::AssertEqual(sizes[0], 32u);
        larvae::AssertEqual(sizes[1], 64u);
        larvae::AssertEqual(sizes[2], 128u);
    });

    auto test19 = larvae::RegisterTest("SlabAllocator", "NewDeleteHelpers", []() {
        comb::SlabAllocator<100, 64> slabs;

        struct TestStruct
        {
            int value;
            float data;

            TestStruct(int v, float d) : value{v}, data{d} {}
        };

        TestStruct* obj = comb::New<TestStruct>(slabs, 42, 3.14f);
        larvae::AssertNotNull(obj);
        larvae::AssertEqual(obj->value, 42);
        larvae::AssertEqual(obj->data, 3.14f);

        comb::Delete(slabs, obj);

        larvae::AssertEqual(slabs.GetUsedMemory(), 0u);
    });

    auto test20 = larvae::RegisterTest("SlabAllocator", "MixedSizeAllocations", []() {
        comb::SlabAllocator<100, 16, 32, 64, 128, 256> slabs;

        void* ptr16 = slabs.Allocate(10, 8);
        void* ptr32 = slabs.Allocate(20, 8);
        void* ptr64 = slabs.Allocate(50, 8);
        void* ptr128 = slabs.Allocate(100, 8);
        void* ptr256 = slabs.Allocate(200, 8);

        larvae::AssertNotNull(ptr16);
        larvae::AssertNotNull(ptr32);
        larvae::AssertNotNull(ptr64);
        larvae::AssertNotNull(ptr128);
        larvae::AssertNotNull(ptr256);

        larvae::AssertEqual(slabs.GetSlabUsedCount(0), 1u);
        larvae::AssertEqual(slabs.GetSlabUsedCount(1), 1u);
        larvae::AssertEqual(slabs.GetSlabUsedCount(2), 1u);
        larvae::AssertEqual(slabs.GetSlabUsedCount(3), 1u);
        larvae::AssertEqual(slabs.GetSlabUsedCount(4), 1u);

        slabs.Deallocate(ptr16);
        slabs.Deallocate(ptr32);
        slabs.Deallocate(ptr64);
        slabs.Deallocate(ptr128);
        slabs.Deallocate(ptr256);

        larvae::AssertEqual(slabs.GetUsedMemory(), 0u);
    });
}
