#include <larvae/larvae.h>
#include <queen/core/entity.h>
#include <queen/core/tick.h>
#include <queen/core/component_info.h>
#include <queen/storage/column.h>
#include <queen/storage/table.h>
#include <queen/command/command_buffer.h>
#include <queen/scheduler/work_stealing_deque.h>
#include <queen/event/events.h>
#include <queen/world/world.h>
#include <wax/containers/hash_set.h>
#include <comb/linear_allocator.h>

namespace
{
    // Test components
    struct Position
    {
        float x, y, z;
    };

    struct Velocity
    {
        float dx, dy, dz;
    };

    struct Health
    {
        int current, max;
    };

    // Component with non-trivial destructor to detect double-destruct
    struct Tracked
    {
        static inline int destruct_count = 0;
        int value{0};
        ~Tracked() { ++destruct_count; }
    };

    // ============================================================================
    // Regression: Entity hash excludes flags
    //
    // Bug: Entity hash previously included flags, causing entities with
    // same index+generation but different flags to hash differently.
    // This broke HashSet lookups when flags changed.
    // ============================================================================

    auto test_entity_hash_1 = larvae::RegisterTest("QueenRegression", "EntityHashExcludesFlags", []() {
        // Two entities with same index+generation but different flags
        queen::Entity e1{42, 7, queen::Entity::Flags::kAlive};
        queen::Entity e2{42, 7, queen::Entity::Flags::kAlive | queen::Entity::Flags::kDisabled};
        queen::Entity e3{42, 7, queen::Entity::Flags::kNone};

        queen::EntityHash hasher{};
        size_t h1 = hasher(e1);
        size_t h2 = hasher(e2);
        size_t h3 = hasher(e3);

        // Same index+generation must produce same hash regardless of flags
        larvae::AssertEqual(h1, h2);
        larvae::AssertEqual(h1, h3);
    });

    auto test_entity_hash_2 = larvae::RegisterTest("QueenRegression", "EntityEqualityExcludesFlags", []() {
        // operator== compares only index+generation, NOT flags
        queen::Entity e1{42, 7, queen::Entity::Flags::kAlive};
        queen::Entity e2{42, 7, queen::Entity::Flags::kDisabled};

        larvae::AssertTrue(e1 == e2);
    });

    auto test_entity_hash_3 = larvae::RegisterTest("QueenRegression", "EntityHashSetWithDifferentFlags", []() {
        comb::LinearAllocator alloc{4096};
        wax::HashSet<queen::Entity, comb::LinearAllocator> set{alloc, 16};

        queen::Entity e1{42, 7, queen::Entity::Flags::kAlive};
        queen::Entity e2{42, 7, queen::Entity::Flags::kDisabled};

        set.Insert(e1);
        set.Insert(e2);

        // Should be treated as the same entity (same index+gen)
        larvae::AssertEqual(set.Count(), size_t{1});
        larvae::AssertTrue(set.Contains(e1));
        larvae::AssertTrue(set.Contains(e2));
    });

    auto test_entity_hash_4 = larvae::RegisterTest("QueenRegression", "StdHashDelegatesToEntityHash", []() {
        queen::Entity e1{42, 7, queen::Entity::Flags::kAlive};
        queen::Entity e2{42, 7, queen::Entity::Flags::kNone};

        std::hash<queen::Entity> std_hasher{};
        queen::EntityHash queen_hasher{};

        // std::hash must produce same result as EntityHash
        larvae::AssertEqual(std_hasher(e1), queen_hasher(e1));
        // And flags should not affect the hash
        larvae::AssertEqual(std_hasher(e1), std_hasher(e2));
    });

    // ============================================================================
    // Regression: Column::SwapRemove properly destructs moved-from source
    //
    // Bug: SwapRemove used to move last element into gap but did NOT
    // destruct the moved-from source, causing resource leaks.
    // ============================================================================

    auto test_column_swap_remove = larvae::RegisterTest("QueenRegression", "ColumnSwapRemoveDestructsSource", []() {
        comb::LinearAllocator alloc{1024 * 1024};

        Tracked::destruct_count = 0;

        queen::Column<comb::LinearAllocator> column{
            alloc, queen::ComponentMeta::Of<Tracked>(), 8
        };

        Tracked t0{10};
        Tracked t1{20};
        Tracked t2{30};
        column.PushCopy(&t0);
        column.PushCopy(&t1);
        column.PushCopy(&t2);

        int before_count = Tracked::destruct_count;

        // SwapRemove index 0: moves index 2 into index 0, then destructs old index 2
        // Should call destruct on the element at index 0 (being overwritten),
        // then destruct on the moved-from source at index 2
        column.SwapRemove(0);

        int destructs_during_swap = Tracked::destruct_count - before_count;

        // Should have 2 destructions: dst (old value at index 0) + src (moved-from at index 2)
        larvae::AssertEqual(destructs_during_swap, 2);

        // Verify the moved element has the correct value
        larvae::AssertEqual(column.Get<Tracked>(0)->value, 30);
        larvae::AssertEqual(column.Size(), size_t{2});
    });

    // ============================================================================
    // Regression: Table::MoveRowTo transfers ticks
    //
    // Bug: MoveRowTo previously did not copy component ticks from source
    // to destination, losing change detection metadata on archetype transitions.
    // ============================================================================

    auto test_table_move_ticks = larvae::RegisterTest("QueenRegression", "TableMoveRowToTransfersTicks", []() {
        comb::LinearAllocator alloc{1024 * 1024};

        wax::Vector<queen::ComponentMeta, comb::LinearAllocator> metas1{alloc};
        metas1.PushBack(queen::ComponentMeta::Of<Position>());
        metas1.PushBack(queen::ComponentMeta::Of<Velocity>());

        wax::Vector<queen::ComponentMeta, comb::LinearAllocator> metas2{alloc};
        metas2.PushBack(queen::ComponentMeta::Of<Position>());
        metas2.PushBack(queen::ComponentMeta::Of<Velocity>());
        metas2.PushBack(queen::ComponentMeta::Of<Health>());

        queen::Table<comb::LinearAllocator> source{alloc, metas1, 16};
        queen::Table<comb::LinearAllocator> target{alloc, metas2, 16};

        // Add entity to source with a specific tick
        queen::Tick add_tick{42};
        queen::Entity e{1, 0, queen::Entity::Flags::kAlive};
        uint32_t src_row = source.AllocateRow(e, add_tick);

        // Modify the Position tick
        queen::Column<comb::LinearAllocator>* pos_col = source.GetColumn<Position>();
        pos_col->MarkChanged(src_row, queen::Tick{50});

        // Allocate dest row
        queen::Entity e2{2, 0, queen::Entity::Flags::kAlive};
        uint32_t dst_row = target.AllocateRow(e2);

        // Move
        size_t moved = source.MoveRowTo(src_row, target, dst_row);
        larvae::AssertEqual(moved, size_t{2}); // Position + Velocity

        // Check that ticks were transferred for Position
        queen::Column<comb::LinearAllocator>* dst_pos = target.GetColumn<Position>();
        queen::ComponentTicks& ticks = dst_pos->GetTicks(dst_row);
        larvae::AssertEqual(ticks.added.value, uint32_t{42});
        larvae::AssertEqual(ticks.changed.value, uint32_t{50});
    });

    // ============================================================================
    // Regression: Events uses index-based lookup (not dangling pointers)
    //
    // Bug: Events previously stored raw pointers to EventQueue objects.
    // When new event types were registered, the vector could reallocate,
    // invalidating those pointers. Now uses index-based lookup.
    // ============================================================================

    struct EventA { int value; };
    struct EventB { float value; };
    struct EventC { int x, y; };
    struct EventD { int data; };

    auto test_events_realloc = larvae::RegisterTest("QueenRegression", "EventsStableAfterReallocation", []() {
        comb::LinearAllocator alloc{1024 * 1024};
        queen::Events<comb::LinearAllocator> events{alloc};

        // Create several event types to potentially trigger internal reallocation
        events.Send(EventA{1});
        events.Send(EventB{2.0f});
        events.Send(EventC{3, 4});
        events.Send(EventD{5});

        // After multiple queue creations, reading the first type should still work
        auto reader_a = events.Reader<EventA>();
        size_t count_a = 0;
        for (const auto& e : reader_a)
        {
            larvae::AssertEqual(e.value, 1);
            ++count_a;
        }
        larvae::AssertEqual(count_a, size_t{1});

        // And writing more to the first type should still work
        events.Send(EventA{10});

        auto reader_a2 = events.Reader<EventA>();
        size_t count_a2 = 0;
        for (const auto& e : reader_a2)
        {
            (void)e;
            ++count_a2;
        }
        larvae::AssertEqual(count_a2, size_t{2});
    });

    // ============================================================================
    // Regression: CommandBuffer move semantics (was double-free)
    //
    // Bug: CommandBuffer move constructor/assignment did not nullify source
    // block pointers, causing double-free when both source and dest were
    // destroyed. Now properly nullifies source.
    // ============================================================================

    auto test_cmdbuf_move = larvae::RegisterTest("QueenRegression", "CommandBufferMoveNullifiesSource", []() {
        comb::LinearAllocator alloc{65536};

        queen::CommandBuffer<comb::LinearAllocator> cmd1{alloc};

        // Queue some commands to allocate data blocks
        cmd1.Spawn().With(Position{1.0f, 2.0f, 3.0f});
        cmd1.Spawn().With(Velocity{4.0f, 5.0f, 6.0f});

        larvae::AssertTrue(cmd1.CommandCount() > 0);

        // Move construct
        queen::CommandBuffer<comb::LinearAllocator> cmd2{static_cast<queen::CommandBuffer<comb::LinearAllocator>&&>(cmd1)};

        // Source should be empty after move
        larvae::AssertEqual(cmd1.CommandCount(), size_t{0});
        larvae::AssertTrue(cmd1.IsEmpty());

        // Destination should have the commands
        larvae::AssertTrue(cmd2.CommandCount() > 0);

        // Destroying both should not crash (no double-free)
    });

    auto test_cmdbuf_move_assign = larvae::RegisterTest("QueenRegression", "CommandBufferMoveAssignNullifiesSource", []() {
        comb::LinearAllocator alloc{65536};

        queen::CommandBuffer<comb::LinearAllocator> cmd1{alloc};
        queen::CommandBuffer<comb::LinearAllocator> cmd2{alloc};

        cmd1.Spawn().With(Position{1.0f, 2.0f, 3.0f});

        // Move assign
        cmd2 = static_cast<queen::CommandBuffer<comb::LinearAllocator>&&>(cmd1);

        larvae::AssertEqual(cmd1.CommandCount(), size_t{0});
        larvae::AssertTrue(cmd2.CommandCount() > 0);
    });

    // ============================================================================
    // Regression: WorkStealingDeque retired buffers (was leak)
    //
    // Bug: When WorkStealingDeque grows, old buffers were not tracked.
    // Now uses a RetiredNode linked list to track old buffers for cleanup.
    // This test verifies growth doesn't crash and all items are preserved.
    // ============================================================================

    auto test_deque_grow = larvae::RegisterTest("QueenRegression", "WorkStealingDequeGrowPreservesItems", []() {
        comb::LinearAllocator alloc{4 * 1024 * 1024};
        queen::WorkStealingDeque<int, comb::LinearAllocator> deque{alloc, 4};

        // Push more items than initial capacity to force Grow()
        constexpr int kCount = 100;
        for (int i = 0; i < kCount; ++i)
        {
            deque.Push(i);
        }

        // Pop all and verify no items lost
        int popped = 0;
        while (auto val = deque.Pop())
        {
            (void)val.value();
            ++popped;
        }

        larvae::AssertEqual(popped, kCount);
    });

    auto test_deque_grow_steal = larvae::RegisterTest("QueenRegression", "WorkStealingDequeGrowWithSteal", []() {
        comb::LinearAllocator alloc{4 * 1024 * 1024};
        queen::WorkStealingDeque<int, comb::LinearAllocator> deque{alloc, 4};

        // Push items, steal some, push more (triggers grow with active steals)
        for (int i = 0; i < 4; ++i)
        {
            deque.Push(i);
        }

        // Steal 2 items
        auto s1 = deque.Steal();
        auto s2 = deque.Steal();
        larvae::AssertTrue(s1.has_value());
        larvae::AssertTrue(s2.has_value());

        // Push more to trigger grow
        for (int i = 4; i < 20; ++i)
        {
            deque.Push(i);
        }

        // Count remaining items
        int count = 0;
        while (auto val = deque.Pop())
        {
            (void)val.value();
            ++count;
        }

        // 20 pushed - 2 stolen = 18 remaining
        larvae::AssertEqual(count, 18);
    });

    // ============================================================================
    // Regression: Hierarchy cycle detection + bounded loops
    //
    // Bug: SetParent did not check for cycles. Setting child as parent
    // of its ancestor would create infinite loops in traversal.
    // Now asserts if cycle would be created.
    // Also: IsDescendantOf/GetRoot/GetDepth use bounded loops (kMaxHierarchyDepth).
    // ============================================================================

    auto test_hierarchy_cycle_1 = larvae::RegisterTest("QueenRegression", "IsDescendantOfDetectsChain", []() {
        queen::World world;

        // Build chain A -> B -> C -> D
        queen::Entity a = world.Spawn(Position{0.0f, 0.0f, 0.0f});
        queen::Entity b = world.Spawn(Position{1.0f, 0.0f, 0.0f});
        queen::Entity c = world.Spawn(Position{2.0f, 0.0f, 0.0f});
        queen::Entity d = world.Spawn(Position{3.0f, 0.0f, 0.0f});

        world.SetParent(b, a);
        world.SetParent(c, b);
        world.SetParent(d, c);

        // D is descendant of A, B, C — but NOT of D itself
        larvae::AssertTrue(world.IsDescendantOf(d, a));
        larvae::AssertTrue(world.IsDescendantOf(d, b));
        larvae::AssertTrue(world.IsDescendantOf(d, c));
        larvae::AssertFalse(world.IsDescendantOf(d, d));

        // A is not descendant of any of its children
        larvae::AssertFalse(world.IsDescendantOf(a, b));
        larvae::AssertFalse(world.IsDescendantOf(a, c));
        larvae::AssertFalse(world.IsDescendantOf(a, d));
    });

    auto test_hierarchy_no_cycle = larvae::RegisterTest("QueenRegression", "SetParentDoesNotCreateCycle", []() {
        queen::World world;

        // Create chain: A -> B -> C
        queen::Entity a = world.Spawn(Position{0.0f, 0.0f, 0.0f});
        queen::Entity b = world.Spawn(Position{1.0f, 0.0f, 0.0f});
        queen::Entity c = world.Spawn(Position{2.0f, 0.0f, 0.0f});

        world.SetParent(b, a);
        world.SetParent(c, b);

        // Verify the chain
        larvae::AssertTrue(world.IsDescendantOf(c, a));
        larvae::AssertTrue(world.IsDescendantOf(b, a));
        larvae::AssertFalse(world.IsDescendantOf(a, c));

        // GetRoot from any node returns A
        larvae::AssertTrue(world.GetRoot(c) == a);
        larvae::AssertTrue(world.GetRoot(b) == a);
        larvae::AssertTrue(world.GetRoot(a) == a);
    });

    auto test_hierarchy_bounded_depth = larvae::RegisterTest("QueenRegression", "GetDepthBounded", []() {
        queen::World world;

        // Build a moderately deep chain to verify bounded traversal works
        constexpr size_t kDepth = 50;
        queen::Entity entities[kDepth];
        entities[0] = world.Spawn(Position{0.0f, 0.0f, 0.0f});

        for (size_t i = 1; i < kDepth; ++i)
        {
            entities[i] = world.Spawn(Position{static_cast<float>(i), 0.0f, 0.0f});
            world.SetParent(entities[i], entities[i - 1]);
        }

        // Depth should be computed correctly
        larvae::AssertEqual(world.GetDepth(entities[0]), uint32_t{0});
        larvae::AssertEqual(world.GetDepth(entities[kDepth - 1]), static_cast<uint32_t>(kDepth - 1));

        // Root traversal from deepest node
        larvae::AssertTrue(world.GetRoot(entities[kDepth - 1]) == entities[0]);
    });
}
