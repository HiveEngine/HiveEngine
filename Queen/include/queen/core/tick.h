#pragma once

#include <cstdint>

namespace queen
{
    /**
     * Tick type for change detection
     *
     * A tick is a monotonically increasing counter used to track when
     * components were added or modified. The tick wraps around at UINT32_MAX,
     * so comparisons must be wraparound-safe.
     *
     * Memory layout:
     * ┌──────────────────────────────────────────────────────────────────┐
     * │ value: uint32_t (4 bytes)                                        │
     * └──────────────────────────────────────────────────────────────────┘
     *
     * Performance characteristics:
     * - Comparison: O(1) - single subtraction and sign check
     * - Increment: O(1) - single add
     * - Thread-safe: No (use atomic for world tick)
     *
     * Use cases:
     * - Tracking when components were added (added_tick)
     * - Tracking when components were modified (changed_tick)
     * - Comparing against system's last_run_tick for change detection
     *
     * Example:
     * @code
     *   Tick current{100};
     *   Tick last_run{95};
     *
     *   if (current.IsNewerThan(last_run)) {
     *       // Component was modified since last system run
     *   }
     * @endcode
     */
    struct Tick
    {
        uint32_t m_value{0};

        constexpr Tick() noexcept = default;
        explicit constexpr Tick(uint32_t v) noexcept
            : m_value{v} {}

        /**
         * Wraparound-safe comparison
         *
         * Returns true if this tick is newer than the other tick.
         * Handles wraparound correctly assuming ticks don't differ by more than 2^31.
         */
        [[nodiscard]] constexpr bool IsNewerThan(Tick other) const noexcept {
            // Cast to signed to handle wraparound correctly
            return static_cast<int32_t>(m_value - other.m_value) > 0;
        }

        /**
         * Check if this tick is at least as new as other
         */
        [[nodiscard]] constexpr bool IsAtLeast(Tick other) const noexcept {
            return static_cast<int32_t>(m_value - other.m_value) >= 0;
        }

        constexpr Tick& operator++() noexcept {
            ++m_value;
            return *this;
        }

        constexpr Tick operator++(int) noexcept {
            Tick tmp = *this;
            ++m_value;
            return tmp;
        }

        [[nodiscard]] constexpr bool operator==(Tick other) const noexcept { return m_value == other.m_value; }

        [[nodiscard]] constexpr bool operator!=(Tick other) const noexcept { return m_value != other.m_value; }
    };

    /**
     * Component ticks for change detection
     *
     * Tracks when a component was added and when it was last modified.
     * Used by Added<T> and Changed<T> query filters.
     *
     * Memory layout:
     * ┌──────────────────────────────────────────────────────────────────┐
     * │ added: Tick (4 bytes) - tick when component was added            │
     * │ changed: Tick (4 bytes) - tick when component was last modified  │
     * └──────────────────────────────────────────────────────────────────┘
     * Total: 8 bytes per component per entity
     *
     * Use cases:
     * - Added<T>: Check if added.IsNewerThan(last_run_tick)
     * - Changed<T>: Check if changed.IsNewerThan(last_run_tick)
     *
     * Example:
     * @code
     *   ComponentTicks ticks{current_tick, current_tick};
     *
     *   // Later, when component is modified:
     *   ticks.MarkChanged(world_tick);
     *
     *   // In system:
     *   if (ticks.WasAdded(last_run_tick)) {
     *       // Handle newly added component
     *   }
     *   if (ticks.WasChanged(last_run_tick)) {
     *       // Handle modified component
     *   }
     * @endcode
     */
    struct ComponentTicks
    {
        Tick m_added{0};
        Tick m_changed{0};

        constexpr ComponentTicks() noexcept = default;

        explicit constexpr ComponentTicks(Tick currentTick) noexcept
            : m_added{currentTick}
            , m_changed{currentTick} {}

        constexpr ComponentTicks(Tick addedTick, Tick changedTick) noexcept
            : m_added{addedTick}
            , m_changed{changedTick} {}

        /**
         * Check if component was added after the given tick
         */
        [[nodiscard]] constexpr bool WasAdded(Tick lastRun) const noexcept { return m_added.IsNewerThan(lastRun); }

        /**
         * Check if component was changed after the given tick
         */
        [[nodiscard]] constexpr bool WasChanged(Tick lastRun) const noexcept { return m_changed.IsNewerThan(lastRun); }

        /**
         * Check if component was added or changed after the given tick
         */
        [[nodiscard]] constexpr bool WasAddedOrChanged(Tick lastRun) const noexcept {
            return WasAdded(lastRun) || WasChanged(lastRun);
        }

        /**
         * Mark component as changed at the given tick
         */
        constexpr void MarkChanged(Tick currentTick) noexcept { m_changed = currentTick; }

        /**
         * Set both added and changed ticks (for newly added components)
         */
        constexpr void SetAdded(Tick currentTick) noexcept {
            m_added = currentTick;
            m_changed = currentTick;
        }
    };
} // namespace queen
