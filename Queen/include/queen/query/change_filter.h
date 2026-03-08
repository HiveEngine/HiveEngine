#pragma once

#include <queen/core/tick.h>
#include <queen/core/type_id.h>
#include <queen/query/query_term.h>

namespace queen
{
    /**
     * Filter mode for change detection
     */
    enum class ChangeFilterMode : uint8_t
    {
        ADDED,           // Only entities where component was added since last_run
        CHANGED,         // Only entities where component was modified since last_run
        ADDED_OR_CHANGED // Either added or changed
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
        TypeId m_typeId = 0;
        ChangeFilterMode m_mode = ChangeFilterMode::CHANGED;
        TermAccess m_access = TermAccess::READ;

        [[nodiscard]] constexpr bool IsValid() const noexcept { return m_typeId != 0; }

        /**
         * Check if component ticks pass this filter
         */
        [[nodiscard]] constexpr bool Matches(ComponentTicks ticks, Tick lastRun) const noexcept {
            switch (m_mode)
            {
                case ChangeFilterMode::ADDED:
                    return ticks.WasAdded(lastRun);
                case ChangeFilterMode::CHANGED:
                    return ticks.WasChanged(lastRun);
                case ChangeFilterMode::ADDED_OR_CHANGED:
                    return ticks.WasAddedOrChanged(lastRun);
            }
            return false;
        }

        template <typename T>
        [[nodiscard]] static constexpr ChangeFilterTerm Create(ChangeFilterMode mode,
                                                               TermAccess access = TermAccess::READ) noexcept {
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
    template <typename T> struct Added
    {
        using ComponentType = T;
        static constexpr ChangeFilterMode mode = ChangeFilterMode::ADDED;
        static constexpr TermAccess access = TermAccess::READ;
        static constexpr TypeId typeId = TypeIdOf<T>();

        [[nodiscard]] static constexpr ChangeFilterTerm ToChangeFilter() noexcept {
            return ChangeFilterTerm{typeId, mode, access};
        }

        [[nodiscard]] static constexpr Term ToTerm() noexcept { return Term{typeId, TermOperator::WITH, access}; }
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
    template <typename T> struct Changed
    {
        using ComponentType = T;
        static constexpr ChangeFilterMode mode = ChangeFilterMode::CHANGED;
        static constexpr TermAccess access = TermAccess::READ;
        static constexpr TypeId typeId = TypeIdOf<T>();

        [[nodiscard]] static constexpr ChangeFilterTerm ToChangeFilter() noexcept {
            return ChangeFilterTerm{typeId, mode, access};
        }

        [[nodiscard]] static constexpr Term ToTerm() noexcept { return Term{typeId, TermOperator::WITH, access}; }
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
    template <typename T> struct AddedOrChanged
    {
        using ComponentType = T;
        static constexpr ChangeFilterMode mode = ChangeFilterMode::ADDED_OR_CHANGED;
        static constexpr TermAccess access = TermAccess::READ;
        static constexpr TypeId typeId = TypeIdOf<T>();

        [[nodiscard]] static constexpr ChangeFilterTerm ToChangeFilter() noexcept {
            return ChangeFilterTerm{typeId, mode, access};
        }

        [[nodiscard]] static constexpr Term ToTerm() noexcept { return Term{typeId, TermOperator::WITH, access}; }
    };

    // ─────────────────────────────────────────────────────────────
    // Change Filter Type Traits
    // ─────────────────────────────────────────────────────────────

    namespace detail
    {
        template <typename T> struct IsChangeFilter : std::false_type
        {
        };

        template <typename T> struct IsChangeFilter<Added<T>> : std::true_type
        {
        };

        template <typename T> struct IsChangeFilter<Changed<T>> : std::true_type
        {
        };

        template <typename T> struct IsChangeFilter<AddedOrChanged<T>> : std::true_type
        {
        };

        template <typename T> constexpr bool isChangeFilterV = IsChangeFilter<T>::value;
    } // namespace detail
} // namespace queen
