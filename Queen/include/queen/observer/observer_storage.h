#pragma once

#include <comb/allocator_concepts.h>

#include <wax/containers/hash_map.h>
#include <wax/containers/vector.h>

#include <queen/core/entity.h>
#include <queen/observer/observer.h>
#include <queen/observer/observer_builder.h>
#include <queen/observer/observer_event.h>

#include <cstring>

namespace queen
{
    class World;

    /**
     * World-owned registry of all observers
     *
     * ObserverStorage manages observer registration, lookup, and invocation.
     * Observers are indexed by (trigger_type, component_type) for fast
     * matching when structural changes occur.
     *
     * The storage uses a two-level structure:
     * 1. Vector of Observer objects (owns all observers)
     * 2. HashMap from ObserverKey to Vector of indices (for fast lookup)
     *
     * Memory layout:
     * ┌────────────────────────────────────────────────────────────────┐
     * │ allocator_: Allocator* (8 bytes)                               │
     * │ observers_: Vector<Observer> (owns all observers)              │
     * │ lookup_: HashMap<ObserverKey, Vector<uint32_t>>                │
     * │   Key{Add, Health}    → [0, 3]  (indices into observers_)      │
     * │   Key{Remove, Health} → [1]                                    │
     * │   Key{Set, Position}  → [2, 4]                                 │
     * └────────────────────────────────────────────────────────────────┘
     *
     * Performance characteristics:
     * - Register: O(1) amortized (vector push + hashmap insert)
     * - Lookup by key: O(1) average (hashmap lookup)
     * - Trigger: O(k) where k = observers matching the key
     * - GetObserver(id): O(1) array access
     *
     * Limitations:
     * - Observer indices stored as uint32_t (max ~4 billion observers)
     * - Lookup vectors grow with observers per key
     * - No automatic deregistration (observers live until storage destruction)
     *
     * Example:
     * @code
     *   ObserverStorage storage{alloc};
     *
     *   // Register observers
     *   ObserverId id = storage.Register<OnAdd<Health>>(world, "LogSpawn")
     *       .Each([](Entity e, const Health& hp) { ... });
     *
     *   // Trigger observers for a change
     *   storage.Trigger(TriggerType::Add, TypeIdOf<Health>(), world, entity, &hp);
     *
     *   // Disable an observer
     *   storage.SetEnabled(id, false);
     * @endcode
     */
    template <comb::Allocator Allocator> class ObserverStorage
    {
    public:
        explicit ObserverStorage(Allocator& allocator)
            : m_allocator{&allocator}
            , m_observers{allocator}
            , m_lookup{allocator, 32}
        {
        }

        ~ObserverStorage() = default;

        ObserverStorage(const ObserverStorage&) = delete;
        ObserverStorage& operator=(const ObserverStorage&) = delete;
        ObserverStorage(ObserverStorage&&) = default;
        ObserverStorage& operator=(ObserverStorage&&) = default;

        // ─────────────────────────────────────────────────────────────────
        // Registration
        // ─────────────────────────────────────────────────────────────────

        /**
         * Register a new observer for a trigger event
         *
         * @tparam TriggerEvent The trigger type (OnAdd<T>, OnRemove<T>, OnSet<T>)
         * @param world World reference for callback execution
         * @param name Observer name (for debugging)
         * @return ObserverBuilder for further configuration
         */
        template <ObserverTrigger TriggerEvent>
        ObserverBuilder<TriggerEvent, Allocator> Register(World& world, const char* name)
        {
            // Start IDs at 1 so 0 can be used as invalid sentinel
            ObserverId id{static_cast<uint32_t>(m_observers.Size()) + 1};

            TriggerType trigger = GetTriggerType<TriggerEvent>();
            TypeId componentId = GetTriggerComponentId<TriggerEvent>();

            uint32_t index = static_cast<uint32_t>(m_observers.Size());
            m_observers.EmplaceBack(*m_allocator, id, name, trigger, componentId);

            // Add to lookup table using vector index (not ID)
            ObserverKey key = ObserverKey::Of<TriggerEvent>();
            AddToLookup(key, index);

            return ObserverBuilder<TriggerEvent, Allocator>{world, *m_allocator, *this,
                                                            &m_observers[m_observers.Size() - 1]};
        }

        // ─────────────────────────────────────────────────────────────────
        // Lookup
        // ─────────────────────────────────────────────────────────────────

        /**
         * Get an observer by ID
         *
         * @param id Observer identifier
         * @return Pointer to observer, or nullptr if invalid
         */
        [[nodiscard]] Observer<Allocator>* GetObserver(ObserverId id)
        {
            // IDs start at 1, so index = id - 1
            if (!id.IsValid() || id.Value() > m_observers.Size())
            {
                return nullptr;
            }
            return &m_observers[id.Value() - 1];
        }

        [[nodiscard]] const Observer<Allocator>* GetObserver(ObserverId id) const
        {
            // IDs start at 1, so index = id - 1
            if (!id.IsValid() || id.Value() > m_observers.Size())
            {
                return nullptr;
            }
            return &m_observers[id.Value() - 1];
        }

        /**
         * Get an observer by name
         *
         * @param name Observer name
         * @return Pointer to observer, or nullptr if not found
         */
        [[nodiscard]] Observer<Allocator>* GetObserverByName(const char* name)
        {
            for (size_t i = 0; i < m_observers.Size(); ++i)
            {
                if (std::strcmp(m_observers[i].Name(), name) == 0)
                {
                    return &m_observers[i];
                }
            }
            return nullptr;
        }

        // ─────────────────────────────────────────────────────────────────
        // Triggering
        // ─────────────────────────────────────────────────────────────────

        /**
         * Trigger all observers matching a structural change
         *
         * @param trigger The trigger type (Add/Remove/Set)
         * @param component_id The component type that changed
         * @param world World reference for callback execution
         * @param entity The entity that changed
         * @param component Pointer to component data (may be nullptr for OnRemove)
         */
        void Trigger(TriggerType trigger, TypeId componentId, World& world, Entity entity,
                     const void* component); // Defined in observer_storage_impl.h

        /**
         * Trigger all observers matching a trigger event type
         *
         * Type-safe version using compile-time trigger type.
         *
         * @tparam TriggerEvent The trigger type
         * @param world World reference
         * @param entity The entity that changed
         * @param component Pointer to component data
         */
        template <ObserverTrigger TriggerEvent>
        void Trigger(World& world, Entity entity, const typename TriggerEvent::ComponentType* component)
        {
            Trigger(GetTriggerType<TriggerEvent>(), GetTriggerComponentId<TriggerEvent>(), world, entity,
                    static_cast<const void*>(component));
        }

        // ─────────────────────────────────────────────────────────────────
        // State management
        // ─────────────────────────────────────────────────────────────────

        /**
         * Enable or disable an observer
         */
        void SetEnabled(ObserverId id, bool enabled)
        {
            Observer<Allocator>* obs = GetObserver(id);
            if (obs != nullptr)
            {
                obs->SetEnabled(enabled);
            }
        }

        /**
         * Check if an observer is enabled
         */
        [[nodiscard]] bool IsEnabled(ObserverId id) const
        {
            const Observer<Allocator>* obs = GetObserver(id);
            if (obs != nullptr)
            {
                return obs->IsEnabled();
            }
            return false;
        }

        /**
         * Get the number of registered observers
         */
        [[nodiscard]] size_t ObserverCount() const noexcept
        {
            return m_observers.Size();
        }

        /**
         * Check if any observers are registered
         */
        [[nodiscard]] bool IsEmpty() const noexcept
        {
            return m_observers.IsEmpty();
        }

        /**
         * Check if any observers exist for a given key
         */
        [[nodiscard]] bool HasObservers(TriggerType trigger, TypeId componentId) const
        {
            ObserverKey key{trigger, componentId};
            auto* indices = m_lookup.Find(key);
            return indices != nullptr && !indices->IsEmpty();
        }

        template <ObserverTrigger TriggerEvent> [[nodiscard]] bool HasObservers() const
        {
            return HasObservers(GetTriggerType<TriggerEvent>(), GetTriggerComponentId<TriggerEvent>());
        }

    private:
        void AddToLookup(ObserverKey key, uint32_t observerIndex)
        {
            auto* indices = m_lookup.Find(key);
            if (indices == nullptr)
            {
                wax::Vector<uint32_t> newIndices{*m_allocator};
                newIndices.PushBack(observerIndex);
                m_lookup.Insert(key, std::move(newIndices));
            }
            else
            {
                indices->PushBack(observerIndex);
            }
        }

        Allocator* m_allocator;
        wax::Vector<Observer<Allocator>> m_observers;
        wax::HashMap<ObserverKey, wax::Vector<uint32_t>, ObserverKeyHash> m_lookup;
    };
} // namespace queen
