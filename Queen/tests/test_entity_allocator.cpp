#include <larvae/larvae.h>
#include <queen/core/entity_allocator.h>
#include <comb/linear_allocator.h>

namespace
{
    auto test1 = larvae::RegisterTest("QueenEntityAllocator", "AllocateReturnsValidEntity", []() {
        comb::LinearAllocator alloc{4096};
        queen::EntityAllocator<comb::LinearAllocator> allocator{alloc, 100};

        queen::Entity e = allocator.Allocate();

        larvae::AssertFalse(e.IsNull());
        larvae::AssertTrue(allocator.IsAlive(e));
        larvae::AssertEqual(e.Index(), 0u);
        larvae::AssertEqual(e.Generation(), static_cast<uint16_t>(0));
    });

    auto test2 = larvae::RegisterTest("QueenEntityAllocator", "AllocateMultipleSequential", []() {
        comb::LinearAllocator alloc{4096};
        queen::EntityAllocator<comb::LinearAllocator> allocator{alloc, 100};

        queen::Entity e1 = allocator.Allocate();
        queen::Entity e2 = allocator.Allocate();
        queen::Entity e3 = allocator.Allocate();

        larvae::AssertEqual(e1.Index(), 0u);
        larvae::AssertEqual(e2.Index(), 1u);
        larvae::AssertEqual(e3.Index(), 2u);

        larvae::AssertTrue(allocator.IsAlive(e1));
        larvae::AssertTrue(allocator.IsAlive(e2));
        larvae::AssertTrue(allocator.IsAlive(e3));
    });

    auto test3 = larvae::RegisterTest("QueenEntityAllocator", "DeallocateMarksAsDead", []() {
        comb::LinearAllocator alloc{4096};
        queen::EntityAllocator<comb::LinearAllocator> allocator{alloc, 100};

        queen::Entity e = allocator.Allocate();
        larvae::AssertTrue(allocator.IsAlive(e));

        allocator.Deallocate(e);
        larvae::AssertFalse(allocator.IsAlive(e));
    });

    auto test4 = larvae::RegisterTest("QueenEntityAllocator", "RecycledEntityHasHigherGeneration", []() {
        comb::LinearAllocator alloc{4096};
        queen::EntityAllocator<comb::LinearAllocator> allocator{alloc, 100};

        queen::Entity e1 = allocator.Allocate();
        uint32_t original_index = e1.Index();

        allocator.Deallocate(e1);

        queen::Entity e2 = allocator.Allocate();

        larvae::AssertEqual(e2.Index(), original_index);
        larvae::AssertGreaterThan(e2.Generation(), e1.Generation());
    });

    auto test5 = larvae::RegisterTest("QueenEntityAllocator", "OldEntityReferenceIsInvalid", []() {
        comb::LinearAllocator alloc{4096};
        queen::EntityAllocator<comb::LinearAllocator> allocator{alloc, 100};

        queen::Entity e1 = allocator.Allocate();
        allocator.Deallocate(e1);
        queen::Entity e2 = allocator.Allocate();

        larvae::AssertFalse(allocator.IsAlive(e1));
        larvae::AssertTrue(allocator.IsAlive(e2));
    });

    auto test6 = larvae::RegisterTest("QueenEntityAllocator", "AliveCount", []() {
        comb::LinearAllocator alloc{4096};
        queen::EntityAllocator<comb::LinearAllocator> allocator{alloc, 100};

        larvae::AssertEqual(allocator.AliveCount(), size_t{0});

        queen::Entity e1 = allocator.Allocate();
        larvae::AssertEqual(allocator.AliveCount(), size_t{1});

        queen::Entity e2 = allocator.Allocate();
        larvae::AssertEqual(allocator.AliveCount(), size_t{2});

        allocator.Deallocate(e1);
        larvae::AssertEqual(allocator.AliveCount(), size_t{1});

        allocator.Deallocate(e2);
        larvae::AssertEqual(allocator.AliveCount(), size_t{0});
    });

    auto test7 = larvae::RegisterTest("QueenEntityAllocator", "FreeListRecycling", []() {
        comb::LinearAllocator alloc{4096};
        queen::EntityAllocator<comb::LinearAllocator> allocator{alloc, 100};

        queen::Entity e1 = allocator.Allocate();
        queen::Entity e2 = allocator.Allocate();
        queen::Entity e3 = allocator.Allocate();

        allocator.Deallocate(e2);
        allocator.Deallocate(e1);

        queen::Entity recycled1 = allocator.Allocate();
        queen::Entity recycled2 = allocator.Allocate();

        larvae::AssertEqual(recycled1.Index(), e1.Index());
        larvae::AssertEqual(recycled2.Index(), e2.Index());
    });

    auto test8 = larvae::RegisterTest("QueenEntityAllocator", "NullEntityIsNotAlive", []() {
        comb::LinearAllocator alloc{4096};
        queen::EntityAllocator<comb::LinearAllocator> allocator{alloc, 100};

        queen::Entity null_entity;
        larvae::AssertFalse(allocator.IsAlive(null_entity));

        queen::Entity invalid = queen::Entity::Invalid();
        larvae::AssertFalse(allocator.IsAlive(invalid));
    });

    auto test9 = larvae::RegisterTest("QueenEntityAllocator", "ManyAllocationsAndDeallocations", []() {
        comb::LinearAllocator alloc{65536};
        queen::EntityAllocator<comb::LinearAllocator> allocator{alloc, 1000};

        wax::Vector<queen::Entity, comb::LinearAllocator> entities{alloc};
        entities.Reserve(100);

        for (int i = 0; i < 100; ++i)
        {
            entities.PushBack(allocator.Allocate());
        }

        larvae::AssertEqual(allocator.AliveCount(), size_t{100});

        for (int i = 0; i < 50; ++i)
        {
            allocator.Deallocate(entities[static_cast<size_t>(i)]);
        }

        larvae::AssertEqual(allocator.AliveCount(), size_t{50});

        for (int i = 0; i < 50; ++i)
        {
            queen::Entity recycled = allocator.Allocate();
            larvae::AssertTrue(allocator.IsAlive(recycled));
        }

        larvae::AssertEqual(allocator.AliveCount(), size_t{100});
    });

    auto test10 = larvae::RegisterTest("QueenEntityAllocator", "Clear", []() {
        comb::LinearAllocator alloc{4096};
        queen::EntityAllocator<comb::LinearAllocator> allocator{alloc, 100};

        queen::Entity e1 = allocator.Allocate();
        queen::Entity e2 = allocator.Allocate();

        allocator.Clear();

        larvae::AssertEqual(allocator.AliveCount(), size_t{0});
        larvae::AssertEqual(allocator.TotalAllocated(), size_t{0});

        queen::Entity new_e = allocator.Allocate();
        larvae::AssertEqual(new_e.Index(), 0u);
    });

    auto test11 = larvae::RegisterTest("QueenEntityAllocator", "DoubleDeallocateIgnored", []() {
        comb::LinearAllocator alloc{4096};
        queen::EntityAllocator<comb::LinearAllocator> allocator{alloc, 100};

        queen::Entity e = allocator.Allocate();
        allocator.Deallocate(e);
        allocator.Deallocate(e);

        larvae::AssertEqual(allocator.FreeListSize(), size_t{1});
    });
}
