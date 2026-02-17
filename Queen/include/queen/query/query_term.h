#pragma once

#include <queen/core/type_id.h>
#include <cstdint>

namespace queen
{
    /**
     * Query term operator - defines how a component is matched
     *
     * Operators control the matching logic for query terms:
     * - With: Entity must have the component (required)
     * - Without: Entity must not have the component (excluded)
     * - Optional: Entity may or may not have the component
     */
    enum class TermOperator : uint8_t
    {
        With,       // Must have component (default)
        Without,    // Must not have component
        Optional    // May have component (access via pointer, nullptr if absent)
    };

    /**
     * Query term access mode - defines read/write access
     *
     * Access modes are used for:
     * - Compile-time dependency analysis (parallel scheduling)
     * - Determining const vs mutable access
     */
    enum class TermAccess : uint8_t
    {
        Read,   // Immutable access (const T&)
        Write,  // Mutable access (T&)
        None    // No data access, just filter (With<Tag>, Without<Tag>)
    };

    /**
     * Query term - a single condition in a query
     *
     * Represents one component requirement in a query. Combined with other
     * terms to form a complete query that matches archetypes.
     *
     * Example:
     * @code
     *   // Query for entities with Position (read) and Velocity (write), without Dead
     *   Term terms[] = {
     *       Term::Create<Position>(TermOperator::With, TermAccess::Read),
     *       Term::Create<Velocity>(TermOperator::With, TermAccess::Write),
     *       Term::Create<Dead>(TermOperator::Without, TermAccess::None)
     *   };
     * @endcode
     */
    struct Term
    {
        TypeId type_id = 0;
        TermOperator op = TermOperator::With;
        TermAccess access = TermAccess::Read;

        [[nodiscard]] constexpr bool IsValid() const noexcept
        {
            return type_id != 0;
        }

        [[nodiscard]] constexpr bool IsRequired() const noexcept
        {
            return op == TermOperator::With;
        }

        [[nodiscard]] constexpr bool IsExcluded() const noexcept
        {
            return op == TermOperator::Without;
        }

        [[nodiscard]] constexpr bool IsOptional() const noexcept
        {
            return op == TermOperator::Optional;
        }

        [[nodiscard]] constexpr bool IsReadOnly() const noexcept
        {
            return access == TermAccess::Read;
        }

        [[nodiscard]] constexpr bool IsWritable() const noexcept
        {
            return access == TermAccess::Write;
        }

        [[nodiscard]] constexpr bool HasDataAccess() const noexcept
        {
            return access != TermAccess::None;
        }

        template<typename T>
        [[nodiscard]] static constexpr Term Create(
            TermOperator op = TermOperator::With,
            TermAccess access = TermAccess::Read) noexcept
        {
            return Term{TypeIdOf<T>(), op, access};
        }
    };

    // ─────────────────────────────────────────────────────────────
    // Query Term Wrappers (Compile-Time DSL)
    // ─────────────────────────────────────────────────────────────

    /**
     * Read<T> - Request read-only access to component T
     *
     * Used in query template parameters to indicate immutable access.
     * Entities must have this component.
     *
     * Example:
     * @code
     *   world.Query<Read<Position>, Read<Velocity>>().Each(...);
     * @endcode
     */
    template<typename T>
    struct Read
    {
        using ComponentType = T;
        static constexpr TermOperator op = TermOperator::With;
        static constexpr TermAccess access = TermAccess::Read;
        static constexpr TypeId type_id = TypeIdOf<T>();

        [[nodiscard]] static constexpr Term ToTerm() noexcept
        {
            return Term{type_id, op, access};
        }
    };

    /**
     * Write<T> - Request mutable access to component T
     *
     * Used in query template parameters to indicate mutable access.
     * Entities must have this component. Creates a write dependency
     * for parallel scheduling analysis.
     *
     * Example:
     * @code
     *   world.Query<Read<Position>, Write<Velocity>>().Each([](const Position& p, Velocity& v) {
     *       v.x += p.x * 0.1f;
     *   });
     * @endcode
     */
    template<typename T>
    struct Write
    {
        using ComponentType = T;
        static constexpr TermOperator op = TermOperator::With;
        static constexpr TermAccess access = TermAccess::Write;
        static constexpr TypeId type_id = TypeIdOf<T>();

        [[nodiscard]] static constexpr Term ToTerm() noexcept
        {
            return Term{type_id, op, access};
        }
    };

    /**
     * With<T> - Filter for entities that have component T (no data access)
     *
     * Used to filter entities without accessing the component data.
     * Useful for tag components or when you only need to check presence.
     *
     * Example:
     * @code
     *   // All entities with Player tag
     *   world.Query<Read<Position>, With<Player>>().Each([](const Position& p) {
     *       // Player tag not accessible, just used as filter
     *   });
     * @endcode
     */
    template<typename T>
    struct With
    {
        using ComponentType = T;
        static constexpr TermOperator op = TermOperator::With;
        static constexpr TermAccess access = TermAccess::None;
        static constexpr TypeId type_id = TypeIdOf<T>();

        [[nodiscard]] static constexpr Term ToTerm() noexcept
        {
            return Term{type_id, op, access};
        }
    };

    /**
     * Without<T> - Filter for entities that do NOT have component T
     *
     * Used to exclude entities that have a specific component.
     *
     * Example:
     * @code
     *   // All alive entities (no Dead component)
     *   world.Query<Read<Position>, Without<Dead>>().Each([](const Position& p) {
     *       // Process living entities
     *   });
     * @endcode
     */
    template<typename T>
    struct Without
    {
        using ComponentType = T;
        static constexpr TermOperator op = TermOperator::Without;
        static constexpr TermAccess access = TermAccess::None;
        static constexpr TypeId type_id = TypeIdOf<T>();

        [[nodiscard]] static constexpr Term ToTerm() noexcept
        {
            return Term{type_id, op, access};
        }
    };

    /**
     * Maybe<T> - Optional read access to component T
     *
     * Used when a component is optional. Access is provided via pointer
     * that may be nullptr if the entity doesn't have the component.
     *
     * Example:
     * @code
     *   world.Query<Read<Position>, Maybe<Health>>().Each([](const Position& p, const Health* h) {
     *       if (h != nullptr) {
     *           // Entity has Health
     *       }
     *   });
     * @endcode
     */
    template<typename T>
    struct Maybe
    {
        using ComponentType = T;
        static constexpr TermOperator op = TermOperator::Optional;
        static constexpr TermAccess access = TermAccess::Read;
        static constexpr TypeId type_id = TypeIdOf<T>();

        [[nodiscard]] static constexpr Term ToTerm() noexcept
        {
            return Term{type_id, op, access};
        }
    };

    /**
     * MaybeWrite<T> - Optional mutable access to component T
     *
     * Like Maybe<T> but with write access.
     */
    template<typename T>
    struct MaybeWrite
    {
        using ComponentType = T;
        static constexpr TermOperator op = TermOperator::Optional;
        static constexpr TermAccess access = TermAccess::Write;
        static constexpr TypeId type_id = TypeIdOf<T>();

        [[nodiscard]] static constexpr Term ToTerm() noexcept
        {
            return Term{type_id, op, access};
        }
    };

    // ─────────────────────────────────────────────────────────────
    // Term Type Traits
    // ─────────────────────────────────────────────────────────────

    namespace detail
    {
        template<typename T>
        struct IsQueryTerm : std::false_type {};

        template<typename T>
        struct IsQueryTerm<Read<T>> : std::true_type {};

        template<typename T>
        struct IsQueryTerm<Write<T>> : std::true_type {};

        template<typename T>
        struct IsQueryTerm<With<T>> : std::true_type {};

        template<typename T>
        struct IsQueryTerm<Without<T>> : std::true_type {};

        template<typename T>
        struct IsQueryTerm<Maybe<T>> : std::true_type {};

        template<typename T>
        struct IsQueryTerm<MaybeWrite<T>> : std::true_type {};

        template<typename T>
        constexpr bool IsQueryTermV = IsQueryTerm<T>::value;

        template<typename T>
        struct HasDataAccess : std::bool_constant<T::access != TermAccess::None> {};

        template<typename T>
        constexpr bool HasDataAccessV = HasDataAccess<T>::value;

        template<typename T>
        struct IsOptionalTerm : std::bool_constant<T::op == TermOperator::Optional> {};

        template<typename T>
        constexpr bool IsOptionalTermV = IsOptionalTerm<T>::value;
    }
}
