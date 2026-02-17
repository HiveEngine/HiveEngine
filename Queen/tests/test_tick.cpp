#include <larvae/larvae.h>
#include <queen/core/tick.h>

namespace
{
    // ============================================================================
    // Tick Basic Tests
    // ============================================================================

    auto test1 = larvae::RegisterTest("QueenTick", "DefaultConstruction", []() {
        queen::Tick tick;
        larvae::AssertEqual(tick.value, uint32_t{0});
    });

    auto test2 = larvae::RegisterTest("QueenTick", "ExplicitConstruction", []() {
        queen::Tick tick{42};
        larvae::AssertEqual(tick.value, uint32_t{42});
    });

    auto test3 = larvae::RegisterTest("QueenTick", "Equality", []() {
        queen::Tick a{10};
        queen::Tick b{10};
        queen::Tick c{20};

        larvae::AssertTrue(a == b);
        larvae::AssertFalse(a != b);
        larvae::AssertFalse(a == c);
        larvae::AssertTrue(a != c);
    });

    auto test4 = larvae::RegisterTest("QueenTick", "PrefixIncrement", []() {
        queen::Tick tick{5};

        queen::Tick& result = ++tick;
        larvae::AssertEqual(tick.value, uint32_t{6});
        larvae::AssertEqual(result.value, uint32_t{6});
    });

    auto test5 = larvae::RegisterTest("QueenTick", "PostfixIncrement", []() {
        queen::Tick tick{5};

        queen::Tick old = tick++;
        larvae::AssertEqual(old.value, uint32_t{5});
        larvae::AssertEqual(tick.value, uint32_t{6});
    });

    // ============================================================================
    // Tick Comparison Tests
    // ============================================================================

    auto test6 = larvae::RegisterTest("QueenTick", "IsNewerThanBasic", []() {
        queen::Tick newer{100};
        queen::Tick older{50};

        larvae::AssertTrue(newer.IsNewerThan(older));
        larvae::AssertFalse(older.IsNewerThan(newer));
    });

    auto test7 = larvae::RegisterTest("QueenTick", "IsNewerThanSameIsFalse", []() {
        queen::Tick a{100};
        queen::Tick b{100};

        larvae::AssertFalse(a.IsNewerThan(b));
        larvae::AssertFalse(b.IsNewerThan(a));
    });

    auto test8 = larvae::RegisterTest("QueenTick", "IsAtLeastBasic", []() {
        queen::Tick newer{100};
        queen::Tick older{50};
        queen::Tick same{100};

        larvae::AssertTrue(newer.IsAtLeast(older));
        larvae::AssertFalse(older.IsAtLeast(newer));
        larvae::AssertTrue(newer.IsAtLeast(same));
    });

    auto test9 = larvae::RegisterTest("QueenTick", "IsNewerThanWraparound", []() {
        // Test tick wraparound: UINT32_MAX + 1 wraps to 0
        // If tick A = UINT32_MAX and tick B = UINT32_MAX - 10,
        // A should be newer than B
        queen::Tick a{UINT32_MAX};
        queen::Tick b{UINT32_MAX - 10};

        larvae::AssertTrue(a.IsNewerThan(b));
        larvae::AssertFalse(b.IsNewerThan(a));
    });

    auto test10 = larvae::RegisterTest("QueenTick", "IsNewerThanWraparoundAcrossBoundary", []() {
        // When tick wraps: value 5 is "newer" than UINT32_MAX - 5
        // because (5 - (UINT32_MAX-5)) = 11 as signed int32 > 0
        queen::Tick wrapped{5};
        queen::Tick before_wrap{UINT32_MAX - 5};

        larvae::AssertTrue(wrapped.IsNewerThan(before_wrap));
        larvae::AssertFalse(before_wrap.IsNewerThan(wrapped));
    });

    auto test11 = larvae::RegisterTest("QueenTick", "IsAtLeastWraparound", []() {
        queen::Tick wrapped{5};
        queen::Tick before_wrap{UINT32_MAX - 5};

        larvae::AssertTrue(wrapped.IsAtLeast(before_wrap));
        larvae::AssertFalse(before_wrap.IsAtLeast(wrapped));
    });

    auto test12 = larvae::RegisterTest("QueenTick", "IncrementWrapsAround", []() {
        queen::Tick tick{UINT32_MAX};
        ++tick;
        larvae::AssertEqual(tick.value, uint32_t{0});
    });

    auto test13 = larvae::RegisterTest("QueenTick", "ConsecutiveIncrements", []() {
        queen::Tick tick{0};
        for (uint32_t i = 0; i < 100; ++i)
        {
            larvae::AssertEqual(tick.value, i);
            ++tick;
        }
        larvae::AssertEqual(tick.value, uint32_t{100});
    });

    // ============================================================================
    // ComponentTicks Tests
    // ============================================================================

    auto test14 = larvae::RegisterTest("QueenComponentTicks", "DefaultConstruction", []() {
        queen::ComponentTicks ticks;
        larvae::AssertEqual(ticks.added.value, uint32_t{0});
        larvae::AssertEqual(ticks.changed.value, uint32_t{0});
    });

    auto test15 = larvae::RegisterTest("QueenComponentTicks", "SingleTickConstruction", []() {
        queen::ComponentTicks ticks{queen::Tick{10}};
        larvae::AssertEqual(ticks.added.value, uint32_t{10});
        larvae::AssertEqual(ticks.changed.value, uint32_t{10});
    });

    auto test16 = larvae::RegisterTest("QueenComponentTicks", "TwoTickConstruction", []() {
        queen::ComponentTicks ticks{queen::Tick{5}, queen::Tick{10}};
        larvae::AssertEqual(ticks.added.value, uint32_t{5});
        larvae::AssertEqual(ticks.changed.value, uint32_t{10});
    });

    auto test17 = larvae::RegisterTest("QueenComponentTicks", "WasAdded", []() {
        queen::Tick current{10};
        queen::ComponentTicks ticks{current};

        // Component was added at tick 10, last_run was tick 5 → was added
        larvae::AssertTrue(ticks.WasAdded(queen::Tick{5}));

        // Component was added at tick 10, last_run was tick 10 → NOT added (not newer)
        larvae::AssertFalse(ticks.WasAdded(queen::Tick{10}));

        // Component was added at tick 10, last_run was tick 15 → NOT added
        larvae::AssertFalse(ticks.WasAdded(queen::Tick{15}));
    });

    auto test18 = larvae::RegisterTest("QueenComponentTicks", "WasChanged", []() {
        queen::ComponentTicks ticks{queen::Tick{5}, queen::Tick{10}};

        larvae::AssertTrue(ticks.WasChanged(queen::Tick{8}));
        larvae::AssertFalse(ticks.WasChanged(queen::Tick{10}));
        larvae::AssertFalse(ticks.WasChanged(queen::Tick{15}));
    });

    auto test19 = larvae::RegisterTest("QueenComponentTicks", "WasAddedOrChanged", []() {
        // Added at tick 5, changed at tick 10
        queen::ComponentTicks ticks{queen::Tick{5}, queen::Tick{10}};

        // last_run=3: both added and changed are newer
        larvae::AssertTrue(ticks.WasAddedOrChanged(queen::Tick{3}));

        // last_run=7: added is NOT newer, but changed IS newer
        larvae::AssertTrue(ticks.WasAddedOrChanged(queen::Tick{7}));

        // last_run=10: neither is newer
        larvae::AssertFalse(ticks.WasAddedOrChanged(queen::Tick{10}));
    });

    auto test20 = larvae::RegisterTest("QueenComponentTicks", "MarkChanged", []() {
        queen::ComponentTicks ticks{queen::Tick{5}};
        larvae::AssertEqual(ticks.changed.value, uint32_t{5});

        ticks.MarkChanged(queen::Tick{20});
        larvae::AssertEqual(ticks.changed.value, uint32_t{20});
        // added should NOT change
        larvae::AssertEqual(ticks.added.value, uint32_t{5});
    });

    auto test21 = larvae::RegisterTest("QueenComponentTicks", "SetAdded", []() {
        queen::ComponentTicks ticks{queen::Tick{5}};

        ticks.SetAdded(queen::Tick{20});
        larvae::AssertEqual(ticks.added.value, uint32_t{20});
        larvae::AssertEqual(ticks.changed.value, uint32_t{20});
    });

    auto test22 = larvae::RegisterTest("QueenComponentTicks", "ChangeDetectionWorkflow", []() {
        // Simulate a real usage pattern:
        // 1. Component added at tick 10
        // 2. System runs at tick 12, detects "added"
        // 3. Component modified at tick 15
        // 4. System runs at tick 18, detects "changed" but not "added"

        queen::ComponentTicks ticks{queen::Tick{10}};

        // Step 2: system at tick 12, last_run=9
        larvae::AssertTrue(ticks.WasAdded(queen::Tick{9}));
        larvae::AssertTrue(ticks.WasChanged(queen::Tick{9}));

        // Step 3: mark changed at tick 15
        ticks.MarkChanged(queen::Tick{15});

        // Step 4: system at tick 18, last_run=12
        larvae::AssertFalse(ticks.WasAdded(queen::Tick{12}));
        larvae::AssertTrue(ticks.WasChanged(queen::Tick{12}));
    });

    // ============================================================================
    // Constexpr Tests
    // ============================================================================

    auto test23 = larvae::RegisterTest("QueenTick", "ConstexprOperations", []() {
        // Verify all operations are constexpr-capable
        constexpr queen::Tick a{10};
        constexpr queen::Tick b{20};

        constexpr bool newer = b.IsNewerThan(a);
        larvae::AssertTrue(newer);

        constexpr bool at_least = b.IsAtLeast(a);
        larvae::AssertTrue(at_least);

        constexpr bool eq = a == a;
        larvae::AssertTrue(eq);

        constexpr bool neq = a != b;
        larvae::AssertTrue(neq);
    });
}
