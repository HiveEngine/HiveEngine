#include <comb/default_allocator.h>
#include <comb/memory_resource.h>

#include <larvae/larvae.h>

namespace
{
    constexpr size_t operator""_MB(unsigned long long mb)
    {
        return mb * 1024 * 1024;
    }

    // Construction

    auto test1 = larvae::RegisterTest("MemoryResource", "ConstructFromDefaultAllocator", []() {
        comb::ChainedBuddyAllocator chained{1_MB, 1_MB};
        comb::DefaultAllocator alloc{chained};

        comb::MemoryResource mr{alloc};

        larvae::AssertStringEqual(mr.GetName(), alloc.GetName());
        larvae::AssertEqual(mr.GetTotalMemory(), alloc.GetTotalMemory());
    });

    auto test2 = larvae::RegisterTest("MemoryResource", "DefaultConstructorUsesGlobalDefault", []() {
        comb::MemoryResource mr;

        comb::DefaultAllocator& global = comb::GetDefaultAllocator();
        larvae::AssertStringEqual(mr.GetName(), global.GetName());
        larvae::AssertEqual(mr.GetTotalMemory(), global.GetTotalMemory());
    });

    // Allocate / Deallocate

    auto test3 = larvae::RegisterTest("MemoryResource", "AllocateAndDeallocate", []() {
        comb::ChainedBuddyAllocator chained{1_MB, 1_MB};
        comb::DefaultAllocator alloc{chained};
        comb::MemoryResource mr{alloc};

        void* ptr = mr.Allocate(128, 8);
        larvae::AssertNotNull(ptr);
        larvae::AssertTrue(mr.GetUsedMemory() > 0);

        mr.Deallocate(ptr);
        larvae::AssertEqual(mr.GetUsedMemory(), 0u);
    });

    auto test4 = larvae::RegisterTest("MemoryResource", "AllocationReflectsOnUnderlying", []() {
        comb::ChainedBuddyAllocator chained{1_MB, 1_MB};
        comb::DefaultAllocator alloc{chained};
        comb::MemoryResource mr{alloc};

        void* ptr = mr.Allocate(256, 8);
        larvae::AssertNotNull(ptr);

        // MemoryResource and underlying allocator should agree
        larvae::AssertEqual(mr.GetUsedMemory(), alloc.GetUsedMemory());

        mr.Deallocate(ptr);
        larvae::AssertEqual(alloc.GetUsedMemory(), 0u);
    });

    // Copy (shares same underlying allocator)

    auto test5 = larvae::RegisterTest("MemoryResource", "CopySharesUnderlying", []() {
        comb::ChainedBuddyAllocator chained{1_MB, 1_MB};
        comb::DefaultAllocator alloc{chained};

        comb::MemoryResource mr1{alloc};
        comb::MemoryResource mr2{mr1};

        // Both point to same allocator, so changes visible through both
        void* ptr = mr1.Allocate(64, 8);
        larvae::AssertNotNull(ptr);
        larvae::AssertEqual(mr1.GetUsedMemory(), mr2.GetUsedMemory());

        mr2.Deallocate(ptr);
        larvae::AssertEqual(mr1.GetUsedMemory(), 0u);
    });

    auto test6 = larvae::RegisterTest("MemoryResource", "CopyAssignment", []() {
        comb::ChainedBuddyAllocator chained1{1_MB, 1_MB};
        comb::DefaultAllocator alloc1{chained1};

        comb::ChainedBuddyAllocator chained2{1_MB, 1_MB};
        comb::DefaultAllocator alloc2{chained2};

        comb::MemoryResource mr1{alloc1};
        comb::MemoryResource mr2{alloc2};

        mr2 = mr1;

        // mr2 now points to alloc1
        larvae::AssertStringEqual(mr2.GetName(), alloc1.GetName());
        larvae::AssertEqual(mr2.GetTotalMemory(), alloc1.GetTotalMemory());
    });

    // Queries

    auto test7 = larvae::RegisterTest("MemoryResource", "GetNameDelegates", []() {
        comb::ChainedBuddyAllocator chained{1_MB, 1_MB};
        comb::DefaultAllocator alloc{chained};
        comb::MemoryResource mr{alloc};

        larvae::AssertStringEqual(mr.GetName(), alloc.GetName());
    });

    auto test8 = larvae::RegisterTest("MemoryResource", "GetTotalMemoryDelegates", []() {
        comb::ChainedBuddyAllocator chained{1_MB, 1_MB};
        comb::DefaultAllocator alloc{chained};
        comb::MemoryResource mr{alloc};

        larvae::AssertEqual(mr.GetTotalMemory(), 1_MB);
    });

    auto test9 = larvae::RegisterTest("MemoryResource", "GetDefaultMemoryResource", []() {
        comb::MemoryResource mr = comb::GetDefaultMemoryResource();

        comb::DefaultAllocator& global = comb::GetDefaultAllocator();
        larvae::AssertEqual(mr.GetTotalMemory(), global.GetTotalMemory());
        larvae::AssertStringEqual(mr.GetName(), global.GetName());
    });
} // namespace
