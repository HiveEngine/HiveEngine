#include <larvae/larvae.h>
#include <comb/stack_allocator.h>
#include <comb/new.h>
#include <cstring>

namespace
{
    constexpr size_t operator""_KB(unsigned long long kb) { return kb * 1024; }
    constexpr size_t operator""_MB(unsigned long long mb) { return mb * 1024 * 1024; }

    // =============================================================================
    // Concept Satisfaction
    // =============================================================================

    auto test1 = larvae::RegisterTest("StackAllocator", "ConceptSatisfaction", []() {
        larvae::AssertTrue((comb::Allocator<comb::StackAllocator>));
    });

    // =============================================================================
    // Basic Functionality
    // =============================================================================

    auto test2 = larvae::RegisterTest("StackAllocator", "ConstructorInitializesCorrectly", []() {
        comb::StackAllocator allocator{1024};

        larvae::AssertEqual(allocator.GetUsedMemory(), 0u);
        larvae::AssertEqual(allocator.GetTotalMemory(), 1024u);
        larvae::AssertStringEqual(allocator.GetName(), "StackAllocator");
    });

    auto test3 = larvae::RegisterTest("StackAllocator", "AllocateReturnsValidPointer", []() {
        comb::StackAllocator allocator{1024};

        void* ptr = allocator.Allocate(64, 8);

        larvae::AssertNotNull(ptr);
        larvae::AssertEqual(allocator.GetUsedMemory(), 64u);
    });

    auto test4 = larvae::RegisterTest("StackAllocator", "AllocateUpdatesUsedMemory", []() {
        comb::StackAllocator allocator{1024};

        larvae::AssertEqual(allocator.GetUsedMemory(), 0u);

        static_cast<void>(allocator.Allocate(104, 8));
        larvae::AssertEqual(allocator.GetUsedMemory(), 104u);

        static_cast<void>(allocator.Allocate(200, 8));
        larvae::AssertEqual(allocator.GetUsedMemory(), 304u);
    });

    auto test5 = larvae::RegisterTest("StackAllocator", "MultipleAllocationsAreSequential", []() {
        comb::StackAllocator allocator{1024};

        void* ptr1 = allocator.Allocate(64, 8);
        void* ptr2 = allocator.Allocate(64, 8);
        void* ptr3 = allocator.Allocate(64, 8);

        larvae::AssertNotNull(ptr1);
        larvae::AssertNotNull(ptr2);
        larvae::AssertNotNull(ptr3);

        // Pointers should be in increasing order
        larvae::AssertTrue(ptr2 > ptr1);
        larvae::AssertTrue(ptr3 > ptr2);

        larvae::AssertEqual(allocator.GetUsedMemory(), 192u);
    });

    // =============================================================================
    // Alignment
    // =============================================================================

    auto test6 = larvae::RegisterTest("StackAllocator", "AllocateRespectsAlignment", []() {
        comb::StackAllocator allocator{1024};

        void* ptr16 = allocator.Allocate(10, 16);
        larvae::AssertEqual(reinterpret_cast<uintptr_t>(ptr16) % 16, 0u);

        void* ptr32 = allocator.Allocate(10, 32);
        larvae::AssertEqual(reinterpret_cast<uintptr_t>(ptr32) % 32, 0u);

        void* ptr64 = allocator.Allocate(10, 64);
        larvae::AssertEqual(reinterpret_cast<uintptr_t>(ptr64) % 64, 0u);
    });

    auto test7 = larvae::RegisterTest("StackAllocator", "AllocateWithMisalignedStart", []() {
        comb::StackAllocator allocator{1024};

        // Allocate 1 byte to misalign current pointer
        static_cast<void>(allocator.Allocate(1, 1));

        // Next allocation should still be properly aligned
        void* ptr = allocator.Allocate(64, 16);
        larvae::AssertEqual(reinterpret_cast<uintptr_t>(ptr) % 16, 0u);

        // Used memory includes padding
        larvae::AssertGreaterThan(allocator.GetUsedMemory(), 65u);
    });

    auto test8 = larvae::RegisterTest("StackAllocator", "Alignment128", []() {
        comb::StackAllocator allocator{4_KB};

        // Misalign first
        static_cast<void>(allocator.Allocate(3, 1));

        void* ptr = allocator.Allocate(64, 128);
        larvae::AssertNotNull(ptr);
        larvae::AssertEqual(reinterpret_cast<uintptr_t>(ptr) % 128, 0u);
    });

    // =============================================================================
    // Out of Memory
    // =============================================================================

    auto test9 = larvae::RegisterTest("StackAllocator", "AllocateReturnsNullWhenOutOfMemory", []() {
        comb::StackAllocator allocator{80};

        void* ptr1 = allocator.Allocate(64, 8);
        larvae::AssertNotNull(ptr1);

        // Second allocation should fail (not enough space)
        void* ptr2 = allocator.Allocate(64, 8);
        larvae::AssertNull(ptr2);
    });

    auto test10 = larvae::RegisterTest("StackAllocator", "AllocateSizeLargerThanCapacity", []() {
        comb::StackAllocator allocator{1024};

        void* ptr = allocator.Allocate(2048, 8);

        larvae::AssertNull(ptr);
        larvae::AssertEqual(allocator.GetUsedMemory(), 0u);
    });

    // =============================================================================
    // Markers
    // =============================================================================

    auto test11 = larvae::RegisterTest("StackAllocator", "GetMarkerReturnsCurrentPosition", []() {
        comb::StackAllocator allocator{1024};

        auto marker0 = allocator.GetMarker();
        larvae::AssertEqual(marker0, 0u);

        static_cast<void>(allocator.Allocate(100, 8));
        auto marker1 = allocator.GetMarker();

        static_cast<void>(allocator.Allocate(200, 8));
        auto marker2 = allocator.GetMarker();

        // Markers should be increasing
        larvae::AssertTrue(marker1 > marker0);
        larvae::AssertTrue(marker2 > marker1);
    });

    auto test12 = larvae::RegisterTest("StackAllocator", "FreeToMarkerRestoresPosition", []() {
        comb::StackAllocator allocator{1024};

        static_cast<void>(allocator.Allocate(104, 8));
        auto marker = allocator.GetMarker();

        static_cast<void>(allocator.Allocate(200, 8));
        larvae::AssertEqual(allocator.GetUsedMemory(), 304u);

        allocator.FreeToMarker(marker);

        larvae::AssertEqual(allocator.GetUsedMemory(), 104u);

        // Should be able to allocate again from marker position
        void* ptr = allocator.Allocate(56, 8);
        larvae::AssertNotNull(ptr);
    });

    auto test13 = larvae::RegisterTest("StackAllocator", "NestedMarkers", []() {
        comb::StackAllocator allocator{1024};

        static_cast<void>(allocator.Allocate(104, 8));
        auto marker1 = allocator.GetMarker();

        static_cast<void>(allocator.Allocate(200, 8));
        auto marker2 = allocator.GetMarker();

        static_cast<void>(allocator.Allocate(304, 8));
        larvae::AssertEqual(allocator.GetUsedMemory(), 608u);

        // Free inner scope
        allocator.FreeToMarker(marker2);
        larvae::AssertEqual(allocator.GetUsedMemory(), 304u);

        // Free outer scope
        allocator.FreeToMarker(marker1);
        larvae::AssertEqual(allocator.GetUsedMemory(), 104u);
    });

    auto test14 = larvae::RegisterTest("StackAllocator", "FreeToMarkerZeroFreesAll", []() {
        comb::StackAllocator allocator{1024};

        static_cast<void>(allocator.Allocate(200, 8));
        static_cast<void>(allocator.Allocate(300, 8));

        allocator.FreeToMarker(0);

        larvae::AssertEqual(allocator.GetUsedMemory(), 0u);

        // Can allocate again from start
        void* ptr = allocator.Allocate(64, 8);
        larvae::AssertNotNull(ptr);
    });

    auto test15 = larvae::RegisterTest("StackAllocator", "MarkerScopedPattern", []() {
        comb::StackAllocator allocator{4_KB};

        // Simulate scope pattern from documentation
        auto marker1 = allocator.GetMarker();
        void* data1 = allocator.Allocate(128, 8);
        larvae::AssertNotNull(data1);

        {
            auto marker2 = allocator.GetMarker();
            void* temp1 = allocator.Allocate(64, 8);
            void* temp2 = allocator.Allocate(64, 8);
            larvae::AssertNotNull(temp1);
            larvae::AssertNotNull(temp2);

            // Free inner scope
            allocator.FreeToMarker(marker2);
        }

        // data1 space is still accounted for
        larvae::AssertGreaterEqual(allocator.GetUsedMemory(), 128u);

        // Free everything
        allocator.FreeToMarker(marker1);
        larvae::AssertEqual(allocator.GetUsedMemory(), 0u);
    });

    // =============================================================================
    // Reset
    // =============================================================================

    auto test16 = larvae::RegisterTest("StackAllocator", "ResetFreesAllMemory", []() {
        comb::StackAllocator allocator{1024};

        static_cast<void>(allocator.Allocate(104, 8));
        static_cast<void>(allocator.Allocate(104, 8));
        static_cast<void>(allocator.Allocate(104, 8));

        larvae::AssertEqual(allocator.GetUsedMemory(), 312u);

        allocator.Reset();

        larvae::AssertEqual(allocator.GetUsedMemory(), 0u);

        // Should be able to allocate again
        void* ptr = allocator.Allocate(104, 8);
        larvae::AssertNotNull(ptr);
    });

    auto test17 = larvae::RegisterTest("StackAllocator", "ResetAllowsReuse", []() {
        comb::StackAllocator allocator{256};

        void* ptr1 = allocator.Allocate(100, 8);
        void* ptr2 = allocator.Allocate(100, 8);

        allocator.Reset();

        void* ptr3 = allocator.Allocate(100, 8);
        void* ptr4 = allocator.Allocate(100, 8);

        larvae::AssertEqual(ptr1, ptr3);
        larvae::AssertEqual(ptr2, ptr4);
    });

    // =============================================================================
    // Deallocate (No-Op)
    // =============================================================================

    auto test18 = larvae::RegisterTest("StackAllocator", "DeallocateIsNoOp", []() {
        comb::StackAllocator allocator{1024};

        void* ptr = allocator.Allocate(100, 8);
        size_t used_before = allocator.GetUsedMemory();

        allocator.Deallocate(ptr);

        size_t used_after = allocator.GetUsedMemory();

        // Deallocate should not change used memory
        larvae::AssertEqual(used_before, used_after);
    });

    auto test19 = larvae::RegisterTest("StackAllocator", "DeallocateNullptrIsSafe", []() {
        comb::StackAllocator allocator{1024};

        allocator.Deallocate(nullptr);

        larvae::AssertTrue(true);
    });

    // =============================================================================
    // Memory Access
    // =============================================================================

    auto test20 = larvae::RegisterTest("StackAllocator", "AllocatedMemoryIsWritable", []() {
        comb::StackAllocator allocator{1024};

        void* ptr = allocator.Allocate(256, 8);
        auto* byte_ptr = static_cast<unsigned char*>(ptr);

        std::memset(byte_ptr, 0x42, 256);

        for (size_t i = 0; i < 256; ++i)
        {
            larvae::AssertEqual(byte_ptr[i], static_cast<unsigned char>(0x42));
        }
    });

    auto test21 = larvae::RegisterTest("StackAllocator", "MultipleAllocationsAreIsolated", []() {
        comb::StackAllocator allocator{1024};

        void* ptr1 = allocator.Allocate(100, 8);
        void* ptr2 = allocator.Allocate(100, 8);

        auto* bytes1 = static_cast<unsigned char*>(ptr1);
        auto* bytes2 = static_cast<unsigned char*>(ptr2);

        std::memset(bytes1, 0xAA, 100);
        std::memset(bytes2, 0xBB, 100);

        larvae::AssertEqual(bytes1[0], static_cast<unsigned char>(0xAA));
        larvae::AssertEqual(bytes1[99], static_cast<unsigned char>(0xAA));
        larvae::AssertEqual(bytes2[0], static_cast<unsigned char>(0xBB));
        larvae::AssertEqual(bytes2[99], static_cast<unsigned char>(0xBB));
    });

    // =============================================================================
    // New/Delete
    // =============================================================================

    auto test22 = larvae::RegisterTest("StackAllocator", "NewConstructsObject", []() {
        comb::StackAllocator allocator{1024};

        struct TestObject
        {
            int value;
            TestObject(int v) : value{v} {}
        };

        TestObject* obj = comb::New<TestObject>(allocator, 42);

        larvae::AssertNotNull(obj);
        larvae::AssertEqual(obj->value, 42);
    });

    auto test23 = larvae::RegisterTest("StackAllocator", "DeleteCallsDestructor", []() {
        comb::StackAllocator allocator{1024};

        struct TestObject
        {
            bool* destroyed;
            TestObject(bool* d) : destroyed{d} { *destroyed = false; }
            ~TestObject() { *destroyed = true; }
        };

        bool destroyed = false;
        TestObject* obj = comb::New<TestObject>(allocator, &destroyed);

        larvae::AssertFalse(destroyed);

        comb::Delete(allocator, obj);

        larvae::AssertTrue(destroyed);
    });

    // =============================================================================
    // GetFreeMemory
    // =============================================================================

    auto test24 = larvae::RegisterTest("StackAllocator", "GetFreeMemoryReflectsUsage", []() {
        comb::StackAllocator allocator{1024};

        larvae::AssertEqual(allocator.GetFreeMemory(), 1024u);

        static_cast<void>(allocator.Allocate(200, 8));
        larvae::AssertEqual(allocator.GetFreeMemory(), 824u);

        static_cast<void>(allocator.Allocate(300, 8));
        larvae::AssertEqual(allocator.GetFreeMemory(), 524u);

        allocator.Reset();
        larvae::AssertEqual(allocator.GetFreeMemory(), 1024u);
    });

    auto test25 = larvae::RegisterTest("StackAllocator", "GetFreeMemoryWithMarkers", []() {
        comb::StackAllocator allocator{1024};

        static_cast<void>(allocator.Allocate(200, 8));
        auto marker = allocator.GetMarker();

        static_cast<void>(allocator.Allocate(400, 8));
        larvae::AssertEqual(allocator.GetFreeMemory(), 424u);

        allocator.FreeToMarker(marker);
        larvae::AssertEqual(allocator.GetFreeMemory(), 824u);
    });

    // =============================================================================
    // Move Semantics
    // =============================================================================

    auto test26 = larvae::RegisterTest("StackAllocator", "MoveConstructorTransfersOwnership", []() {
        comb::StackAllocator allocator1{1024};
        static_cast<void>(allocator1.Allocate(100, 8));

        comb::StackAllocator allocator2{std::move(allocator1)};

        larvae::AssertEqual(allocator2.GetUsedMemory(), 100u);
        larvae::AssertEqual(allocator2.GetTotalMemory(), 1024u);
        larvae::AssertStringEqual(allocator2.GetName(), "StackAllocator");
    });

    auto test27 = larvae::RegisterTest("StackAllocator", "MoveAssignmentTransfersOwnership", []() {
        comb::StackAllocator allocator1{1024};
        static_cast<void>(allocator1.Allocate(100, 8));

        comb::StackAllocator allocator2{512};

        allocator2 = std::move(allocator1);

        larvae::AssertEqual(allocator2.GetUsedMemory(), 100u);
        larvae::AssertEqual(allocator2.GetTotalMemory(), 1024u);
    });

    auto test28 = larvae::RegisterTest("StackAllocator", "MoveConstructorNullifiesSource", []() {
        comb::StackAllocator allocator1{1024};
        static_cast<void>(allocator1.Allocate(100, 8));

        comb::StackAllocator allocator2{std::move(allocator1)};

        // Source should be zeroed
        larvae::AssertEqual(allocator1.GetTotalMemory(), 0u);
        larvae::AssertEqual(allocator1.GetUsedMemory(), 0u);
    });

    // =============================================================================
    // Performance
    // =============================================================================

    auto test29 = larvae::RegisterTest("StackAllocator", "ManySmallAllocations", []() {
        comb::StackAllocator allocator{1_MB};

        for (int i = 0; i < 10000; ++i)
        {
            void* ptr = allocator.Allocate(16, 8);
            larvae::AssertNotNull(ptr);
        }

        larvae::AssertGreaterEqual(allocator.GetUsedMemory(), 160000u);
    });

    auto test30 = larvae::RegisterTest("StackAllocator", "RepeatedMarkerCycles", []() {
        comb::StackAllocator allocator{4_KB};

        for (int i = 0; i < 1000; ++i)
        {
            auto marker = allocator.GetMarker();

            void* ptr = allocator.Allocate(64, 8);
            larvae::AssertNotNull(ptr);

            allocator.FreeToMarker(marker);
        }

        larvae::AssertEqual(allocator.GetUsedMemory(), 0u);
    });
}
