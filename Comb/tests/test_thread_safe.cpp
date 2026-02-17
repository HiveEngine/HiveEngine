#include <larvae/larvae.h>
#include <comb/thread_safe_allocator.h>
#include <comb/buddy_allocator.h>
#include <comb/linear_allocator.h>
#include <comb/new.h>
#include <thread>

namespace
{
    constexpr size_t operator""_KB(unsigned long long kb) { return kb * 1024; }
    constexpr size_t operator""_MB(unsigned long long mb) { return mb * 1024 * 1024; }

    // =============================================================================
    // Concept Satisfaction
    // =============================================================================

    auto test1 = larvae::RegisterTest("ThreadSafeAllocator", "ConceptSatisfaction", []() {
        larvae::AssertTrue((comb::Allocator<comb::ThreadSafeAllocator<comb::BuddyAllocator>>));
        larvae::AssertTrue((comb::Allocator<comb::ThreadSafeAllocator<comb::LinearAllocator>>));
    });

    // =============================================================================
    // Basic Delegation
    // =============================================================================

    auto test2 = larvae::RegisterTest("ThreadSafeAllocator", "AllocateDelegates", []() {
        comb::BuddyAllocator buddy{1_MB};
        comb::ThreadSafeAllocator<comb::BuddyAllocator> safe{buddy};

        void* ptr = safe.Allocate(64, 8);
        larvae::AssertNotNull(ptr);

        // Used memory should be reflected
        larvae::AssertTrue(safe.GetUsedMemory() > 0);

        safe.Deallocate(ptr);
        larvae::AssertEqual(safe.GetUsedMemory(), 0u);
    });

    auto test3 = larvae::RegisterTest("ThreadSafeAllocator", "DeallocateDelegates", []() {
        comb::BuddyAllocator buddy{1_MB};
        comb::ThreadSafeAllocator<comb::BuddyAllocator> safe{buddy};

        void* ptr = safe.Allocate(128, 8);
        larvae::AssertNotNull(ptr);
        larvae::AssertTrue(safe.GetUsedMemory() > 0);

        safe.Deallocate(ptr);
        larvae::AssertEqual(safe.GetUsedMemory(), 0u);
    });

    auto test4 = larvae::RegisterTest("ThreadSafeAllocator", "GetNameReturnsCorrectName", []() {
        comb::BuddyAllocator buddy{1_KB};
        comb::ThreadSafeAllocator<comb::BuddyAllocator> safe{buddy};

        larvae::AssertStringEqual(safe.GetName(), "ThreadSafeAllocator");
    });

    auto test5 = larvae::RegisterTest("ThreadSafeAllocator", "GetUsedMemoryDelegates", []() {
        comb::BuddyAllocator buddy{1_MB};
        comb::ThreadSafeAllocator<comb::BuddyAllocator> safe{buddy};

        larvae::AssertEqual(safe.GetUsedMemory(), 0u);

        void* ptr = safe.Allocate(100, 8);
        larvae::AssertNotNull(ptr);

        // ThreadSafe reports same as underlying
        larvae::AssertEqual(safe.GetUsedMemory(), buddy.GetUsedMemory());

        safe.Deallocate(ptr);
    });

    auto test6 = larvae::RegisterTest("ThreadSafeAllocator", "GetTotalMemoryDelegates", []() {
        comb::BuddyAllocator buddy{1_MB};
        comb::ThreadSafeAllocator<comb::BuddyAllocator> safe{buddy};

        larvae::AssertEqual(safe.GetTotalMemory(), buddy.GetTotalMemory());
        larvae::AssertEqual(safe.GetTotalMemory(), 1_MB);
    });

    // =============================================================================
    // Underlying Access
    // =============================================================================

    auto test7 = larvae::RegisterTest("ThreadSafeAllocator", "UnderlyingReturnsReferenceToAllocator", []() {
        comb::BuddyAllocator buddy{1_MB};
        comb::ThreadSafeAllocator<comb::BuddyAllocator> safe{buddy};

        comb::BuddyAllocator& ref = safe.Underlying();
        larvae::AssertEqual(&ref, &buddy);
    });

    auto test8 = larvae::RegisterTest("ThreadSafeAllocator", "ConstUnderlyingReturnsConstReference", []() {
        comb::BuddyAllocator buddy{1_MB};
        const comb::ThreadSafeAllocator<comb::BuddyAllocator> safe{buddy};

        const comb::BuddyAllocator& ref = safe.Underlying();
        larvae::AssertEqual(&ref, &buddy);
    });

    // =============================================================================
    // New/Delete Through Wrapper
    // =============================================================================

    auto test9 = larvae::RegisterTest("ThreadSafeAllocator", "NewDeleteWorks", []() {
        comb::BuddyAllocator buddy{1_MB};
        comb::ThreadSafeAllocator<comb::BuddyAllocator> safe{buddy};

        struct TestObject
        {
            int value;
            TestObject(int v) : value{v} {}
        };

        TestObject* obj = comb::New<TestObject>(safe, 42);
        larvae::AssertNotNull(obj);
        larvae::AssertEqual(obj->value, 42);

        comb::Delete(safe, obj);
        larvae::AssertEqual(safe.GetUsedMemory(), 0u);
    });

    auto test10 = larvae::RegisterTest("ThreadSafeAllocator", "DeleteCallsDestructor", []() {
        comb::BuddyAllocator buddy{1_MB};
        comb::ThreadSafeAllocator<comb::BuddyAllocator> safe{buddy};

        struct TestObject
        {
            bool* destroyed;
            TestObject(bool* d) : destroyed{d} { *destroyed = false; }
            ~TestObject() { *destroyed = true; }
        };

        bool destroyed = false;
        TestObject* obj = comb::New<TestObject>(safe, &destroyed);
        larvae::AssertFalse(destroyed);

        comb::Delete(safe, obj);
        larvae::AssertTrue(destroyed);
    });

    // =============================================================================
    // Move Semantics
    // =============================================================================

    auto test11 = larvae::RegisterTest("ThreadSafeAllocator", "MoveConstructor", []() {
        comb::BuddyAllocator buddy{1_MB};
        comb::ThreadSafeAllocator<comb::BuddyAllocator> safe1{buddy};

        void* ptr = safe1.Allocate(64, 8);
        larvae::AssertNotNull(ptr);

        comb::ThreadSafeAllocator<comb::BuddyAllocator> safe2{std::move(safe1)};

        // safe2 now wraps the buddy allocator
        larvae::AssertTrue(safe2.GetUsedMemory() > 0);
        larvae::AssertEqual(&safe2.Underlying(), &buddy);

        safe2.Deallocate(ptr);
        larvae::AssertEqual(safe2.GetUsedMemory(), 0u);
    });

    auto test12 = larvae::RegisterTest("ThreadSafeAllocator", "MoveAssignment", []() {
        comb::BuddyAllocator buddy1{1_MB};
        comb::BuddyAllocator buddy2{512_KB};
        comb::ThreadSafeAllocator<comb::BuddyAllocator> safe1{buddy1};
        comb::ThreadSafeAllocator<comb::BuddyAllocator> safe2{buddy2};

        void* ptr = safe1.Allocate(64, 8);
        larvae::AssertNotNull(ptr);

        safe2 = std::move(safe1);

        // safe2 now points to buddy1
        larvae::AssertEqual(&safe2.Underlying(), &buddy1);
        larvae::AssertTrue(safe2.GetUsedMemory() > 0);

        safe2.Deallocate(ptr);
    });

    // =============================================================================
    // Multiple Allocations
    // =============================================================================

    auto test13 = larvae::RegisterTest("ThreadSafeAllocator", "MultipleAllocationsAndDeallocations", []() {
        comb::BuddyAllocator buddy{1_MB};
        comb::ThreadSafeAllocator<comb::BuddyAllocator> safe{buddy};

        void* ptrs[10];
        for (int i = 0; i < 10; ++i)
        {
            ptrs[i] = safe.Allocate(64, 8);
            larvae::AssertNotNull(ptrs[i]);
        }

        larvae::AssertTrue(safe.GetUsedMemory() > 0);

        for (int i = 0; i < 10; ++i)
        {
            safe.Deallocate(ptrs[i]);
        }

        larvae::AssertEqual(safe.GetUsedMemory(), 0u);
    });

    auto test14 = larvae::RegisterTest("ThreadSafeAllocator", "OOMReturnsNull", []() {
        comb::BuddyAllocator buddy{1_KB};
        comb::ThreadSafeAllocator<comb::BuddyAllocator> safe{buddy};

        void* ptr1 = safe.Allocate(1000, 8);
        larvae::AssertNotNull(ptr1);

        void* ptr2 = safe.Allocate(64, 8);
        larvae::AssertNull(ptr2);

        safe.Deallocate(ptr1);
    });

    // =============================================================================
    // Concurrent Access
    // =============================================================================

    auto test15 = larvae::RegisterTest("ThreadSafeAllocator", "ConcurrentAllocations", []() {
        comb::BuddyAllocator buddy{16_MB};
        comb::ThreadSafeAllocator<comb::BuddyAllocator> safe{buddy};

        constexpr int kThreadCount = 4;
        constexpr int kAllocsPerThread = 100;

        void* ptrs[kThreadCount][kAllocsPerThread]{};
        bool success[kThreadCount]{};

        auto worker = [&](int threadIndex) {
            bool ok = true;
            for (int i = 0; i < kAllocsPerThread; ++i)
            {
                ptrs[threadIndex][i] = safe.Allocate(64, 8);
                if (ptrs[threadIndex][i] == nullptr)
                {
                    ok = false;
                    break;
                }
            }
            success[threadIndex] = ok;
        };

        std::thread threads[kThreadCount];
        for (int t = 0; t < kThreadCount; ++t)
        {
            threads[t] = std::thread(worker, t);
        }

        for (int t = 0; t < kThreadCount; ++t)
        {
            threads[t].join();
        }

        // All threads should succeed
        for (int t = 0; t < kThreadCount; ++t)
        {
            larvae::AssertTrue(success[t]);
        }

        // Deallocate all
        for (int t = 0; t < kThreadCount; ++t)
        {
            for (int i = 0; i < kAllocsPerThread; ++i)
            {
                if (ptrs[t][i] != nullptr)
                {
                    safe.Deallocate(ptrs[t][i]);
                }
            }
        }

        larvae::AssertEqual(safe.GetUsedMemory(), 0u);
    });

    auto test16 = larvae::RegisterTest("ThreadSafeAllocator", "ConcurrentAllocAndDealloc", []() {
        comb::BuddyAllocator buddy{16_MB};
        comb::ThreadSafeAllocator<comb::BuddyAllocator> safe{buddy};

        constexpr int kThreadCount = 4;
        constexpr int kCycles = 200;

        auto worker = [&]() {
            for (int i = 0; i < kCycles; ++i)
            {
                void* ptr = safe.Allocate(128, 8);
                if (ptr != nullptr)
                {
                    safe.Deallocate(ptr);
                }
            }
        };

        std::thread threads[kThreadCount];
        for (int t = 0; t < kThreadCount; ++t)
        {
            threads[t] = std::thread(worker);
        }

        for (int t = 0; t < kThreadCount; ++t)
        {
            threads[t].join();
        }

        larvae::AssertEqual(safe.GetUsedMemory(), 0u);
    });

    // =============================================================================
    // With LinearAllocator
    // =============================================================================

    auto test17 = larvae::RegisterTest("ThreadSafeAllocator", "WorksWithLinearAllocator", []() {
        comb::LinearAllocator linear{1_MB};
        comb::ThreadSafeAllocator<comb::LinearAllocator> safe{linear};

        void* ptr1 = safe.Allocate(64, 8);
        void* ptr2 = safe.Allocate(128, 16);

        larvae::AssertNotNull(ptr1);
        larvae::AssertNotNull(ptr2);

        larvae::AssertTrue(safe.GetUsedMemory() > 0);
        larvae::AssertEqual(safe.GetTotalMemory(), 1_MB);
    });
}
