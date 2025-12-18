#pragma once

#include <queen/observer/observer_event.h>
#include <queen/core/entity.h>
#include <comb/allocator_concepts.h>
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
        constexpr ObserverId() noexcept : value_{0} {}
        constexpr explicit ObserverId(uint32_t id) noexcept : value_{id} {}

        [[nodiscard]] constexpr uint32_t Value() const noexcept { return value_; }
        [[nodiscard]] constexpr bool IsValid() const noexcept { return value_ != 0; }

        [[nodiscard]] constexpr bool operator==(ObserverId other) const noexcept
        {
            return value_ == other.value_;
        }

        [[nodiscard]] constexpr bool operator!=(ObserverId other) const noexcept
        {
            return value_ != other.value_;
        }

    private:
        uint32_t value_;
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
    using ObserverCallbackFn = void (*)(World& world, Entity entity, const void* component, void* user_data);

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
    template<comb::Allocator Allocator>
    class Observer
    {
    public:
        static constexpr size_t kMaxNameLength = 63;

        Observer(Allocator& allocator, ObserverId id, const char* name, TriggerType trigger, TypeId component_id)
            : id_{id}
            , trigger_{trigger}
            , enabled_{true}
            , component_id_{component_id}
            , callback_fn_{nullptr}
            , user_data_{nullptr}
            , destructor_fn_{nullptr}
            , allocator_{&allocator}
        {
            if (name != nullptr)
            {
                size_t len = std::strlen(name);
                if (len > kMaxNameLength) len = kMaxNameLength;
                std::memcpy(name_, name, len);
                name_[len] = '\0';
            }
            else
            {
                name_[0] = '\0';
            }
        }

        ~Observer()
        {
            if (user_data_ != nullptr)
            {
                if (destructor_fn_ != nullptr)
                {
                    destructor_fn_(user_data_);
                }
                allocator_->Deallocate(user_data_);
            }
        }

        Observer(const Observer&) = delete;
        Observer& operator=(const Observer&) = delete;

        Observer(Observer&& other) noexcept
            : id_{other.id_}
            , trigger_{other.trigger_}
            , enabled_{other.enabled_}
            , component_id_{other.component_id_}
            , callback_fn_{other.callback_fn_}
            , user_data_{other.user_data_}
            , destructor_fn_{other.destructor_fn_}
            , allocator_{other.allocator_}
        {
            std::memcpy(name_, other.name_, sizeof(name_));
            other.user_data_ = nullptr;
            other.destructor_fn_ = nullptr;
        }

        Observer& operator=(Observer&& other) noexcept
        {
            if (this != &other)
            {
                if (user_data_ != nullptr)
                {
                    if (destructor_fn_ != nullptr)
                    {
                        destructor_fn_(user_data_);
                    }
                    allocator_->Deallocate(user_data_);
                }

                id_ = other.id_;
                trigger_ = other.trigger_;
                enabled_ = other.enabled_;
                component_id_ = other.component_id_;
                std::memcpy(name_, other.name_, sizeof(name_));
                callback_fn_ = other.callback_fn_;
                user_data_ = other.user_data_;
                destructor_fn_ = other.destructor_fn_;
                allocator_ = other.allocator_;

                other.user_data_ = nullptr;
                other.destructor_fn_ = nullptr;
            }
            return *this;
        }

        // ─────────────────────────────────────────────────────────────────
        // Accessors
        // ─────────────────────────────────────────────────────────────────

        [[nodiscard]] ObserverId Id() const noexcept { return id_; }
        [[nodiscard]] const char* Name() const noexcept { return name_; }
        [[nodiscard]] TriggerType Trigger() const noexcept { return trigger_; }
        [[nodiscard]] TypeId ComponentId() const noexcept { return component_id_; }
        [[nodiscard]] bool IsEnabled() const noexcept { return enabled_; }

        [[nodiscard]] ObserverKey Key() const noexcept
        {
            return ObserverKey{trigger_, component_id_};
        }

        // ─────────────────────────────────────────────────────────────────
        // Mutators
        // ─────────────────────────────────────────────────────────────────

        void SetEnabled(bool enabled) noexcept
        {
            enabled_ = enabled;
        }

        void SetCallback(ObserverCallbackFn fn, void* user_data, void (*destructor)(void*))
        {
            if (user_data_ != nullptr)
            {
                if (destructor_fn_ != nullptr)
                {
                    destructor_fn_(user_data_);
                }
                allocator_->Deallocate(user_data_);
            }
            callback_fn_ = fn;
            user_data_ = user_data;
            destructor_fn_ = destructor;
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
        void Invoke(World& world, Entity entity, const void* component) const
        {
            if (callback_fn_ != nullptr && enabled_)
            {
                callback_fn_(world, entity, component, user_data_);
            }
        }

        [[nodiscard]] bool HasCallback() const noexcept
        {
            return callback_fn_ != nullptr;
        }

    private:
        ObserverId id_;
        TriggerType trigger_;
        bool enabled_;
        // 2 bytes padding
        TypeId component_id_;
        ObserverCallbackFn callback_fn_;
        void* user_data_;
        void (*destructor_fn_)(void*);
        Allocator* allocator_;
        char name_[kMaxNameLength + 1];
    };
}
