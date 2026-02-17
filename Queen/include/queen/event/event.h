#pragma once

#include <queen/core/type_id.h>
#include <type_traits>
#include <cstdint>

namespace queen
{
    /**
     * Concept for types that can be used as events
     *
     * Events are plain data types that carry information between systems.
     * They must be trivially copyable for efficient queue storage and
     * trivially destructible to avoid cleanup overhead.
     *
     * Use cases:
     * - Gameplay events (DamageEvent, SpawnEvent, DeathEvent)
     * - System-to-system communication
     * - Input events (JumpPressed, AttackTriggered)
     * - Audio/VFX triggers
     *
     * Performance characteristics:
     * - Zero overhead type constraint (compile-time only)
     * - Enables memcpy-based queue operations
     * - No destructor calls needed on buffer clear
     *
     * Limitations:
     * - Cannot contain pointers to dynamic memory (would leak)
     * - Cannot contain non-trivial types (std::string, std::vector)
     * - Must be self-contained (all data inline)
     *
     * Example:
     * @code
     *   // Valid event - trivially copyable struct
     *   struct DamageEvent {
     *       Entity target;
     *       Entity source;
     *       float amount;
     *       DamageType type;
     *   };
     *   static_assert(Event<DamageEvent>);
     *
     *   // Valid event - simple data
     *   struct JumpEvent {
     *       Entity entity;
     *       float force;
     *   };
     *   static_assert(Event<JumpEvent>);
     *
     *   // Invalid - contains std::string (not trivially copyable)
     *   struct InvalidEvent {
     *       std::string message;  // ERROR: not trivially copyable
     *   };
     *   // static_assert(Event<InvalidEvent>);  // Would fail
     * @endcode
     */
    template<typename T>
    concept Event = std::is_trivially_copyable_v<T>
                 && std::is_trivially_destructible_v<T>
                 && !std::is_empty_v<T>;  // Events should carry data

    /**
     * Type-safe identifier for event types
     *
     * EventId wraps a TypeId to provide type-safe event type identification.
     * Used internally by the event system for queue lookup.
     *
     * Memory layout:
     * ┌────────────────────────────────────────┐
     * │ value_: TypeId (8 bytes)               │
     * └────────────────────────────────────────┘
     *
     * Performance characteristics:
     * - Construction: O(1) - compile-time hash
     * - Comparison: O(1) - integer comparison
     * - Hash: O(1) - direct value use
     *
     * Example:
     * @code
     *   EventId damage_id = EventIdOf<DamageEvent>();
     *   EventId spawn_id = EventIdOf<SpawnEvent>();
     *
     *   if (damage_id == EventIdOf<DamageEvent>()) {
     *       // Same event type
     *   }
     * @endcode
     */
    class EventId
    {
    public:
        constexpr EventId() noexcept : value_{0} {}
        constexpr explicit EventId(TypeId id) noexcept : value_{id} {}

        [[nodiscard]] constexpr TypeId Value() const noexcept { return value_; }

        [[nodiscard]] constexpr bool operator==(const EventId& other) const noexcept
        {
            return value_ == other.value_;
        }

        [[nodiscard]] constexpr bool IsValid() const noexcept
        {
            return value_ != 0;
        }

    private:
        TypeId value_;
    };

    /**
     * Get the EventId for an event type at compile-time
     *
     * @tparam T Event type (must satisfy Event concept)
     * @return Unique EventId for the type
     */
    template<Event T>
    [[nodiscard]] constexpr EventId EventIdOf() noexcept
    {
        return EventId{TypeIdOf<T>()};
    }

    /**
     * Type-erased metadata for an event type
     *
     * Contains size and alignment information needed for type-erased
     * event queue operations.
     *
     * Memory layout:
     * ┌────────────────────────────────────────┐
     * │ id: EventId (8 bytes)                  │
     * │ size: size_t (8 bytes)                 │
     * │ alignment: size_t (8 bytes)            │
     * └────────────────────────────────────────┘
     */
    struct EventMeta
    {
        EventId id;
        size_t size;
        size_t alignment;

        template<Event T>
        [[nodiscard]] static constexpr EventMeta Of() noexcept
        {
            return EventMeta{
                EventIdOf<T>(),
                sizeof(T),
                alignof(T)
            };
        }
    };
}
