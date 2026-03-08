#pragma once

#include <comb/allocator_concepts.h>

#include <queen/core/entity.h>
#include <queen/observer/observer_event.h>

#include <cstdint>
#include <cstring>

namespace queen
{
    class World;

    /**
     * Unique identifier for observers
     *
     * ObserverId is a simple wrapper around a 32-bit integer for type safety.
     * Used to reference observers for enable/disable operations.
     */
    class ObserverId
    {
    public:
        constexpr ObserverId() noexcept
            : m_value{0} {}
        constexpr explicit ObserverId(uint32_t id) noexcept
            : m_value{id} {}

        [[nodiscard]] constexpr uint32_t Value() const noexcept { return m_value; }
        [[nodiscard]] constexpr bool IsValid() const noexcept { return m_value != 0; }

        [[nodiscard]] constexpr bool operator==(ObserverId other) const noexcept { return m_value == other.m_value; }

    private:
        uint32_t m_value;
    };

    /**
     * Type-erased observer callback function
     *
     * The callback receives:
     * - world: Reference to the World for component access
     * - entity: The entity being observed
     * - component: Pointer to the component data (may be nullptr for OnRemove after destruction)
     * - user_data: User-provided closure data
     */
    using ObserverCallbackFn = void (*)(World& world, Entity entity, const void* component, void* userData);

    /**
     * Reactive callback triggered by structural ECS changes
     *
     * Observer is a descriptor that stores all metadata needed to execute
     * an observer callback when a matching structural change occurs.
     * Observers are invoked synchronously at the point of change.
     *
     * Trigger points:
     * - OnAdd: Called after component is added and initialized
     * - OnRemove: Called before component is destroyed
     * - OnSet: Called after component value is modified
     *
     * Use cases:
     * - Logging component additions/removals for debugging
     * - Validating component data on modification
     * - Maintaining derived state (spatial indices, caches)
     * - Triggering side effects (audio, VFX, network sync)
     *
     * Memory layout:
     * ┌────────────────────────────────────────────────────────────────┐
     * │ id_: ObserverId (4 bytes)                                      │
     * │ trigger_: TriggerType (1 byte)                                 │
     * │ enabled_: bool (1 byte)                                        │
     * │ padding (2 bytes)                                              │
     * │ component_id_: TypeId (8 bytes)                                │
     * │ callback_fn_: function pointer (8 bytes)                       │
     * │ user_data_: void* (8 bytes)                                    │
     * │ destructor_fn_: function pointer (8 bytes)                     │
     * │ allocator_: Allocator* (8 bytes)                               │
     * │ name_: char[64] (64 bytes)                                     │
     * └────────────────────────────────────────────────────────────────┘
     * Total: ~112 bytes
     *
     * Performance characteristics:
     * - Trigger: O(1) - direct function call
     * - Enable/disable: O(1) - flag toggle
     * - Name lookup: O(n) - linear search (use ObserverId for fast access)
     *
     * Limitations:
     * - Observer name limited to 63 characters
     * - Synchronous execution (blocks structural change)
     * - Cannot safely spawn/despawn during callback (use Commands)
     * - One component type per observer
     *
     * Example:
     * @code
     *   // Created via World::Observer<>()
     *   ObserverId id = world.Observer<OnAdd<Health>>("LogSpawn")
     *       .Each([](Entity e, const Health& hp) {
     *           Log("Entity {} has {} HP", e.Index(), hp.value);
     *       });
     *
     *   // Disable observer
     *   world.SetObserverEnabled(id, false);
     * @endcode
     */
    template <comb::Allocator Allocator> class Observer
    {
    public:
        static constexpr size_t kMaxNameLength = 63;
        static constexpr size_t kMaxFilters = 4;

        Observer(Allocator& allocator, ObserverId id, const char* name, TriggerType trigger, TypeId componentId)
            : m_id{id}
            , m_trigger{trigger}
            , m_enabled{true}
            , m_filterCount{0}
            , m_componentId{componentId}
            , m_callbackFn{nullptr}
            , m_userData{nullptr}
            , m_destructorFn{nullptr}
            , m_allocator{&allocator} {
            if (name != nullptr)
            {
                size_t len = std::strlen(name);
                if (len > kMaxNameLength)
                    len = kMaxNameLength;
                std::memcpy(m_name, name, len);
                m_name[len] = '\0';
            }
            else
            {
                m_name[0] = '\0';
            }
        }

        ~Observer() {
            if (m_userData != nullptr)
            {
                if (m_destructorFn != nullptr)
                {
                    m_destructorFn(m_userData);
                }
                m_allocator->Deallocate(m_userData);
            }
        }

        Observer(const Observer&) = delete;
        Observer& operator=(const Observer&) = delete;

        Observer(Observer&& other) noexcept
            : m_id{other.m_id}
            , m_trigger{other.m_trigger}
            , m_enabled{other.m_enabled}
            , m_filterCount{other.m_filterCount}
            , m_componentId{other.m_componentId}
            , m_callbackFn{other.m_callbackFn}
            , m_userData{other.m_userData}
            , m_destructorFn{other.m_destructorFn}
            , m_allocator{other.m_allocator} {
            std::memcpy(m_name, other.m_name, sizeof(m_name));
            std::memcpy(m_filterIds, other.m_filterIds, sizeof(TypeId) * m_filterCount);
            other.m_userData = nullptr;
            other.m_destructorFn = nullptr;
        }

        Observer& operator=(Observer&& other) noexcept {
            if (this != &other)
            {
                if (m_userData != nullptr)
                {
                    if (m_destructorFn != nullptr)
                    {
                        m_destructorFn(m_userData);
                    }
                    m_allocator->Deallocate(m_userData);
                }

                m_id = other.m_id;
                m_trigger = other.m_trigger;
                m_enabled = other.m_enabled;
                m_filterCount = other.m_filterCount;
                m_componentId = other.m_componentId;
                std::memcpy(m_name, other.m_name, sizeof(m_name));
                std::memcpy(m_filterIds, other.m_filterIds, sizeof(TypeId) * m_filterCount);
                m_callbackFn = other.m_callbackFn;
                m_userData = other.m_userData;
                m_destructorFn = other.m_destructorFn;
                m_allocator = other.m_allocator;

                other.m_userData = nullptr;
                other.m_destructorFn = nullptr;
            }
            return *this;
        }

        // ─────────────────────────────────────────────────────────────────
        // Accessors
        // ─────────────────────────────────────────────────────────────────

        [[nodiscard]] ObserverId Id() const noexcept { return m_id; }
        [[nodiscard]] const char* Name() const noexcept { return m_name; }
        [[nodiscard]] TriggerType Trigger() const noexcept { return m_trigger; }
        [[nodiscard]] TypeId ComponentId() const noexcept { return m_componentId; }
        [[nodiscard]] bool IsEnabled() const noexcept { return m_enabled; }

        [[nodiscard]] ObserverKey Key() const noexcept { return ObserverKey{m_trigger, m_componentId}; }

        // ─────────────────────────────────────────────────────────────────
        // Mutators
        // ─────────────────────────────────────────────────────────────────

        void SetEnabled(bool enabled) noexcept { m_enabled = enabled; }

        void AddFilter(TypeId typeId) noexcept {
            if (m_filterCount < kMaxFilters)
            {
                m_filterIds[m_filterCount++] = typeId;
            }
        }

        [[nodiscard]] bool HasFilters() const noexcept { return m_filterCount > 0; }
        [[nodiscard]] uint8_t FilterCount() const noexcept { return m_filterCount; }
        [[nodiscard]] TypeId FilterId(uint8_t index) const noexcept { return m_filterIds[index]; }

        void SetCallback(ObserverCallbackFn fn, void* userData, void (*destructor)(void*)) {
            if (m_userData != nullptr)
            {
                if (m_destructorFn != nullptr)
                {
                    m_destructorFn(m_userData);
                }
                m_allocator->Deallocate(m_userData);
            }
            m_callbackFn = fn;
            m_userData = userData;
            m_destructorFn = destructor;
        }

        // ─────────────────────────────────────────────────────────────────
        // Execution
        // ─────────────────────────────────────────────────────────────────

        /**
         * Invoke the observer callback
         *
         * @param world World reference for component access
         * @param entity The entity being observed
         * @param component Pointer to component data (may be nullptr)
         */
        void Invoke(World& world, Entity entity, const void* component) const {
            if (m_callbackFn != nullptr && m_enabled)
            {
                m_callbackFn(world, entity, component, m_userData);
            }
        }

        [[nodiscard]] bool HasCallback() const noexcept { return m_callbackFn != nullptr; }

    private:
        ObserverId m_id;
        TriggerType m_trigger;
        bool m_enabled;
        uint8_t m_filterCount;
        TypeId m_componentId;
        TypeId m_filterIds[kMaxFilters];
        ObserverCallbackFn m_callbackFn;
        void* m_userData;
        void (*m_destructorFn)(void*);
        Allocator* m_allocator;
        char m_name[kMaxNameLength + 1];
    };
} // namespace queen
