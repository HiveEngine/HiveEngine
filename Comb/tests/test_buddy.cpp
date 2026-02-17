#include <larvae/larvae.h>
#include <comb/buddy_allocator.h>
#include <comb/allocator_concepts.h>
#include <comb/new.h>
#include <cstring>

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

    // =============================================================================
    // Concept Satisfaction
    // =============================================================================

    auto test12 = larvae::RegisterTest("BuddyAllocator", "ConceptSatisfaction", []() {
        larvae::AssertTrue((comb::Allocator<comb::BuddyAllocator>));
    });

    // =============================================================================
    // Reset
    // =============================================================================

    auto test13 = larvae::RegisterTest("BuddyAllocator", "ResetFreesAllMemory", []() {
        comb::BuddyAllocator buddy{4_KB};

        void* p1 = buddy.Allocate(100, 8);
        void* p2 = buddy.Allocate(200, 8);
        void* p3 = buddy.Allocate(500, 8);
        larvae::AssertNotNull(p1);
        larvae::AssertNotNull(p2);
        larvae::AssertNotNull(p3);
        larvae::AssertTrue(buddy.GetUsedMemory() > 0);

        buddy.Reset();

        larvae::AssertEqual(buddy.GetUsedMemory(), 0u);
    });

    auto test14 = larvae::RegisterTest("BuddyAllocator", "ResetAllowsReuse", []() {
        comb::BuddyAllocator buddy{1_KB};

        // Fill most of the allocator
        void* p1 = buddy.Allocate(900, 8);
        larvae::AssertNotNull(p1);

        // No more room
        void* p2 = buddy.Allocate(200, 8);
        larvae::AssertNull(p2);

        buddy.Reset();

        // Can allocate again
        void* p3 = buddy.Allocate(900, 8);
        larvae::AssertNotNull(p3);

        buddy.Deallocate(p3);
    });

    auto test15 = larvae::RegisterTest("BuddyAllocator", "ResetThenFullCycle", []() {
        comb::BuddyAllocator buddy{4_KB};

        for (int cycle = 0; cycle < 5; ++cycle)
        {
            void* ptrs[8];
            for (int i = 0; i < 8; ++i)
            {
                ptrs[i] = buddy.Allocate(64, 8);
                larvae::AssertNotNull(ptrs[i]);
            }

            buddy.Reset();
            larvae::AssertEqual(buddy.GetUsedMemory(), 0u);
        }
    });

    // =============================================================================
    // Alignment
    // =============================================================================

    auto test16 = larvae::RegisterTest("BuddyAllocator", "AllocatedPointersAreUsable", []() {
        comb::BuddyAllocator buddy{1_MB};

        // BuddyAllocator blocks are power-of-2 sized; alignment depends on block placement.
        // In debug mode, guard bytes may offset the user pointer.
        // Verify that allocations for various sizes are valid and usable.
        void* ptr1 = buddy.Allocate(100, 8);
        void* ptr2 = buddy.Allocate(200, 8);
        void* ptr3 = buddy.Allocate(500, 8);

        larvae::AssertNotNull(ptr1);
        larvae::AssertNotNull(ptr2);
        larvae::AssertNotNull(ptr3);

        // Memory should be writable
        std::memset(ptr1, 0xAA, 100);
        std::memset(ptr2, 0xBB, 200);
        std::memset(ptr3, 0xCC, 500);

        larvae::AssertEqual(static_cast<unsigned char*>(ptr1)[0], static_cast<unsigned char>(0xAA));
        larvae::AssertEqual(static_cast<unsigned char*>(ptr2)[0], static_cast<unsigned char>(0xBB));
        larvae::AssertEqual(static_cast<unsigned char*>(ptr3)[0], static_cast<unsigned char>(0xCC));

        buddy.Deallocate(ptr1);
        buddy.Deallocate(ptr2);
        buddy.Deallocate(ptr3);
    });

    auto test17 = larvae::RegisterTest("BuddyAllocator", "ManyDifferentSizes", []() {
        comb::BuddyAllocator buddy{1_MB};

        void* ptrs[20];
        for (int i = 0; i < 20; ++i)
        {
            ptrs[i] = buddy.Allocate(48 + static_cast<size_t>(i) * 16, 8);
            larvae::AssertNotNull(ptrs[i]);
        }

        for (int i = 0; i < 20; ++i)
        {
            buddy.Deallocate(ptrs[i]);
        }

        larvae::AssertEqual(buddy.GetUsedMemory(), 0u);
    });

    // =============================================================================
    // Move Semantics
    // =============================================================================

    auto test18 = larvae::RegisterTest("BuddyAllocator", "MoveConstructorTransfersOwnership", []() {
        comb::BuddyAllocator buddy1{1_MB};
        void* ptr = buddy1.Allocate(100, 8);
        larvae::AssertNotNull(ptr);
        size_t used = buddy1.GetUsedMemory();

        comb::BuddyAllocator buddy2{std::move(buddy1)};

        larvae::AssertEqual(buddy2.GetUsedMemory(), used);
        larvae::AssertEqual(buddy2.GetTotalMemory(), 1_MB);
        larvae::AssertStringEqual(buddy2.GetName(), "BuddyAllocator");

        buddy2.Deallocate(ptr);
        larvae::AssertEqual(buddy2.GetUsedMemory(), 0u);
    });

    auto test19 = larvae::RegisterTest("BuddyAllocator", "MoveAssignmentTransfersOwnership", []() {
        comb::BuddyAllocator buddy1{1_MB};
        void* ptr = buddy1.Allocate(200, 8);
        larvae::AssertNotNull(ptr);
        size_t used = buddy1.GetUsedMemory();

        comb::BuddyAllocator buddy2{4_KB};

        buddy2 = std::move(buddy1);

        larvae::AssertEqual(buddy2.GetUsedMemory(), used);
        larvae::AssertEqual(buddy2.GetTotalMemory(), 1_MB);

        buddy2.Deallocate(ptr);
        larvae::AssertEqual(buddy2.GetUsedMemory(), 0u);
    });

    auto test20 = larvae::RegisterTest("BuddyAllocator", "MoveConstructorNullifiesSource", []() {
        comb::BuddyAllocator buddy1{1_MB};
        static_cast<void>(buddy1.Allocate(100, 8));

        comb::BuddyAllocator buddy2{std::move(buddy1)};

        larvae::AssertEqual(buddy1.GetTotalMemory(), 0u);
        larvae::AssertEqual(buddy1.GetUsedMemory(), 0u);
    });

    // =============================================================================
    // New/Delete
    // =============================================================================

    auto test21 = larvae::RegisterTest("BuddyAllocator", "NewDeleteWorks", []() {
        comb::BuddyAllocator buddy{1_MB};

        struct TestObj
        {
            int value;
            TestObj(int v) : value{v} {}
        };

        TestObj* obj = comb::New<TestObj>(buddy, 42);
        larvae::AssertNotNull(obj);
        larvae::AssertEqual(obj->value, 42);

        comb::Delete(buddy, obj);
        larvae::AssertEqual(buddy.GetUsedMemory(), 0u);
    });

    auto test22 = larvae::RegisterTest("BuddyAllocator", "DeleteCallsDestructor", []() {
        comb::BuddyAllocator buddy{1_MB};

        struct TestObj
        {
            bool* destroyed;
            TestObj(bool* d) : destroyed{d} { *destroyed = false; }
            ~TestObj() { *destroyed = true; }
        };

        bool destroyed = false;
        TestObj* obj = comb::New<TestObj>(buddy, &destroyed);
        larvae::AssertFalse(destroyed);

        comb::Delete(buddy, obj);
        larvae::AssertTrue(destroyed);
    });

    // =============================================================================
    // Memory Access
    // =============================================================================

    auto test23 = larvae::RegisterTest("BuddyAllocator", "AllocatedMemoryIsWritable", []() {
        comb::BuddyAllocator buddy{1_MB};

        void* ptr = buddy.Allocate(256, 8);
        auto* byte_ptr = static_cast<unsigned char*>(ptr);

        std::memset(byte_ptr, 0x42, 256);

        for (size_t i = 0; i < 256; ++i)
        {
            larvae::AssertEqual(byte_ptr[i], static_cast<unsigned char>(0x42));
        }

        buddy.Deallocate(ptr);
    });

    // =============================================================================
    // GetName
    // =============================================================================

    auto test24 = larvae::RegisterTest("BuddyAllocator", "GetNameReturnsCorrectName", []() {
        comb::BuddyAllocator buddy{1_KB};
        larvae::AssertStringEqual(buddy.GetName(), "BuddyAllocator");
    });
}
