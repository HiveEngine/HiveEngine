#pragma once

#include <queen/core/type_id.h>
#include <queen/core/tick.h>
#include <queen/query/query_term.h>

namespace queen
{
    /**
     * Filter mode for change detection
     */
    enum class ChangeFilterMode : uint8_t
    {
        Added,      // Only entities where component was added since last_run
        Changed,    // Only entities where component was modified since last_run
        AddedOrChanged  // Either added or changed
    };

    /**
     * Change filter term - filters entities by component tick
     *
     * Used to filter query results based on when components were added or changed.
     * This filter is applied during iteration, not during archetype matching.
     *
     * Memory layout:
     * ┌──────────────────────────────────────────────────────────────────┐
     * │ type_id: TypeId (8 bytes) - component to check                   │
     * │ mode: ChangeFilterMode (1 byte) - Added, Changed, or both        │
     * │ access: TermAccess (1 byte) - read/write access                  │
     * └──────────────────────────────────────────────────────────────────┘
     */
    struct ChangeFilterTerm
    {
        TypeId type_id = 0;
        ChangeFilterMode mode = ChangeFilterMode::Changed;
        TermAccess access = TermAccess::Read;

        [[nodiscard]] constexpr bool IsValid() const noexcept
        {
            return type_id != 0;
        }

        /**
         * Check if component ticks pass this filter
         */
        [[nodiscard]] constexpr bool Matches(ComponentTicks ticks, Tick last_run) const noexcept
        {
            switch (mode)
            {
            case ChangeFilterMode::Added:
                return ticks.WasAdded(last_run);
            case ChangeFilterMode::Changed:
                return ticks.WasChanged(last_run);
            case ChangeFilterMode::AddedOrChanged:
                return ticks.WasAddedOrChanged(last_run);
            }
            return false;
        }

        template<typename T>
        [[nodiscard]] static constexpr ChangeFilterTerm Create(
            ChangeFilterMode mode,
            TermAccess access = TermAccess::Read) noexcept
        {
            return ChangeFilterTerm{TypeIdOf<T>(), mode, access};
        }
    };

    // ─────────────────────────────────────────────────────────────
    // Change Filter Wrappers (Compile-Time DSL)
    // ─────────────────────────────────────────────────────────────

    /**
     * Added<T> - Filter for entities where component T was recently added
     *
     * Only matches entities where the component was added since the system
     * last ran (based on tick comparison).
     *
     * Use cases:
     * - Initialize newly spawned entities
     * - React to component additions
     * - One-time setup when component is first attached
     *
     * Example:
     * @code
     *   // Initialize health bar for newly added Health components
     *   world.Query<Read<Health>, Added<Health>>().Each([](const Health& h) {
     *       // Only called for entities with newly added Health
     *   });
     * @endcode
     */
    template<typename T>
    struct Added
    {
        using ComponentType = T;
        static constexpr ChangeFilterMode mode = ChangeFilterMode::Added;
        static constexpr TermAccess access = TermAccess::Read;
        static constexpr TypeId type_id = TypeIdOf<T>();

        [[nodiscard]] static constexpr ChangeFilterTerm ToChangeFilter() noexcept
        {
            return ChangeFilterTerm{type_id, mode, access};
        }

        [[nodiscard]] static constexpr Term ToTerm() noexcept
        {
            return Term{type_id, TermOperator::With, access};
        }
    };

    /**
     * Changed<T> - Filter for entities where component T was recently modified
     *
     * Only matches entities where the component was modified since the system
     * last ran (based on tick comparison).
     *
     * Use cases:
     * - React to component modifications
     * - Update derived data when source changes
     * - Optimize systems by skipping unchanged entities
     *
     * Example:
     * @code
     *   // Only update physics when transform changed
     *   world.Query<Read<Transform>, Changed<Transform>>().Each([](const Transform& t) {
     *       // Only called for entities with modified Transform
     *   });
     * @endcode
     */
    template<typename T>
    struct Changed
    {
        using ComponentType = T;
        static constexpr ChangeFilterMode mode = ChangeFilterMode::Changed;
        static constexpr TermAccess access = TermAccess::Read;
        static constexpr TypeId type_id = TypeIdOf<T>();

        [[nodiscard]] static constexpr ChangeFilterTerm ToChangeFilter() noexcept
        {
            return ChangeFilterTerm{type_id, mode, access};
        }

        [[nodiscard]] static constexpr Term ToTerm() noexcept
        {
            return Term{type_id, TermOperator::With, access};
        }
    };

    /**
     * AddedOrChanged<T> - Filter for entities where component T was added or modified
     *
     * Combines Added and Changed filters.
     *
     * Example:
     * @code
     *   world.Query<Read<Position>, AddedOrChanged<Position>>().Each([](const Position& p) {
     *       // Called when position is new or modified
     *   });
     * @endcode
     */
    template<typename T>
    struct AddedOrChanged
    {
        using ComponentType = T;
        static constexpr ChangeFilterMode mode = ChangeFilterMode::AddedOrChanged;
        static constexpr TermAccess access = TermAccess::Read;
        static constexpr TypeId type_id = TypeIdOf<T>();

        [[nodiscard]] static constexpr ChangeFilterTerm ToChangeFilter() noexcept
        {
            return ChangeFilterTerm{type_id, mode, access};
        }

        [[nodiscard]] static constexpr Term ToTerm() noexcept
        {
            return Term{type_id, TermOperator::With, access};
        }
    };

    // ─────────────────────────────────────────────────────────────
    // Change Filter Type Traits
    // ─────────────────────────────────────────────────────────────

    namespace detail
    {
        template<typename T>
        struct IsChangeFilter : std::false_type {};

        template<typename T>
        struct IsChangeFilter<Added<T>> : std::true_type {};

        template<typename T>
        struct IsChangeFilter<Changed<T>> : std::true_type {};

        template<typename T>
        struct IsChangeFilter<AddedOrChanged<T>> : std::true_type {};

        template<typename T>
        constexpr bool IsChangeFilterV = IsChangeFilter<T>::value;
    }
}
