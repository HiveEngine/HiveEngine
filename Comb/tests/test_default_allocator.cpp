#include <larvae/larvae.h>
#include <comb/default_allocator.h>
#include <comb/new.h>

namespace
{
    constexpr size_t operator""_KB(unsigned long long kb) { return kb * 1024; }
    constexpr size_t operator""_MB(unsigned long long mb) { return mb * 1024 * 1024; }

    // =============================================================================
    // DefaultAllocator Concept
    // =============================================================================

    auto test1 = larvae::RegisterTest("DefaultAllocator", "ConceptSatisfaction", []() {
        larvae::AssertTrue((comb::Allocator<comb::DefaultAllocator>));
    });

    // =============================================================================
    // DefaultAllocator Basic Usage
    // =============================================================================

    auto test2 = larvae::RegisterTest("DefaultAllocator", "BasicAllocation", []() {
        comb::BuddyAllocator buddy{1_MB};
        comb::DefaultAllocator alloc{buddy};

        void* ptr = alloc.Allocate(64, 8);
        larvae::AssertNotNull(ptr);
        larvae::AssertTrue(alloc.GetUsedMemory() > 0);

        alloc.Deallocate(ptr);
        larvae::AssertEqual(alloc.GetUsedMemory(), 0u);
    });

    auto test3 = larvae::RegisterTest("DefaultAllocator", "NewDeleteWorks", []() {
        comb::BuddyAllocator buddy{1_MB};
        comb::DefaultAllocator alloc{buddy};

        struct TestObj
        {
            int x;
            float y;
            TestObj(int a, float b) : x{a}, y{b} {}
        };

        TestObj* obj = comb::New<TestObj>(alloc, 42, 3.14f);
        larvae::AssertNotNull(obj);
        larvae::AssertEqual(obj->x, 42);
        larvae::AssertEqual(obj->y, 3.14f);

        comb::Delete(alloc, obj);
        larvae::AssertEqual(alloc.GetUsedMemory(), 0u);
    });

    // =============================================================================
    // GetDefaultAllocator (Singleton)
    // =============================================================================

    auto test4 = larvae::RegisterTest("DefaultAllocator", "GetDefaultAllocatorReturnsSameInstance", []() {
        comb::DefaultAllocator& alloc1 = comb::GetDefaultAllocator();
        comb::DefaultAllocator& alloc2 = comb::GetDefaultAllocator();

        larvae::AssertEqual(&alloc1, &alloc2);
    });

    auto test5 = larvae::RegisterTest("DefaultAllocator", "GetDefaultAllocatorIsUsable", []() {
        comb::DefaultAllocator& alloc = comb::GetDefaultAllocator();

        void* ptr = alloc.Allocate(128, 8);
        larvae::AssertNotNull(ptr);
        larvae::AssertTrue(alloc.GetUsedMemory() > 0);

        alloc.Deallocate(ptr);
    });

    auto test6 = larvae::RegisterTest("DefaultAllocator", "GetDefaultAllocatorHas32MB", []() {
        comb::DefaultAllocator& alloc = comb::GetDefaultAllocator();

        larvae::AssertEqual(alloc.GetTotalMemory(), 32_MB);
    });

    // =============================================================================
    // ModuleAllocator
    // =============================================================================

    auto test7 = larvae::RegisterTest("ModuleAllocator", "ConstructionAndBasicUsage", []() {
        comb::ModuleAllocator module{"TestModule", 1_MB};

        larvae::AssertStringEqual(module.GetName(), "TestModule");
        larvae::AssertEqual(module.GetTotalMemory(), 1_MB);
        larvae::AssertEqual(module.GetUsedMemory(), 0u);
    });

    auto test8 = larvae::RegisterTest("ModuleAllocator", "GetReturnsDefaultAllocator", []() {
        comb::ModuleAllocator module{"TestModule", 1_MB};

        comb::DefaultAllocator& alloc = module.Get();

        void* ptr = alloc.Allocate(64, 8);
        larvae::AssertNotNull(ptr);
        larvae::AssertTrue(module.GetUsedMemory() > 0);

        alloc.Deallocate(ptr);
        larvae::AssertEqual(module.GetUsedMemory(), 0u);
    });

    auto test9 = larvae::RegisterTest("ModuleAllocator", "GetUnderlyingReturnsBuddyAllocator", []() {
        comb::ModuleAllocator module{"TestModule", 1_MB};

        comb::BuddyAllocator& buddy = module.GetUnderlying();

        larvae::AssertEqual(buddy.GetTotalMemory(), 1_MB);
        larvae::AssertStringEqual(buddy.GetName(), "BuddyAllocator");
    });

    auto test10 = larvae::RegisterTest("ModuleAllocator", "ConstGetReturnsConstRef", []() {
        const comb::ModuleAllocator module{"TestModule", 1_MB};

        const comb::DefaultAllocator& alloc = module.Get();
        larvae::AssertEqual(alloc.GetTotalMemory(), 1_MB);
    });

    // =============================================================================
    // ModuleRegistry
    // =============================================================================

    auto test11 = larvae::RegisterTest("ModuleRegistry", "ModuleRegistersOnConstruction", []() {
        size_t countBefore = comb::ModuleRegistry::GetInstance().GetCount();

        {
            comb::ModuleAllocator module{"RegTestModule", 1_MB};
            size_t countDuring = comb::ModuleRegistry::GetInstance().GetCount();
            larvae::AssertEqual(countDuring, countBefore + 1);
        }

        size_t countAfter = comb::ModuleRegistry::GetInstance().GetCount();
        larvae::AssertEqual(countAfter, countBefore);
    });

    auto test12 = larvae::RegisterTest("ModuleRegistry", "ModuleUnregistersOnDestruction", []() {
        size_t countBefore = comb::ModuleRegistry::GetInstance().GetCount();

        {
            comb::ModuleAllocator module1{"RegTest1", 512_KB};
            comb::ModuleAllocator module2{"RegTest2", 512_KB};

            larvae::AssertEqual(comb::ModuleRegistry::GetInstance().GetCount(), countBefore + 2);
        }

        larvae::AssertEqual(comb::ModuleRegistry::GetInstance().GetCount(), countBefore);
    });

    auto test13 = larvae::RegisterTest("ModuleRegistry", "GetInstanceReturnsSameInstance", []() {
        comb::ModuleRegistry& reg1 = comb::ModuleRegistry::GetInstance();
        comb::ModuleRegistry& reg2 = comb::ModuleRegistry::GetInstance();

        larvae::AssertEqual(&reg1, &reg2);
    });

    auto test14 = larvae::RegisterTest("ModuleRegistry", "MultipleModulesTrackIndependently", []() {
        comb::ModuleAllocator module1{"ModA", 1_MB};
        comb::ModuleAllocator module2{"ModB", 2_MB};

        comb::DefaultAllocator& allocA = module1.Get();
        comb::DefaultAllocator& allocB = module2.Get();

        void* ptrA = allocA.Allocate(256, 8);
        void* ptrB = allocB.Allocate(512, 8);

        larvae::AssertNotNull(ptrA);
        larvae::AssertNotNull(ptrB);

        // Each module tracks independently
        larvae::AssertTrue(module1.GetUsedMemory() > 0);
        larvae::AssertTrue(module2.GetUsedMemory() > 0);

        // Different capacities
        larvae::AssertEqual(module1.GetTotalMemory(), 1_MB);
        larvae::AssertEqual(module2.GetTotalMemory(), 2_MB);

        allocA.Deallocate(ptrA);
        allocB.Deallocate(ptrB);
    });

    auto test15 = larvae::RegisterTest("ModuleRegistry", "GetEntryReturnsCorrectInfo", []() {
        comb::ModuleAllocator module{"EntryTestModule", 1_MB};

        // The new module should be the last entry
        size_t idx = comb::ModuleRegistry::GetInstance().GetCount() - 1;
        const auto& entry = comb::ModuleRegistry::GetInstance().GetEntry(idx);

        larvae::AssertStringEqual(entry.name, "EntryTestModule");
        larvae::AssertEqual(entry.allocator, &module);
    });
}
