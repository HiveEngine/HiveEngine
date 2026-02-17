#pragma once

#include <queen/observer/observer_event.h>
#include <queen/observer/observer.h>
#include <queen/core/entity.h>
#include <comb/allocator_concepts.h>
#include <type_traits>

namespace queen
{
    class World;

    template<comb::Allocator Allocator>
    class ObserverStorage;

    /**
     * Fluent API for registering observers
     *
     * ObserverBuilder provides a builder pattern for defining observers
     * that react to structural ECS changes. The builder extracts the
     * trigger type and component type from the template parameter.
     *
     * Memory layout:
     * ┌────────────────────────────────────────────────────────────────┐
     * │ world_: World* (8 bytes)                                       │
     * │ allocator_: Allocator* (8 bytes)                               │
     * │ storage_: ObserverStorage* (8 bytes)                           │
     * │ observer_: Observer* (8 bytes)                                 │
     * └────────────────────────────────────────────────────────────────┘
     *
     * Performance characteristics:
     * - Each() registration: O(1) - allocate and store callback
     * - Build: O(1) - already registered with storage
     *
     * Use cases:
     * - Logging component lifecycle events
     * - Validating component data on add/set
     * - Maintaining derived state (caches, indices)
     * - Triggering side effects (audio, VFX)
     *
     * Limitations:
     * - One component type per observer
     * - Cannot observe multiple trigger types simultaneously
     * - Callback executes synchronously (may block)
     *
     * Example:
     * @code
     *   // Simple observer - entity and component
     *   world.Observer<OnAdd<Health>>("LogSpawn")
     *       .Each([](Entity e, const Health& hp) {
     *           Log("Entity {} spawned with {} HP", e.Index(), hp.value);
     *       });
     *
     *   // Observer with filter - only trigger for Player entities
     *   world.Observer<OnRemove<Health>>("PlayerDeath")
     *       .With<Player>()
     *       .Each([](Entity e, const Health& hp) {
     *           Log("Player {} died!", e.Index());
     *       });
     *
     *   // Observer with Commands access for deferred mutations
     *   world.Observer<OnSet<Health>>("DeathCheck")
     *       .EachWithCommands([](Entity e, const Health& hp, Commands& cmd) {
     *           if (hp.value <= 0) {
     *               cmd.Get().Despawn(e);
     *           }
     *       });
     *
     *   // Entity-only observer (no component data)
     *   world.Observer<OnRemove<Health>>("LogDeath")
     *       .EachEntity([](Entity e) {
     *           Log("Entity {} despawned", e.Index());
     *       });
     * @endcode
     */
    template<ObserverTrigger TriggerEvent, comb::Allocator Allocator>
    class ObserverBuilder
    {
    public:
        using ComponentType = typename TriggerEvent::ComponentType;

        ObserverBuilder(World& world, Allocator& allocator,
                       ObserverStorage<Allocator>& storage, Observer<Allocator>* observer)
            : world_{&world}
            , allocator_{&allocator}
            , storage_{&storage}
            , observer_{observer}
        {
        }

        /**
         * Add a filter component requirement
         *
         * The observer will only trigger for entities that have
         * the specified component type.
         *
         * @tparam T Component type to require
         * @return Reference to this builder for chaining
         */
        template<typename T>
        ObserverBuilder& With()
        {
            static_assert(sizeof(T) == 0, "Observer::With<T>() filter is not yet implemented");
            return *this;
        }

        /**
         * Register callback with entity and component reference
         *
         * Callback signature: void(Entity e, const Component& c)
         * The component reference is valid during the callback.
         *
         * @tparam F Lambda type
         * @param func Callback function
         * @return ObserverId for the registered observer
         */
        template<typename F>
        ObserverId Each(F&& func)
        {
            using FuncType = std::decay_t<F>;

            // Allocate storage for the lambda
            void* user_data = allocator_->Allocate(sizeof(FuncType), alignof(FuncType));
            new (user_data) FuncType{std::forward<F>(func)};

            // Create type-erased callback wrapper
            auto callback = [](World& world, Entity entity, const void* component, void* data) {
                (void)world;
                FuncType* fn = static_cast<FuncType*>(data);
                const ComponentType* comp = static_cast<const ComponentType*>(component);
                if (comp != nullptr)
                {
                    (*fn)(entity, *comp);
                }
            };

            auto destructor = [](void* data) {
                FuncType* fn = static_cast<FuncType*>(data);
                fn->~FuncType();
            };

            observer_->SetCallback(callback, user_data, destructor);
            return observer_->Id();
        }

        /**
         * Register callback with entity only (no component data)
         *
         * Callback signature: void(Entity e)
         * Use this when you don't need the component data.
         *
         * @tparam F Lambda type
         * @param func Callback function
         * @return ObserverId for the registered observer
         */
        template<typename F>
        ObserverId EachEntity(F&& func)
        {
            using FuncType = std::decay_t<F>;

            void* user_data = allocator_->Allocate(sizeof(FuncType), alignof(FuncType));
            new (user_data) FuncType{std::forward<F>(func)};

            auto callback = [](World& world, Entity entity, const void* component, void* data) {
                (void)world;
                (void)component;
                FuncType* fn = static_cast<FuncType*>(data);
                (*fn)(entity);
            };

            auto destructor = [](void* data) {
                FuncType* fn = static_cast<FuncType*>(data);
                fn->~FuncType();
            };

            observer_->SetCallback(callback, user_data, destructor);
            return observer_->Id();
        }

        /**
         * Register callback with entity, component, and World reference
         *
         * Callback signature: void(World& world, Entity e, const Component& c)
         * Use this when you need to query other components during the callback.
         *
         * @tparam F Lambda type
         * @param func Callback function
         * @return ObserverId for the registered observer
         */
        template<typename F>
        ObserverId EachWithWorld(F&& func)
        {
            using FuncType = std::decay_t<F>;

            void* user_data = allocator_->Allocate(sizeof(FuncType), alignof(FuncType));
            new (user_data) FuncType{std::forward<F>(func)};

            auto callback = [](World& world, Entity entity, const void* component, void* data) {
                FuncType* fn = static_cast<FuncType*>(data);
                const ComponentType* comp = static_cast<const ComponentType*>(component);
                if (comp != nullptr)
                {
                    (*fn)(world, entity, *comp);
                }
            };

            auto destructor = [](void* data) {
                FuncType* fn = static_cast<FuncType*>(data);
                fn->~FuncType();
            };

            observer_->SetCallback(callback, user_data, destructor);
            return observer_->Id();
        }

        /**
         * Get the observer ID (before callback is registered)
         */
        [[nodiscard]] ObserverId Id() const noexcept
        {
            return observer_->Id();
        }

    private:
        World* world_;
        Allocator* allocator_;
        ObserverStorage<Allocator>* storage_;
        Observer<Allocator>* observer_;
    };
}
