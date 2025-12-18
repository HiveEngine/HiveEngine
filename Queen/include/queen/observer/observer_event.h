#pragma once

#include <queen/core/type_id.h>
#include <type_traits>

namespace queen
{
    /**
     * Observer trigger event types for structural ECS changes
     *
     * These tag types are used to register observers that react to
     * component lifecycle events. Observers are invoked synchronously
     * when the corresponding structural change occurs.
     *
     * Three trigger types are supported:
     * - OnAdd<T>: Component T added to an entity
     * - OnRemove<T>: Component T removed from an entity
     * - OnSet<T>: Component T value modified
     *
     * Use cases:
     * - Logging component additions/removals for debugging
     * - Validating component data on modification
     * - Maintaining derived state when components change
     * - Triggering side effects (audio, VFX) on game events
     *
     * Memory layout:
     * ┌────────────────────────────────────────────────────────────────┐
     * │ OnAdd<T>/OnRemove<T>/OnSet<T> are empty tag types              │
     * │ They carry component type T at compile-time only               │
     * │ No runtime storage cost                                        │
     * └────────────────────────────────────────────────────────────────┘
     *
     * Performance characteristics:
     * - Type extraction: Compile-time (zero runtime cost)
     * - Observer matching: O(1) hash lookup by trigger+component TypeId
     *
     * Limitations:
     * - Only one component type per observer registration
     * - Cannot observe multiple triggers in single observer
     * - OnSet requires explicit change tracking (not automatic)
     *
     * Example:
     * @code
     *   // Observer triggered when Health component is added
     *   world.Observer<OnAdd<Health>>("LogSpawn")
     *       .Each([](Entity e, const Health& hp) {
     *           Log("Entity {} spawned with {} HP", e, hp.value);
     *       });
     *
     *   // Observer triggered when Health is removed
     *   world.Observer<OnRemove<Health>>("LogDeath")
     *       .Each([](Entity e) {
     *           Log("Entity {} died", e);
     *       });
     *
     *   // Observer triggered when Position is modified
     *   world.Observer<OnSet<Position>>("TrackMovement")
     *       .Each([](Entity e, const Position& pos) {
     *           UpdateSpatialIndex(e, pos);
     *       });
     * @endcode
     */

    /**
     * Trigger type for component addition
     *
     * Fired when a component of type T is added to an entity.
     * This includes:
     * - Spawn with component: world.Spawn().Add<T>()
     * - Add to existing entity: world.Commands().Get().Add<T>(entity)
     * - Archetype change that adds component
     *
     * @tparam T The component type being observed
     */
    template<typename T>
    struct OnAdd
    {
        using ComponentType = T;
        static constexpr TypeId trigger_id = TypeIdOf<OnAdd<T>>();
        static constexpr TypeId component_id = TypeIdOf<T>();
    };

    /**
     * Trigger type for component removal
     *
     * Fired when a component of type T is removed from an entity.
     * This includes:
     * - Despawn: world.Commands().Get().Despawn(entity)
     * - Remove component: world.Commands().Get().Remove<T>(entity)
     * - Archetype change that removes component
     *
     * @tparam T The component type being observed
     */
    template<typename T>
    struct OnRemove
    {
        using ComponentType = T;
        static constexpr TypeId trigger_id = TypeIdOf<OnRemove<T>>();
        static constexpr TypeId component_id = TypeIdOf<T>();
    };

    /**
     * Trigger type for component value modification
     *
     * Fired when a component of type T has its value changed.
     * This requires explicit notification:
     * - Using Mut<T> in queries (marks as modified)
     * - Using Set<T>(entity, value) commands
     *
     * @tparam T The component type being observed
     */
    template<typename T>
    struct OnSet
    {
        using ComponentType = T;
        static constexpr TypeId trigger_id = TypeIdOf<OnSet<T>>();
        static constexpr TypeId component_id = TypeIdOf<T>();
    };

    // ─────────────────────────────────────────────────────────────────
    // Trigger type detection concepts
    // ─────────────────────────────────────────────────────────────────

    namespace detail
    {
        template<typename T>
        struct IsOnAdd : std::false_type {};

        template<typename T>
        struct IsOnAdd<OnAdd<T>> : std::true_type {};

        template<typename T>
        struct IsOnRemove : std::false_type {};

        template<typename T>
        struct IsOnRemove<OnRemove<T>> : std::true_type {};

        template<typename T>
        struct IsOnSet : std::false_type {};

        template<typename T>
        struct IsOnSet<OnSet<T>> : std::true_type {};
    }

    /**
     * Concept for OnAdd trigger types
     */
    template<typename T>
    concept IsOnAddTrigger = detail::IsOnAdd<T>::value;

    /**
     * Concept for OnRemove trigger types
     */
    template<typename T>
    concept IsOnRemoveTrigger = detail::IsOnRemove<T>::value;

    /**
     * Concept for OnSet trigger types
     */
    template<typename T>
    concept IsOnSetTrigger = detail::IsOnSet<T>::value;

    /**
     * Concept for any valid observer trigger type
     */
    template<typename T>
    concept ObserverTrigger = IsOnAddTrigger<T> || IsOnRemoveTrigger<T> || IsOnSetTrigger<T>;

    // ─────────────────────────────────────────────────────────────────
    // Trigger type enumeration (for runtime identification)
    // ─────────────────────────────────────────────────────────────────

    /**
     * Runtime identifier for trigger types
     *
     * Used in ObserverStorage for fast lookup without template overhead.
     */
    enum class TriggerType : uint8_t
    {
        Add,
        Remove,
        Set
    };

    /**
     * Get runtime trigger type from compile-time trigger
     */
    template<ObserverTrigger T>
    [[nodiscard]] constexpr TriggerType GetTriggerType() noexcept
    {
        if constexpr (IsOnAddTrigger<T>)
        {
            return TriggerType::Add;
        }
        else if constexpr (IsOnRemoveTrigger<T>)
        {
            return TriggerType::Remove;
        }
        else
        {
            return TriggerType::Set;
        }
    }

    /**
     * Extract component TypeId from a trigger type
     */
    template<ObserverTrigger T>
    [[nodiscard]] constexpr TypeId GetTriggerComponentId() noexcept
    {
        return T::component_id;
    }

    // ─────────────────────────────────────────────────────────────────
    // Observer key for storage lookup
    // ─────────────────────────────────────────────────────────────────

    /**
     * Composite key for observer lookup
     *
     * Combines trigger type and component type for fast hash lookup.
     * Used by ObserverStorage to find observers matching a structural change.
     *
     * Memory layout:
     * ┌────────────────────────────────────────────────────────────────┐
     * │ trigger: TriggerType (1 byte) + padding (7 bytes)             │
     * │ component_id: TypeId (8 bytes)                                │
     * └────────────────────────────────────────────────────────────────┘
     */
    struct ObserverKey
    {
        TriggerType trigger;
        TypeId component_id;

        [[nodiscard]] constexpr bool operator==(const ObserverKey& other) const noexcept
        {
            return trigger == other.trigger && component_id == other.component_id;
        }

        [[nodiscard]] constexpr bool operator!=(const ObserverKey& other) const noexcept
        {
            return !(*this == other);
        }

        /**
         * Create key from compile-time trigger type
         */
        template<ObserverTrigger T>
        [[nodiscard]] static constexpr ObserverKey Of() noexcept
        {
            return ObserverKey{GetTriggerType<T>(), GetTriggerComponentId<T>()};
        }

        /**
         * Create key from runtime values
         */
        [[nodiscard]] static constexpr ObserverKey From(TriggerType trigger, TypeId component_id) noexcept
        {
            return ObserverKey{trigger, component_id};
        }
    };

    /**
     * Hash function for ObserverKey
     *
     * Used by HashMap for observer storage lookup.
     */
    struct ObserverKeyHash
    {
        [[nodiscard]] constexpr uint64_t operator()(const ObserverKey& key) const noexcept
        {
            // Combine trigger type and component_id using FNV-1a style mixing
            uint64_t hash = static_cast<uint64_t>(key.trigger);
            hash ^= key.component_id;
            hash *= 0x100000001b3ULL; // FNV prime
            return hash;
        }
    };
}
