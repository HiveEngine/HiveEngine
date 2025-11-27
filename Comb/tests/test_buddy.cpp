#include <larvae/larvae.h>
#include <comb/buddy_allocator.h>

namespace
{
    constexpr size_t operator""_KB(unsigned long long kb) { return kb * 1024; }
    constexpr size_t operator""_MB(unsigned long long mb) { return mb * 1024 * 1024; }

    auto test1 = larvae::RegisterTest("BuddyAllocator", "BasicAllocation", []() {
        comb::BuddyAllocator buddy{1_MB};

        void* ptr1 = buddy.Allocate(100, 8);
        larvae::AssertNotNull(ptr1);

        void* ptr2 = buddy.Allocate(200, 8);
        larvae::AssertNotNull(ptr2);

        void* ptr3 = buddy.Allocate(500, 8);
        larvae::AssertNotNull(ptr3);

        buddy.Deallocate(ptr1);
        buddy.Deallocate(ptr2);
        buddy.Deallocate(ptr3);

        larvae::AssertEqual(buddy.GetUsedMemory(), 0u);
    });

    auto test2 = larvae::RegisterTest("BuddyAllocator", "PowerOfTwoRounding", []() {
        comb::BuddyAllocator buddy{64_KB};

        void* ptr1 = buddy.Allocate(48, 8);
        larvae::AssertNotNull(ptr1);
        larvae::AssertEqual(buddy.GetUsedMemory(), 64u);

        void* ptr2 = buddy.Allocate(112, 8);
        larvae::AssertNotNull(ptr2);
        larvae::AssertEqual(buddy.GetUsedMemory(), 192u);

        void* ptr3 = buddy.Allocate(240, 8);
        larvae::AssertNotNull(ptr3);
        larvae::AssertEqual(buddy.GetUsedMemory(), 448u);

        buddy.Deallocate(ptr1);
        larvae::AssertEqual(buddy.GetUsedMemory(), 384u);

        buddy.Deallocate(ptr2);
        larvae::AssertEqual(buddy.GetUsedMemory(), 256u);

        buddy.Deallocate(ptr3);
        larvae::AssertEqual(buddy.GetUsedMemory(), 0u);
    });

    auto test3 = larvae::RegisterTest("BuddyAllocator", "Splitting", []() {
        comb::BuddyAllocator buddy{1_KB};

        void* ptr1 = buddy.Allocate(48, 8);
        larvae::AssertNotNull(ptr1);
        larvae::AssertEqual(buddy.GetUsedMemory(), 64u);

        void* ptr2 = buddy.Allocate(48, 8);
        larvae::AssertNotNull(ptr2);
        larvae::AssertEqual(buddy.GetUsedMemory(), 128u);

        void* ptr3 = buddy.Allocate(112, 8);
        larvae::AssertNotNull(ptr3);
        larvae::AssertEqual(buddy.GetUsedMemory(), 256u);

        buddy.Deallocate(ptr1);
        buddy.Deallocate(ptr2);
        buddy.Deallocate(ptr3);
        larvae::AssertEqual(buddy.GetUsedMemory(), 0u);
    });

    auto test4 = larvae::RegisterTest("BuddyAllocator", "Coalescing", []() {
        comb::BuddyAllocator buddy{1_KB};

        void* ptr1 = buddy.Allocate(112, 8);
        void* ptr2 = buddy.Allocate(112, 8);
        larvae::AssertNotNull(ptr1);
        larvae::AssertNotNull(ptr2);
        larvae::AssertEqual(buddy.GetUsedMemory(), 256u);

        buddy.Deallocate(ptr1);
        buddy.Deallocate(ptr2);
        larvae::AssertEqual(buddy.GetUsedMemory(), 0u);

        void* big = buddy.Allocate(240, 8);
        larvae::AssertNotNull(big);
        larvae::AssertEqual(buddy.GetUsedMemory(), 256u);

        buddy.Deallocate(big);
    });

    auto test5 = larvae::RegisterTest("BuddyAllocator", "OutOfMemory", []() {
        comb::BuddyAllocator buddy{1_KB};

        void* ptr1 = buddy.Allocate(1000, 8);
        larvae::AssertNotNull(ptr1);
        larvae::AssertEqual(buddy.GetUsedMemory(), 1024u);

        void* ptr2 = buddy.Allocate(64, 8);
        larvae::AssertNull(ptr2);

        buddy.Deallocate(ptr1);
        ptr2 = buddy.Allocate(64, 8);
        larvae::AssertNotNull(ptr2);

        buddy.Deallocate(ptr2);
    });

    auto test6 = larvae::RegisterTest("BuddyAllocator", "MixedSizes", []() {
        comb::BuddyAllocator buddy{4_KB};

        void* p1 = buddy.Allocate(50, 8);
        void* p2 = buddy.Allocate(100, 8);
        void* p3 = buddy.Allocate(200, 8);
        void* p4 = buddy.Allocate(500, 8);
        void* p5 = buddy.Allocate(1000, 8);

        larvae::AssertNotNull(p1);
        larvae::AssertNotNull(p2);
        larvae::AssertNotNull(p3);
        larvae::AssertNotNull(p4);
        larvae::AssertNotNull(p5);

        larvae::AssertEqual(buddy.GetUsedMemory(), 1984u);

        buddy.Deallocate(p3);
        larvae::AssertEqual(buddy.GetUsedMemory(), 1728u);

        buddy.Deallocate(p1);
        larvae::AssertEqual(buddy.GetUsedMemory(), 1664u);

        buddy.Deallocate(p5);
        larvae::AssertEqual(buddy.GetUsedMemory(), 640u);

        buddy.Deallocate(p2);
        larvae::AssertEqual(buddy.GetUsedMemory(), 512u);

        buddy.Deallocate(p4);
        larvae::AssertEqual(buddy.GetUsedMemory(), 0u);
    });

    auto test7 = larvae::RegisterTest("BuddyAllocator", "RepeatedAllocations", []() {
        comb::BuddyAllocator buddy{16_KB};

        for (int i = 0; i < 100; ++i)
        {
            void* ptr = buddy.Allocate(64, 8);
            larvae::AssertNotNull(ptr);
            buddy.Deallocate(ptr);
        }

        larvae::AssertEqual(buddy.GetUsedMemory(), 0u);
    });

    auto test8 = larvae::RegisterTest("BuddyAllocator", "LargeAllocation", []() {
        comb::BuddyAllocator buddy{16_MB};

        void* ptr = buddy.Allocate(8_MB, 8);
        larvae::AssertNotNull(ptr);

        larvae::AssertEqual(buddy.GetUsedMemory(), 16_MB);

        buddy.Deallocate(ptr);
        larvae::AssertEqual(buddy.GetUsedMemory(), 0u);
    });

    auto test9 = larvae::RegisterTest("BuddyAllocator", "SmallAllocations", []() {
        comb::BuddyAllocator buddy{4_KB};

        void* ptrs[32];
        for (int i = 0; i < 32; ++i)
        {
            ptrs[i] = buddy.Allocate(32, 8);
            larvae::AssertNotNull(ptrs[i]);
        }

        larvae::AssertEqual(buddy.GetUsedMemory(), 2048u);

        for (int i = 0; i < 32; ++i)
        {
            buddy.Deallocate(ptrs[i]);
        }

        larvae::AssertEqual(buddy.GetUsedMemory(), 0u);
    });

    auto test10 = larvae::RegisterTest("BuddyAllocator", "FragmentationRecovery", []() {
        comb::BuddyAllocator buddy{4_KB};

        void* p1 = buddy.Allocate(240, 8);
        void* p2 = buddy.Allocate(240, 8);
        void* p3 = buddy.Allocate(240, 8);
        void* p4 = buddy.Allocate(240, 8);

        buddy.Deallocate(p1);
        buddy.Deallocate(p3);

        larvae::AssertEqual(buddy.GetUsedMemory(), 512u);

        buddy.Deallocate(p2);
        buddy.Deallocate(p4);

        larvae::AssertEqual(buddy.GetUsedMemory(), 0u);

        void* big = buddy.Allocate(1000, 8);
        larvae::AssertNotNull(big);
        buddy.Deallocate(big);
    });

    auto test11 = larvae::RegisterTest("BuddyAllocator", "NullDeallocation", []() {
        comb::BuddyAllocator buddy{1_KB};

        buddy.Deallocate(nullptr);
        larvae::AssertEqual(buddy.GetUsedMemory(), 0u);
    });
}
