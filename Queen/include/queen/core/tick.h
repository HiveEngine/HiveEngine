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
        uint32_t value{0};

        constexpr Tick() noexcept = default;
        explicit constexpr Tick(uint32_t v) noexcept : value{v} {}

        /**
         * Wraparound-safe comparison
         *
         * Returns true if this tick is newer than the other tick.
         * Handles wraparound correctly assuming ticks don't differ by more than 2^31.
         */
        [[nodiscard]] constexpr bool IsNewerThan(Tick other) const noexcept
        {
            // Cast to signed to handle wraparound correctly
            return static_cast<int32_t>(value - other.value) > 0;
        }

        /**
         * Check if this tick is at least as new as other
         */
        [[nodiscard]] constexpr bool IsAtLeast(Tick other) const noexcept
        {
            return static_cast<int32_t>(value - other.value) >= 0;
        }

        constexpr Tick& operator++() noexcept
        {
            ++value;
            return *this;
        }

        constexpr Tick operator++(int) noexcept
        {
            Tick tmp = *this;
            ++value;
            return tmp;
        }

        [[nodiscard]] constexpr bool operator==(Tick other) const noexcept
        {
            return value == other.value;
        }

        [[nodiscard]] constexpr bool operator!=(Tick other) const noexcept
        {
            return value != other.value;
        }
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
        Tick added{0};
        Tick changed{0};

        constexpr ComponentTicks() noexcept = default;

        explicit constexpr ComponentTicks(Tick current_tick) noexcept
            : added{current_tick}
            , changed{current_tick}
        {
        }

        constexpr ComponentTicks(Tick added_tick, Tick changed_tick) noexcept
            : added{added_tick}
            , changed{changed_tick}
        {
        }

        /**
         * Check if component was added after the given tick
         */
        [[nodiscard]] constexpr bool WasAdded(Tick last_run) const noexcept
        {
            return added.IsNewerThan(last_run);
        }

        /**
         * Check if component was changed after the given tick
         */
        [[nodiscard]] constexpr bool WasChanged(Tick last_run) const noexcept
        {
            return changed.IsNewerThan(last_run);
        }

        /**
         * Check if component was added or changed after the given tick
         */
        [[nodiscard]] constexpr bool WasAddedOrChanged(Tick last_run) const noexcept
        {
            return WasAdded(last_run) || WasChanged(last_run);
        }

        /**
         * Mark component as changed at the given tick
         */
        constexpr void MarkChanged(Tick current_tick) noexcept
        {
            changed = current_tick;
        }

        /**
         * Set both added and changed ticks (for newly added components)
         */
        constexpr void SetAdded(Tick current_tick) noexcept
        {
            added = current_tick;
            changed = current_tick;
        }
    };
}
