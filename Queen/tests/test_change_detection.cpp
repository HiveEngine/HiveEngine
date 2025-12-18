#include <larvae/larvae.h>
#include <queen/core/tick.h>
#include <queen/query/change_filter.h>
#include <queen/query/mut.h>
#include <queen/world/world.h>
#include <comb/linear_allocator.h>

namespace
{
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

    // ─────────────────────────────────────────────────────────────
    // Tick Basic Tests
    // ─────────────────────────────────────────────────────────────

    auto test1 = larvae::RegisterTest("QueenChangeDetection", "TickConstruction", []() {
        queen::Tick tick{};
        larvae::AssertEqual(tick.value, uint32_t{0});

        queen::Tick tick2{42};
        larvae::AssertEqual(tick2.value, uint32_t{42});
    });

    auto test2 = larvae::RegisterTest("QueenChangeDetection", "TickIncrement", []() {
        queen::Tick tick{10};
        ++tick;
        larvae::AssertEqual(tick.value, uint32_t{11});

        queen::Tick copy = tick++;
        larvae::AssertEqual(copy.value, uint32_t{11});
        larvae::AssertEqual(tick.value, uint32_t{12});
    });

    auto test3 = larvae::RegisterTest("QueenChangeDetection", "TickIsNewerThan", []() {
        queen::Tick older{10};
        queen::Tick newer{20};

        larvae::AssertTrue(newer.IsNewerThan(older));
        larvae::AssertFalse(older.IsNewerThan(newer));
        larvae::AssertFalse(older.IsNewerThan(older));
    });

    auto test4 = larvae::RegisterTest("QueenChangeDetection", "TickWraparound", []() {
        // Test wraparound: UINT32_MAX wraps to 0
        queen::Tick almost_max{0xFFFFFFFF - 5};
        queen::Tick wrapped{5};

        // After wrap, 5 is "newer" than UINT32_MAX-5
        larvae::AssertTrue(wrapped.IsNewerThan(almost_max));
        larvae::AssertFalse(almost_max.IsNewerThan(wrapped));
    });

    auto test5 = larvae::RegisterTest("QueenChangeDetection", "TickEquality", []() {
        queen::Tick t1{100};
        queen::Tick t2{100};
        queen::Tick t3{200};

        larvae::AssertTrue(t1 == t2);
        larvae::AssertFalse(t1 == t3);
        larvae::AssertTrue(t1 != t3);
        larvae::AssertFalse(t1 != t2);
    });

    // ─────────────────────────────────────────────────────────────
    // ComponentTicks Basic Tests
    // ─────────────────────────────────────────────────────────────

    auto test6 = larvae::RegisterTest("QueenChangeDetection", "ComponentTicksConstruction", []() {
        queen::ComponentTicks ticks{};
        larvae::AssertEqual(ticks.added.value, uint32_t{0});
        larvae::AssertEqual(ticks.changed.value, uint32_t{0});
    });

    auto test7 = larvae::RegisterTest("QueenChangeDetection", "ComponentTicksFromTick", []() {
        queen::Tick tick{42};
        queen::ComponentTicks ticks{tick};

        larvae::AssertEqual(ticks.added.value, uint32_t{42});
        larvae::AssertEqual(ticks.changed.value, uint32_t{42});
    });

    auto test8 = larvae::RegisterTest("QueenChangeDetection", "ComponentTicksWasAdded", []() {
        queen::ComponentTicks ticks{queen::Tick{10}, queen::Tick{10}};

        larvae::AssertTrue(ticks.WasAdded(queen::Tick{5}));
        larvae::AssertFalse(ticks.WasAdded(queen::Tick{15}));
        larvae::AssertFalse(ticks.WasAdded(queen::Tick{10}));
    });

    auto test9 = larvae::RegisterTest("QueenChangeDetection", "ComponentTicksWasChanged", []() {
        queen::ComponentTicks ticks{queen::Tick{5}, queen::Tick{15}};

        larvae::AssertTrue(ticks.WasChanged(queen::Tick{10}));
        larvae::AssertFalse(ticks.WasChanged(queen::Tick{20}));
        larvae::AssertFalse(ticks.WasChanged(queen::Tick{15}));
    });

    auto test10 = larvae::RegisterTest("QueenChangeDetection", "ComponentTicksMarkChanged", []() {
        queen::ComponentTicks ticks{queen::Tick{5}, queen::Tick{5}};
        ticks.MarkChanged(queen::Tick{20});

        larvae::AssertEqual(ticks.added.value, uint32_t{5});
        larvae::AssertEqual(ticks.changed.value, uint32_t{20});
    });

    auto test11 = larvae::RegisterTest("QueenChangeDetection", "ComponentTicksSetAdded", []() {
        queen::ComponentTicks ticks{queen::Tick{5}, queen::Tick{5}};
        ticks.SetAdded(queen::Tick{20});

        larvae::AssertEqual(ticks.added.value, uint32_t{20});
        larvae::AssertEqual(ticks.changed.value, uint32_t{20});
    });

    auto test12 = larvae::RegisterTest("QueenChangeDetection", "ComponentTicksWasAddedOrChanged", []() {
        // Both added and changed at tick 10
        queen::ComponentTicks ticks1{queen::Tick{10}, queen::Tick{10}};
        larvae::AssertTrue(ticks1.WasAddedOrChanged(queen::Tick{5}));
        larvae::AssertFalse(ticks1.WasAddedOrChanged(queen::Tick{15}));

        // Added at 5, changed at 15
        queen::ComponentTicks ticks2{queen::Tick{5}, queen::Tick{15}};
        larvae::AssertTrue(ticks2.WasAddedOrChanged(queen::Tick{10}));  // Changed
        larvae::AssertTrue(ticks2.WasAddedOrChanged(queen::Tick{3}));   // Added
        larvae::AssertFalse(ticks2.WasAddedOrChanged(queen::Tick{20})); // Neither
    });

    // ─────────────────────────────────────────────────────────────
    // World Tick Tests
    // ─────────────────────────────────────────────────────────────

    auto test13 = larvae::RegisterTest("QueenChangeDetection", "WorldInitialTick", []() {
        comb::LinearAllocator alloc{262144};
        queen::World world{};

        // World starts at tick 1 (0 means "never changed")
        larvae::AssertEqual(world.CurrentTick().value, uint32_t{1});
    });

    auto test14 = larvae::RegisterTest("QueenChangeDetection", "WorldIncrementTick", []() {
        comb::LinearAllocator alloc{262144};
        queen::World world{};

        world.IncrementTick();
        larvae::AssertEqual(world.CurrentTick().value, uint32_t{2});

        world.IncrementTick();
        larvae::AssertEqual(world.CurrentTick().value, uint32_t{3});
    });

    auto test15 = larvae::RegisterTest("QueenChangeDetection", "WorldUpdateIncrementsTick", []() {
        comb::LinearAllocator alloc{262144};
        queen::World world{};

        uint32_t initial_tick = world.CurrentTick().value;

        world.Update();
        larvae::AssertEqual(world.CurrentTick().value, initial_tick + 1);

        world.Update();
        larvae::AssertEqual(world.CurrentTick().value, initial_tick + 2);
    });

    // ─────────────────────────────────────────────────────────────
    // Change Filter Term Tests
    // ─────────────────────────────────────────────────────────────

    auto test16 = larvae::RegisterTest("QueenChangeDetection", "ChangeFilterTermAdded", []() {
        auto filter = queen::ChangeFilterTerm::Create<Position>(queen::ChangeFilterMode::Added);

        larvae::AssertEqual(filter.type_id, queen::TypeIdOf<Position>());
        larvae::AssertTrue(filter.mode == queen::ChangeFilterMode::Added);

        // Component added at tick 10
        queen::ComponentTicks ticks{queen::Tick{10}, queen::Tick{10}};

        // Should match if last_run was before added
        larvae::AssertTrue(filter.Matches(ticks, queen::Tick{5}));
        larvae::AssertFalse(filter.Matches(ticks, queen::Tick{15}));
    });

    auto test17 = larvae::RegisterTest("QueenChangeDetection", "ChangeFilterTermChanged", []() {
        auto filter = queen::ChangeFilterTerm::Create<Position>(queen::ChangeFilterMode::Changed);

        // Component added at tick 5, changed at tick 15
        queen::ComponentTicks ticks{queen::Tick{5}, queen::Tick{15}};

        // Should match if last_run was before changed
        larvae::AssertTrue(filter.Matches(ticks, queen::Tick{10}));
        larvae::AssertFalse(filter.Matches(ticks, queen::Tick{20}));
    });

    // ─────────────────────────────────────────────────────────────
    // Added/Changed DSL Type Tests
    // ─────────────────────────────────────────────────────────────

    auto test18 = larvae::RegisterTest("QueenChangeDetection", "AddedTypeTraits", []() {
        larvae::AssertEqual(queen::Added<Position>::type_id, queen::TypeIdOf<Position>());
        larvae::AssertTrue(queen::Added<Position>::mode == queen::ChangeFilterMode::Added);
        larvae::AssertTrue(queen::Added<Position>::access == queen::TermAccess::Read);
    });

    auto test19 = larvae::RegisterTest("QueenChangeDetection", "ChangedTypeTraits", []() {
        larvae::AssertEqual(queen::Changed<Position>::type_id, queen::TypeIdOf<Position>());
        larvae::AssertTrue(queen::Changed<Position>::mode == queen::ChangeFilterMode::Changed);
        larvae::AssertTrue(queen::Changed<Position>::access == queen::TermAccess::Read);
    });

    auto test20 = larvae::RegisterTest("QueenChangeDetection", "IsChangeFilterV", []() {
        larvae::AssertTrue(queen::detail::IsChangeFilterV<queen::Added<Position>>);
        larvae::AssertTrue(queen::detail::IsChangeFilterV<queen::Changed<Position>>);
        larvae::AssertTrue(queen::detail::IsChangeFilterV<queen::AddedOrChanged<Position>>);
        larvae::AssertFalse(queen::detail::IsChangeFilterV<queen::Read<Position>>);
        larvae::AssertFalse(queen::detail::IsChangeFilterV<queen::Write<Position>>);
        larvae::AssertFalse(queen::detail::IsChangeFilterV<Position>);
    });

    // ─────────────────────────────────────────────────────────────
    // Column Ticks Tests
    // ─────────────────────────────────────────────────────────────

    auto test21 = larvae::RegisterTest("QueenChangeDetection", "ColumnStoresTicks", []() {
        comb::LinearAllocator alloc{262144};
        queen::Column<comb::LinearAllocator> column{alloc, queen::ComponentMeta::Of<Position>(), 16};

        Position pos{1.0f, 2.0f, 3.0f};
        column.PushCopy(&pos, queen::Tick{42});

        larvae::AssertEqual(column.Size(), size_t{1});
        larvae::AssertEqual(column.GetTicks(0).added.value, uint32_t{42});
        larvae::AssertEqual(column.GetTicks(0).changed.value, uint32_t{42});
    });

    auto test22 = larvae::RegisterTest("QueenChangeDetection", "ColumnMarkChanged", []() {
        comb::LinearAllocator alloc{262144};
        queen::Column<comb::LinearAllocator> column{alloc, queen::ComponentMeta::Of<Position>(), 16};

        Position pos{1.0f, 2.0f, 3.0f};
        column.PushCopy(&pos, queen::Tick{10});

        column.MarkChanged(0, queen::Tick{20});

        larvae::AssertEqual(column.GetTicks(0).added.value, uint32_t{10});
        larvae::AssertEqual(column.GetTicks(0).changed.value, uint32_t{20});
    });

    auto test23 = larvae::RegisterTest("QueenChangeDetection", "ColumnSwapRemovePreservesTicks", []() {
        comb::LinearAllocator alloc{262144};
        queen::Column<comb::LinearAllocator> column{alloc, queen::ComponentMeta::Of<Position>(), 16};

        Position pos1{1.0f, 0.0f, 0.0f};
        Position pos2{2.0f, 0.0f, 0.0f};
        Position pos3{3.0f, 0.0f, 0.0f};

        column.PushCopy(&pos1, queen::Tick{10});
        column.PushCopy(&pos2, queen::Tick{20});
        column.PushCopy(&pos3, queen::Tick{30});

        // Remove middle element - last element (pos3) moves to index 1
        column.SwapRemove(1);

        larvae::AssertEqual(column.Size(), size_t{2});
        // Index 0 unchanged
        larvae::AssertEqual(column.GetTicks(0).added.value, uint32_t{10});
        // Index 1 now has ticks from element that was at index 2
        larvae::AssertEqual(column.GetTicks(1).added.value, uint32_t{30});
    });

    auto test24 = larvae::RegisterTest("QueenChangeDetection", "ColumnTicksDataPointer", []() {
        comb::LinearAllocator alloc{262144};
        queen::Column<comb::LinearAllocator> column{alloc, queen::ComponentMeta::Of<Position>(), 16};

        Position pos1{1.0f, 0.0f, 0.0f};
        Position pos2{2.0f, 0.0f, 0.0f};

        column.PushCopy(&pos1, queen::Tick{10});
        column.PushCopy(&pos2, queen::Tick{20});

        const queen::ComponentTicks* ticks = column.TicksData();
        larvae::AssertNotNull(ticks);
        larvae::AssertEqual(ticks[0].added.value, uint32_t{10});
        larvae::AssertEqual(ticks[1].added.value, uint32_t{20});
    });

    auto test25 = larvae::RegisterTest("QueenChangeDetection", "ColumnDefaultTick", []() {
        comb::LinearAllocator alloc{262144};
        queen::Column<comb::LinearAllocator> column{alloc, queen::ComponentMeta::Of<Position>(), 16};

        Position pos{1.0f, 2.0f, 3.0f};
        // Push without tick - should default to tick 0
        column.PushCopy(&pos);

        larvae::AssertEqual(column.GetTicks(0).added.value, uint32_t{0});
    });

    // ─────────────────────────────────────────────────────────────
    // System Last Run Tick Tests
    // ─────────────────────────────────────────────────────────────

    auto test26 = larvae::RegisterTest("QueenChangeDetection", "SystemDescriptorLastRunTickInitiallyZero", []() {
        comb::LinearAllocator alloc{262144};
        queen::World world{};

        queen::SystemId id = world.System<queen::Read<Position>>("TestSystem")
            .Each([](const Position&) {});

        auto* desc = world.GetSystemStorage().GetSystem(id);
        larvae::AssertNotNull(desc);
        larvae::AssertEqual(desc->LastRunTick().value, uint32_t{0});
    });

    auto test27 = larvae::RegisterTest("QueenChangeDetection", "SystemLastRunTickUpdatedAfterExecution", []() {
        comb::LinearAllocator alloc{262144};
        queen::World world{};

        (void)world.Spawn(Position{1.0f, 2.0f, 3.0f});

        int run_count = 0;
        queen::SystemId id = world.System<queen::Read<Position>>("TestSystem")
            .Each([&](const Position&) { ++run_count; });

        larvae::AssertEqual(run_count, 0);

        // Get initial tick before Update
        uint32_t tick_before = world.CurrentTick().value;

        // Run the system via Update
        world.Update();

        larvae::AssertEqual(run_count, 1);

        // System's last_run_tick should equal the tick at start of Update
        auto* desc = world.GetSystemStorage().GetSystem(id);
        larvae::AssertEqual(desc->LastRunTick().value, tick_before + 1);
    });

    auto test28 = larvae::RegisterTest("QueenChangeDetection", "SystemLastRunTickTracksMultipleUpdates", []() {
        comb::LinearAllocator alloc{262144};
        queen::World world{};

        (void)world.Spawn(Position{1.0f, 2.0f, 3.0f});

        queen::SystemId id = world.System<queen::Read<Position>>("TestSystem")
            .Each([](const Position&) {});

        auto* desc = world.GetSystemStorage().GetSystem(id);

        // First update
        world.Update();
        uint32_t tick_after_first = desc->LastRunTick().value;

        // Second update
        world.Update();
        uint32_t tick_after_second = desc->LastRunTick().value;

        larvae::AssertTrue(tick_after_second > tick_after_first);
        larvae::AssertEqual(tick_after_second, tick_after_first + 1);
    });

    auto test29 = larvae::RegisterTest("QueenChangeDetection", "DisabledSystemDoesNotUpdateLastRunTick", []() {
        comb::LinearAllocator alloc{262144};
        queen::World world{};

        (void)world.Spawn(Position{1.0f, 2.0f, 3.0f});

        int run_count = 0;
        queen::SystemId id = world.System<queen::Read<Position>>("TestSystem")
            .Each([&](const Position&) { ++run_count; });

        // Disable the system
        world.SetSystemEnabled(id, false);

        auto* desc = world.GetSystemStorage().GetSystem(id);
        uint32_t tick_before = desc->LastRunTick().value;

        // Update should not run disabled system
        world.Update();

        larvae::AssertEqual(run_count, 0);
        larvae::AssertEqual(desc->LastRunTick().value, tick_before);
    });

    // ─────────────────────────────────────────────────────────────
    // Mut<T> Wrapper Tests
    // ─────────────────────────────────────────────────────────────

    auto test30 = larvae::RegisterTest("QueenChangeDetection", "MutDefaultConstruction", []() {
        queen::Mut<Position> mut;
        larvae::AssertFalse(static_cast<bool>(mut));
    });

    auto test31 = larvae::RegisterTest("QueenChangeDetection", "MutConstruction", []() {
        Position pos{1.0f, 2.0f, 3.0f};
        queen::ComponentTicks ticks{queen::Tick{10}};

        queen::Mut<Position> mut{&pos, &ticks, queen::Tick{20}};

        larvae::AssertTrue(static_cast<bool>(mut));
    });

    auto test32 = larvae::RegisterTest("QueenChangeDetection", "MutArrowMarksChanged", []() {
        Position pos{1.0f, 2.0f, 3.0f};
        queen::ComponentTicks ticks{queen::Tick{10}};

        larvae::AssertEqual(ticks.changed.value, uint32_t{10});

        queen::Mut<Position> mut{&pos, &ticks, queen::Tick{20}};

        // Access via arrow - should mark changed
        mut->x = 5.0f;

        larvae::AssertEqual(ticks.changed.value, uint32_t{20});
        larvae::AssertEqual(pos.x, 5.0f);
    });

    auto test33 = larvae::RegisterTest("QueenChangeDetection", "MutDerefMarksChanged", []() {
        Position pos{1.0f, 2.0f, 3.0f};
        queen::ComponentTicks ticks{queen::Tick{10}};

        queen::Mut<Position> mut{&pos, &ticks, queen::Tick{25}};

        // Access via deref - should mark changed
        Position& ref = *mut;
        ref.y = 10.0f;

        larvae::AssertEqual(ticks.changed.value, uint32_t{25});
        larvae::AssertEqual(pos.y, 10.0f);
    });

    auto test34 = larvae::RegisterTest("QueenChangeDetection", "MutGetMarksChanged", []() {
        Position pos{1.0f, 2.0f, 3.0f};
        queen::ComponentTicks ticks{queen::Tick{10}};

        queen::Mut<Position> mut{&pos, &ticks, queen::Tick{30}};

        Position* ptr = mut.Get();
        ptr->z = 15.0f;

        larvae::AssertEqual(ticks.changed.value, uint32_t{30});
        larvae::AssertEqual(pos.z, 15.0f);
    });

    auto test35 = larvae::RegisterTest("QueenChangeDetection", "MutGetReadOnlyDoesNotMarkChanged", []() {
        Position pos{1.0f, 2.0f, 3.0f};
        queen::ComponentTicks ticks{queen::Tick{10}};

        queen::Mut<Position> mut{&pos, &ticks, queen::Tick{30}};

        // Read-only access should not mark changed
        const Position* ptr = mut.GetReadOnly();
        float x = ptr->x;
        (void)x;

        // Changed tick should remain 10, not 30
        larvae::AssertEqual(ticks.changed.value, uint32_t{10});
    });

    auto test36 = larvae::RegisterTest("QueenChangeDetection", "MutConstArrowDoesNotMarkChanged", []() {
        Position pos{1.0f, 2.0f, 3.0f};
        queen::ComponentTicks ticks{queen::Tick{10}};

        const queen::Mut<Position> mut{&pos, &ticks, queen::Tick{30}};

        // Const arrow should not mark changed
        float x = mut->x;
        (void)x;

        larvae::AssertEqual(ticks.changed.value, uint32_t{10});
    });

    auto test37 = larvae::RegisterTest("QueenChangeDetection", "MutWasAddedWasChanged", []() {
        Position pos{1.0f, 2.0f, 3.0f};
        queen::ComponentTicks ticks{queen::Tick{10}, queen::Tick{20}};

        queen::Mut<Position> mut{&pos, &ticks, queen::Tick{30}};

        // Was added at tick 10
        larvae::AssertTrue(mut.WasAdded(queen::Tick{5}));
        larvae::AssertFalse(mut.WasAdded(queen::Tick{15}));

        // Was changed at tick 20
        larvae::AssertTrue(mut.WasChanged(queen::Tick{15}));
        larvae::AssertFalse(mut.WasChanged(queen::Tick{25}));
    });

    auto test38 = larvae::RegisterTest("QueenChangeDetection", "MutTypeTraits", []() {
        larvae::AssertTrue(queen::detail::IsMutV<queen::Mut<Position>>);
        larvae::AssertFalse(queen::detail::IsMutV<Position>);
        larvae::AssertFalse(queen::detail::IsMutV<queen::Read<Position>>);

        static_assert(std::is_same_v<queen::detail::UnwrapMutT<queen::Mut<Position>>, Position>);
        static_assert(std::is_same_v<queen::detail::UnwrapMutT<Position>, Position>);
    });

    // ─────────────────────────────────────────────────────────────
    // Query with Change Filters Tests
    // ─────────────────────────────────────────────────────────────

    auto test39 = larvae::RegisterTest("QueenChangeDetection", "QueryWithAddedFilter", []() {
        comb::LinearAllocator alloc{262144};
        queen::World world{};

        // Spawn at tick 1 (world starts at tick 1)
        (void)world.Spawn(Position{1.0f, 0.0f, 0.0f});
        (void)world.Spawn(Position{2.0f, 0.0f, 0.0f});

        // Increment tick to 2, spawn more
        world.IncrementTick();
        (void)world.Spawn(Position{3.0f, 0.0f, 0.0f});

        // Query with Added<Position> filter, last_run_tick=1
        // Should only see entities added after tick 1
        auto query = world.Query<queen::Read<Position>, queen::Added<Position>>();
        query.SetLastRunTick(queen::Tick{1});

        int count = 0;
        float sum = 0.0f;
        query.Each([&](const Position& pos) {
            ++count;
            sum += pos.x;
        });

        // Only the entity added at tick 2 should match
        larvae::AssertEqual(count, 1);
        larvae::AssertEqual(sum, 3.0f);
    });

    auto test40 = larvae::RegisterTest("QueenChangeDetection", "QueryWithAddedFilterMatchesAll", []() {
        comb::LinearAllocator alloc{262144};
        queen::World world{};

        // Spawn at tick 1
        (void)world.Spawn(Position{1.0f, 0.0f, 0.0f});
        (void)world.Spawn(Position{2.0f, 0.0f, 0.0f});

        // Query with Added<Position>, last_run_tick=0
        // Should see all entities (all added after tick 0)
        auto query = world.Query<queen::Read<Position>, queen::Added<Position>>();
        query.SetLastRunTick(queen::Tick{0});

        int count = 0;
        query.Each([&](const Position&) { ++count; });

        larvae::AssertEqual(count, 2);
    });

    auto test41 = larvae::RegisterTest("QueenChangeDetection", "QueryWithAddedFilterMatchesNone", []() {
        comb::LinearAllocator alloc{262144};
        queen::World world{};

        // Spawn at tick 1
        (void)world.Spawn(Position{1.0f, 0.0f, 0.0f});
        (void)world.Spawn(Position{2.0f, 0.0f, 0.0f});

        // Query with Added<Position>, last_run_tick=10
        // Should see no entities (all added before tick 10)
        auto query = world.Query<queen::Read<Position>, queen::Added<Position>>();
        query.SetLastRunTick(queen::Tick{10});

        int count = 0;
        query.Each([&](const Position&) { ++count; });

        larvae::AssertEqual(count, 0);
    });

    auto test42 = larvae::RegisterTest("QueenChangeDetection", "QueryWithChangedFilter", []() {
        comb::LinearAllocator alloc{262144};
        queen::World world{};

        // Increment world tick to 5
        world.IncrementTick(); // 2
        world.IncrementTick(); // 3
        world.IncrementTick(); // 4
        world.IncrementTick(); // 5

        // Spawn entities at tick 5
        (void)world.Spawn(Position{1.0f, 0.0f, 0.0f});
        (void)world.Spawn(Position{2.0f, 0.0f, 0.0f});
        (void)world.Spawn(Position{3.0f, 0.0f, 0.0f});

        // All entities were added at tick 5, so Changed<Position> with last_run=3
        // should see all of them (changed_tick=5 > 3)
        auto query = world.Query<queen::Read<Position>, queen::Changed<Position>>();
        query.SetLastRunTick(queen::Tick{3});

        int count = 0;
        query.Each([&](const Position&) {
            ++count;
        });

        larvae::AssertEqual(count, 3);
    });

    auto test43 = larvae::RegisterTest("QueenChangeDetection", "QueryWithoutChangeFilterIteratesAll", []() {
        comb::LinearAllocator alloc{262144};
        queen::World world{};

        (void)world.Spawn(Position{1.0f, 0.0f, 0.0f});
        (void)world.Spawn(Position{2.0f, 0.0f, 0.0f});
        (void)world.Spawn(Position{3.0f, 0.0f, 0.0f});

        // Regular query without change filters - should iterate all
        auto query = world.Query<queen::Read<Position>>();
        query.SetLastRunTick(queen::Tick{100}); // Should have no effect

        int count = 0;
        query.Each([&](const Position&) { ++count; });

        larvae::AssertEqual(count, 3);
    });

    auto test44 = larvae::RegisterTest("QueenChangeDetection", "QueryWithAddedOrChangedFilter", []() {
        comb::LinearAllocator alloc{262144};
        queen::World world{};

        // Spawn at tick 1 - both e1 and e2 added at tick 1
        (void)world.Spawn(Position{1.0f, 0.0f, 0.0f}); // e1
        (void)world.Spawn(Position{2.0f, 0.0f, 0.0f}); // e2

        // Increment tick to 5
        world.IncrementTick(); // 2
        world.IncrementTick(); // 3
        world.IncrementTick(); // 4
        world.IncrementTick(); // 5

        // Spawn e3 at tick 5
        (void)world.Spawn(Position{3.0f, 0.0f, 0.0f}); // e3

        // Query with AddedOrChanged<Position>, last_run_tick=3
        // Should see e3 (added at 5 > 3), but not e1 and e2 (added at 1)
        auto query = world.Query<queen::Read<Position>, queen::AddedOrChanged<Position>>();
        query.SetLastRunTick(queen::Tick{3});

        int count = 0;
        float sum = 0.0f;
        query.Each([&](const Position& pos) {
            ++count;
            sum += pos.x;
        });

        larvae::AssertEqual(count, 1);
        larvae::AssertEqual(sum, 3.0f);  // Just e3
    });

    auto test45 = larvae::RegisterTest("QueenChangeDetection", "QueryWithEntityAndChangeFilter", []() {
        comb::LinearAllocator alloc{262144};
        queen::World world{};

        // Spawn at tick 1
        (void)world.Spawn(Position{1.0f, 0.0f, 0.0f});

        // Increment tick to 2
        world.IncrementTick();
        queen::Entity e2 = world.Spawn(Position{2.0f, 0.0f, 0.0f});

        // EachWithEntity should also work with change filters
        auto query = world.Query<queen::Read<Position>, queen::Added<Position>>();
        query.SetLastRunTick(queen::Tick{1});

        queen::Entity found_entity;
        query.EachWithEntity([&](queen::Entity e, const Position&) {
            found_entity = e;
        });

        larvae::AssertEqual(found_entity.Index(), e2.Index());
    });
}
