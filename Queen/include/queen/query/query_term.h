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
        WITH,    // Must have component (default)
        WITHOUT, // Must not have component
        Optional // May have component (access via pointer, nullptr if absent)
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
        READ,  // Immutable access (const T&)
        WRITE, // Mutable access (T&)
        NONE   // No data access, just filter (With<Tag>, Without<Tag>)
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
        TypeId m_typeId = 0;
        TermOperator m_op = TermOperator::WITH;
        TermAccess m_access = TermAccess::READ;

        [[nodiscard]] constexpr bool IsValid() const noexcept
        {
            return m_typeId != 0;
        }

        [[nodiscard]] constexpr bool IsRequired() const noexcept
        {
            return m_op == TermOperator::WITH;
        }

        [[nodiscard]] constexpr bool IsExcluded() const noexcept
        {
            return m_op == TermOperator::WITHOUT;
        }

        [[nodiscard]] constexpr bool IsOptional() const noexcept
        {
            return m_op == TermOperator::Optional;
        }

        [[nodiscard]] constexpr bool IsReadOnly() const noexcept
        {
            return m_access == TermAccess::READ;
        }

        [[nodiscard]] constexpr bool IsWritable() const noexcept
        {
            return m_access == TermAccess::WRITE;
        }

        [[nodiscard]] constexpr bool HasDataAccess() const noexcept
        {
            return m_access != TermAccess::NONE;
        }

        template <typename T>
        [[nodiscard]] static constexpr Term Create(TermOperator op = TermOperator::WITH,
                                                   TermAccess access = TermAccess::READ) noexcept
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
    template <typename T> struct Read
    {
        using ComponentType = T;
        static constexpr TermOperator op = TermOperator::WITH;
        static constexpr TermAccess access = TermAccess::READ;
        static constexpr TypeId typeId = TypeIdOf<T>();

        [[nodiscard]] static constexpr Term ToTerm() noexcept
        {
            return Term{typeId, op, access};
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
    template <typename T> struct Write
    {
        using ComponentType = T;
        static constexpr TermOperator op = TermOperator::WITH;
        static constexpr TermAccess access = TermAccess::WRITE;
        static constexpr TypeId typeId = TypeIdOf<T>();

        [[nodiscard]] static constexpr Term ToTerm() noexcept
        {
            return Term{typeId, op, access};
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
    template <typename T> struct With
    {
        using ComponentType = T;
        static constexpr TermOperator op = TermOperator::WITH;
        static constexpr TermAccess access = TermAccess::NONE;
        static constexpr TypeId typeId = TypeIdOf<T>();

        [[nodiscard]] static constexpr Term ToTerm() noexcept
        {
            return Term{typeId, op, access};
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
    template <typename T> struct Without
    {
        using ComponentType = T;
        static constexpr TermOperator op = TermOperator::WITHOUT;
        static constexpr TermAccess access = TermAccess::NONE;
        static constexpr TypeId typeId = TypeIdOf<T>();

        [[nodiscard]] static constexpr Term ToTerm() noexcept
        {
            return Term{typeId, op, access};
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
    template <typename T> struct Maybe
    {
        using ComponentType = T;
        static constexpr TermOperator op = TermOperator::Optional;
        static constexpr TermAccess access = TermAccess::READ;
        static constexpr TypeId typeId = TypeIdOf<T>();

        [[nodiscard]] static constexpr Term ToTerm() noexcept
        {
            return Term{typeId, op, access};
        }
    };

    /**
     * MaybeWrite<T> - Optional mutable access to component T
     *
     * Like Maybe<T> but with write access.
     */
    template <typename T> struct MaybeWrite
    {
        using ComponentType = T;
        static constexpr TermOperator op = TermOperator::Optional;
        static constexpr TermAccess access = TermAccess::WRITE;
        static constexpr TypeId typeId = TypeIdOf<T>();

        [[nodiscard]] static constexpr Term ToTerm() noexcept
        {
            return Term{typeId, op, access};
        }
    };

    // ─────────────────────────────────────────────────────────────
    // Term Type Traits
    // ─────────────────────────────────────────────────────────────

    namespace detail
    {
        template <typename T> struct IsQueryTerm : std::false_type
        {
        };

        template <typename T> struct IsQueryTerm<Read<T>> : std::true_type
        {
        };

        template <typename T> struct IsQueryTerm<Write<T>> : std::true_type
        {
        };

        template <typename T> struct IsQueryTerm<With<T>> : std::true_type
        {
        };

        template <typename T> struct IsQueryTerm<Without<T>> : std::true_type
        {
        };

        template <typename T> struct IsQueryTerm<Maybe<T>> : std::true_type
        {
        };

        template <typename T> struct IsQueryTerm<MaybeWrite<T>> : std::true_type
        {
        };

        template <typename T> constexpr bool isQueryTermV = IsQueryTerm<T>::value;

        template <typename T> struct HasDataAccess : std::bool_constant<T::access != TermAccess::NONE>
        {
        };

        template <typename T> constexpr bool hasDataAccessV = HasDataAccess<T>::value;

        template <typename T> struct IsOptionalTerm : std::bool_constant<T::op == TermOperator::Optional>
        {
        };

        template <typename T> constexpr bool isOptionalTermV = IsOptionalTerm<T>::value;
    } // namespace detail
} // namespace queen
