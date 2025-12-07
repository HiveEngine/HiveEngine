#pragma once

#include <queen/core/type_id.h>
#include <queen/core/entity.h>
#include <queen/core/entity_allocator.h>
#include <queen/core/entity_location.h>
#include <queen/core/component_info.h>
#include <queen/core/tick.h>
#include <queen/storage/archetype.h>
#include <queen/storage/archetype_graph.h>
#include <queen/storage/component_index.h>
#include <queen/query/query.h>
#include <queen/system/system_storage.h>
#include <queen/scheduler/scheduler.h>
#include <queen/scheduler/parallel_scheduler.h>
#include <queen/command/commands.h>
#include <queen/event/events.h>
#include <queen/observer/observers.h>
#include <queen/world/world_allocators.h>
#include <comb/allocator_concepts.h>
#include <wax/containers/vector.h>
#include <wax/containers/hash_map.h>
#include <hive/core/assert.h>
#include <cstring>

namespace queen
{
    // Forward declarations
    class EntityBuilder;

    template<comb::Allocator Allocator>
    class CommandBuffer;

    // Allocator type aliases used by World
    using PersistentAllocator = comb::BuddyAllocator;
    using ComponentAllocator = comb::BuddyAllocator;
    using FrameAllocator = comb::LinearAllocator;

    /**
     * Central ECS world containing all entities, components, and resources
     *
     * The World is the main entry point for the ECS. It manages entity lifecycle,
     * component storage, resources (global singletons), and provides access to
     * queries.
     *
     * Memory is managed through WorldAllocators which provides:
     * - Persistent allocator (BuddyAllocator): Archetypes, systems, graphs
     * - Component allocator (BuddyAllocator): Entity data, table columns
     * - Frame allocator (LinearAllocator): Per-frame temporary data, reset each Update()
     * - Thread allocators (LinearAllocator per thread): Parallel execution
     *
     * Memory layout:
     * ┌────────────────────────────────────────────────────────────────┐
     * │ Persistent (BuddyAllocator)                                    │
     * │ - archetype_graph_: All archetypes with transitions            │
     * │ - component_index_: TypeId -> Archetypes reverse lookup        │
     * │ - systems_: System storage and metadata                        │
     * │ - scheduler_: Dependency graph and execution order             │
     * │ - resources_: TypeId -> void* global singleton storage         │
     * ├────────────────────────────────────────────────────────────────┤
     * │ Components (BuddyAllocator)                                    │
     * │ - entity_allocator_: Entity ID allocation and recycling        │
     * │ - entity_locations_: Entity -> (Archetype, Row) mapping        │
     * │ - Table column data                                            │
     * ├────────────────────────────────────────────────────────────────┤
     * │ Frame (LinearAllocator) - Reset each Update()                  │
     * │ - commands_: Deferred command buffers                          │
     * │ - Temporary query results                                      │
     * ├────────────────────────────────────────────────────────────────┤
     * │ Thread[0..N] (LinearAllocator per thread)                      │
     * │ - Per-thread temporary allocations                             │
     * │ - Parallel system execution data                               │
     * └────────────────────────────────────────────────────────────────┘
     *
     * Performance characteristics:
     * - Spawn: O(1) amortized (archetype lookup cached)
     * - Despawn: O(n) where n = components (moves data)
     * - Get<T>: O(1) (location lookup + column access)
     * - Add<T>: O(n) (archetype transition, data move)
     * - Remove<T>: O(n) (archetype transition, data move)
     * - IsAlive: O(1)
     * - Resource<T>: O(1) (hash map lookup)
     * - InsertResource<T>: O(1) amortized (hash map insert)
     *
     * Limitations:
     * - Not thread-safe (use command buffers for cross-thread operations)
     *
     * Example:
     * @code
     *   // Create with default allocator sizes
     *   queen::World world;
     *
     *   // Or with custom configuration
     *   queen::WorldAllocatorConfig config;
     *   config.persistent_size = 16_MB;
     *   config.component_size = 128_MB;
     *   queen::World world{config};
     *
     *   // Entities and components
     *   auto entity = world.Spawn()
     *       .With(Position{1.0f, 2.0f, 3.0f})
     *       .With(Velocity{0.1f, 0.0f, 0.0f})
     *       .Build();
     *
     *   Position* pos = world.Get<Position>(entity);
     *   world.Despawn(entity);
     *
     *   // Resources (global singletons)
     *   world.InsertResource(Time{0.0f, 0.016f});
     *   Time* time = world.Resource<Time>();
     *   time->delta = 0.033f;
     *
     *   // Update (sequential or parallel)
     *   world.Update();           // Sequential
     *   world.UpdateParallel();   // Parallel with auto-detected workers
     *   world.UpdateParallel(4);  // Parallel with 4 workers
     * @endcode
     */
    class World
    {
    public:
        using EntityRecord = EntityRecordT<Archetype<ComponentAllocator>>;

        /**
         * Create World with default allocator configuration
         */
        World()
            : World(WorldAllocatorConfig{})
        {
        }

        /**
         * Create World with custom allocator configuration
         */
        explicit World(const WorldAllocatorConfig& config)
            : allocators_{WorldAllocators::Create(config)}
            , entity_allocator_{allocators_.Components()}
            , entity_locations_{allocators_.Components()}
            , archetype_graph_{allocators_.Components()}
            , component_index_{allocators_.Persistent()}
            , resources_{allocators_.Persistent()}
            , resource_metas_{allocators_.Persistent()}
            , systems_{allocators_.Persistent()}
            , scheduler_{allocators_.Persistent()}
            , commands_{allocators_.Persistent()}
            , events_{allocators_.Persistent()}
            , observers_{allocators_.Persistent()}
        {
            component_index_.RegisterArchetype(archetype_graph_.GetEmptyArchetype());
        }

        /**
         * Create World with pre-configured WorldAllocators
         */
        explicit World(WorldAllocators&& allocators)
            : allocators_{std::move(allocators)}
            , entity_allocator_{allocators_.Components()}
            , entity_locations_{allocators_.Components()}
            , archetype_graph_{allocators_.Components()}
            , component_index_{allocators_.Persistent()}
            , resources_{allocators_.Persistent()}
            , resource_metas_{allocators_.Persistent()}
            , systems_{allocators_.Persistent()}
            , scheduler_{allocators_.Persistent()}
            , commands_{allocators_.Persistent()}
            , events_{allocators_.Persistent()}
            , observers_{allocators_.Persistent()}
        {
            component_index_.RegisterArchetype(archetype_graph_.GetEmptyArchetype());
        }

        ~World()
        {
            // Clean up parallel scheduler if created
            if (parallel_scheduler_ != nullptr)
            {
                parallel_scheduler_->~ParallelScheduler<PersistentAllocator>();
                allocators_.Persistent().Deallocate(parallel_scheduler_);
                parallel_scheduler_ = nullptr;
            }

            // Clean up resources
            for (auto it = resources_.begin(); it != resources_.end(); ++it)
            {
                TypeId type_id = it.Key();
                void* data = it.Value();

                for (size_t i = 0; i < resource_metas_.Size(); ++i)
                {
                    if (resource_metas_[i].type_id == type_id)
                    {
                        if (resource_metas_[i].destruct != nullptr)
                        {
                            resource_metas_[i].destruct(data);
                        }
                        break;
                    }
                }

                allocators_.Persistent().Deallocate(data);
            }
        }

        World(const World&) = delete;
        World& operator=(const World&) = delete;
        World(World&&) = default;
        World& operator=(World&&) = delete;

        [[nodiscard]] EntityBuilder Spawn();

        template<typename... Components>
        [[nodiscard]] Entity Spawn(Components&&... components);

        void Despawn(Entity entity)
        {
            if (!IsAlive(entity))
            {
                return;
            }

            EntityRecord* record = entity_locations_.Get(entity);
            if (record == nullptr)
            {
                return;
            }

            Archetype<ComponentAllocator>* archetype = record->archetype;
            uint32_t row = record->row;

            Entity moved = archetype->FreeRow(row);

            if (!moved.IsNull() && !(moved == entity))
            {
                EntityRecord* moved_record = entity_locations_.Get(moved);
                if (moved_record != nullptr)
                {
                    moved_record->row = row;
                }
            }

            entity_locations_.Remove(entity);
            entity_allocator_.Deallocate(entity);
        }

        [[nodiscard]] bool IsAlive(Entity entity) const
        {
            return entity_allocator_.IsAlive(entity);
        }

        template<typename T>
        [[nodiscard]] T* Get(Entity entity) noexcept
        {
            if (!IsAlive(entity))
            {
                return nullptr;
            }

            EntityRecord* record = entity_locations_.Get(entity);
            if (record == nullptr || record->archetype == nullptr)
            {
                return nullptr;
            }

            return record->archetype->template GetComponent<T>(record->row);
        }

        template<typename T>
        [[nodiscard]] const T* Get(Entity entity) const noexcept
        {
            if (!IsAlive(entity))
            {
                return nullptr;
            }

            const EntityRecord* record = entity_locations_.Get(entity);
            if (record == nullptr || record->archetype == nullptr)
            {
                return nullptr;
            }

            return record->archetype->template GetComponent<T>(record->row);
        }

        template<typename T>
        [[nodiscard]] bool Has(Entity entity) const noexcept
        {
            if (!IsAlive(entity))
            {
                return false;
            }

            const EntityRecord* record = entity_locations_.Get(entity);
            if (record == nullptr || record->archetype == nullptr)
            {
                return false;
            }

            return record->archetype->template HasComponent<T>();
        }

        template<typename T>
        void Add(Entity entity, T&& component)
        {
            if (!IsAlive(entity))
            {
                return;
            }

            EntityRecord* record = entity_locations_.Get(entity);
            if (record == nullptr || record->archetype == nullptr)
            {
                return;
            }

            Archetype<ComponentAllocator>* old_arch = record->archetype;

            if (old_arch->template HasComponent<T>())
            {
                old_arch->template SetComponent<T>(record->row, std::forward<T>(component));
                // Trigger OnSet observers for existing component
                const T* comp = old_arch->template GetComponent<T>(record->row);
                observers_.template Trigger<OnSet<T>>(*this, entity, comp);
                return;
            }

            Archetype<ComponentAllocator>* new_arch = archetype_graph_.template GetOrCreateAddTarget<T>(*old_arch);

            if (new_arch != old_arch)
            {
                RegisterNewArchetype(new_arch);
            }

            MoveEntity(entity, *record, old_arch, new_arch);

            new_arch->template SetComponent<T>(record->row, std::forward<T>(component));

            // Trigger OnAdd observers for new component
            const T* comp = new_arch->template GetComponent<T>(record->row);
            observers_.template Trigger<OnAdd<T>>(*this, entity, comp);
        }

        template<typename T>
        void Remove(Entity entity)
        {
            if (!IsAlive(entity))
            {
                return;
            }

            EntityRecord* record = entity_locations_.Get(entity);
            if (record == nullptr || record->archetype == nullptr)
            {
                return;
            }

            Archetype<ComponentAllocator>* old_arch = record->archetype;

            if (!old_arch->template HasComponent<T>())
            {
                return;
            }

            // Trigger OnRemove observers BEFORE removing (so they can access the data)
            const T* comp = old_arch->template GetComponent<T>(record->row);
            observers_.template Trigger<OnRemove<T>>(*this, entity, comp);

            Archetype<ComponentAllocator>* new_arch = archetype_graph_.template GetOrCreateRemoveTarget<T>(*old_arch);

            if (new_arch != old_arch)
            {
                RegisterNewArchetype(new_arch);
            }

            MoveEntity(entity, *record, old_arch, new_arch);
        }

        template<typename T>
        void Set(Entity entity, T&& component)
        {
            Add<T>(entity, std::forward<T>(component));
        }

        [[nodiscard]] size_t EntityCount() const noexcept
        {
            return entity_allocator_.AliveCount();
        }

        [[nodiscard]] size_t ArchetypeCount() const noexcept
        {
            return archetype_graph_.ArchetypeCount();
        }

        // ─────────────────────────────────────────────────────────────
        // Resources (global singletons)
        // ─────────────────────────────────────────────────────────────

        template<typename T>
        void InsertResource(T&& resource)
        {
            using DecayedT = std::decay_t<T>;
            TypeId type_id = TypeIdOf<DecayedT>();

            void** existing = resources_.Find(type_id);
            if (existing != nullptr)
            {
                auto* typed = static_cast<DecayedT*>(*existing);
                *typed = std::forward<T>(resource);
                return;
            }

            void* data = allocators_.Persistent().Allocate(sizeof(DecayedT), alignof(DecayedT));
            hive::Assert(data != nullptr, "Failed to allocate resource");

            new (data) DecayedT{std::forward<T>(resource)};

            resources_.Insert(type_id, data);
            resource_metas_.PushBack(ComponentMeta::Of<DecayedT>());
        }

        template<typename T>
        [[nodiscard]] T* Resource() noexcept
        {
            TypeId type_id = TypeIdOf<T>();

            void** found = resources_.Find(type_id);
            if (found == nullptr)
            {
                return nullptr;
            }

            return static_cast<T*>(*found);
        }

        template<typename T>
        [[nodiscard]] const T* Resource() const noexcept
        {
            TypeId type_id = TypeIdOf<T>();

            void* const* found = resources_.Find(type_id);
            if (found == nullptr)
            {
                return nullptr;
            }

            return static_cast<const T*>(*found);
        }

        template<typename T>
        [[nodiscard]] bool HasResource() const noexcept
        {
            TypeId type_id = TypeIdOf<T>();
            return resources_.Contains(type_id);
        }

        template<typename T>
        void RemoveResource()
        {
            TypeId type_id = TypeIdOf<T>();

            void** found = resources_.Find(type_id);
            if (found == nullptr)
            {
                return;
            }

            void* data = *found;

            for (size_t i = 0; i < resource_metas_.Size(); ++i)
            {
                if (resource_metas_[i].type_id == type_id)
                {
                    if (resource_metas_[i].destruct != nullptr)
                    {
                        resource_metas_[i].destruct(data);
                    }

                    if (i < resource_metas_.Size() - 1)
                    {
                        resource_metas_[i] = resource_metas_[resource_metas_.Size() - 1];
                    }
                    resource_metas_.PopBack();
                    break;
                }
            }

            allocators_.Persistent().Deallocate(data);
            resources_.Remove(type_id);
        }

        [[nodiscard]] size_t ResourceCount() const noexcept
        {
            return resources_.Count();
        }

        // ─────────────────────────────────────────────────────────────
        // Allocator Access
        // ─────────────────────────────────────────────────────────────

        /**
         * Get the WorldAllocators for direct access
         */
        [[nodiscard]] WorldAllocators& GetAllocators() noexcept
        {
            return allocators_;
        }

        [[nodiscard]] const WorldAllocators& GetAllocators() const noexcept
        {
            return allocators_;
        }

        /**
         * Get the persistent allocator (for long-lived metadata)
         */
        [[nodiscard]] PersistentAllocator& GetPersistentAllocator() noexcept
        {
            return allocators_.Persistent();
        }

        /**
         * Get the component allocator (for entity data)
         */
        [[nodiscard]] ComponentAllocator& GetComponentAllocator() noexcept
        {
            return allocators_.Components();
        }

        /**
         * Get the frame allocator (for per-frame temporary data)
         */
        [[nodiscard]] FrameAllocator& GetFrameAllocator() noexcept
        {
            return allocators_.Frame();
        }

        /**
         * Get a thread-local allocator for parallel execution
         */
        [[nodiscard]] FrameAllocator& GetThreadAllocator(size_t thread_index) noexcept
        {
            return allocators_.ThreadFrame(thread_index);
        }

        [[nodiscard]] ArchetypeGraph<ComponentAllocator>& GetArchetypeGraph() noexcept
        {
            return archetype_graph_;
        }

        [[nodiscard]] ComponentIndex<PersistentAllocator>& GetComponentIndex() noexcept
        {
            return component_index_;
        }

        // ─────────────────────────────────────────────────────────────
        // Queries
        // ─────────────────────────────────────────────────────────────

        /**
         * Create a query to iterate over entities matching the given terms
         *
         * Example:
         * @code
         *   world.Query<Read<Position>, Write<Velocity>>()
         *       .Each([](const Position& pos, Velocity& vel) {
         *           vel.dx += pos.x * 0.1f;
         *       });
         * @endcode
         */
        template<typename... Terms>
        [[nodiscard]] queen::Query<PersistentAllocator, Terms...> Query()
        {
            return queen::Query<PersistentAllocator, Terms...>{allocators_.Persistent(), component_index_};
        }

        // ─────────────────────────────────────────────────────────────
        // Systems
        // ─────────────────────────────────────────────────────────────

        /**
         * Register a new system with query-based iteration
         *
         * Example:
         * @code
         *   world.System<Read<Position>, Write<Velocity>>("Movement")
         *       .Each([](const Position& pos, Velocity& vel) {
         *           vel.dx += pos.x * 0.1f;
         *       });
         * @endcode
         */
        template<typename... Terms>
        [[nodiscard]] SystemBuilder<PersistentAllocator, Terms...> System(const char* name)
        {
            return systems_.template Register<Terms...>(*this, name);
        }

        /**
         * Run a specific system by ID
         */
        void RunSystem(SystemId id)
        {
            systems_.RunSystem(*this, id);
        }

        /**
         * Run all registered systems in registration order
         */
        void RunAllSystems()
        {
            systems_.RunAll(*this);
        }

        /**
         * Get the number of registered systems
         */
        [[nodiscard]] size_t SystemCount() const noexcept
        {
            return systems_.SystemCount();
        }

        /**
         * Enable or disable a system
         */
        void SetSystemEnabled(SystemId id, bool enabled)
        {
            systems_.SetSystemEnabled(id, enabled);
        }

        /**
         * Check if a system is enabled
         */
        [[nodiscard]] bool IsSystemEnabled(SystemId id) const
        {
            return systems_.IsSystemEnabled(id);
        }

        /**
         * Get the system storage for advanced operations
         */
        [[nodiscard]] SystemStorage<PersistentAllocator>& GetSystemStorage() noexcept
        {
            return systems_;
        }

        /**
         * Update the world by running all systems in dependency order
         *
         * This uses the scheduler to compute the correct execution order
         * based on access patterns and explicit ordering constraints.
         * The world tick is incremented at the start of each Update().
         * The frame allocator is reset at the end.
         */
        void Update()
        {
            IncrementTick();
            events_.SwapBuffers();  // Swap event buffers for new frame
            scheduler_.RunAll(*this, systems_);
            allocators_.ResetFrame();
        }

        /**
         * Update the world using parallel execution
         *
         * Independent systems are executed concurrently using a thread pool.
         * Systems with conflicting data access are serialized.
         * Creates the parallel scheduler on first call.
         * Thread allocators are reset after each system batch.
         *
         * @param worker_count Number of worker threads (0 = auto-detect)
         */
        void UpdateParallel(size_t worker_count = 0)
        {
            IncrementTick();
            events_.SwapBuffers();  // Swap event buffers for new frame

            // Create parallel scheduler on first use
            if (parallel_scheduler_ == nullptr)
            {
                void* mem = allocators_.Persistent().Allocate(
                    sizeof(ParallelScheduler<PersistentAllocator>),
                    alignof(ParallelScheduler<PersistentAllocator>)
                );
                parallel_scheduler_ = new (mem) ParallelScheduler<PersistentAllocator>(allocators_.Persistent(), worker_count);
            }

            parallel_scheduler_->RunAll(*this, systems_);
            allocators_.ResetFrame();
            allocators_.ResetThreadFrames();
        }

        /**
         * Get the scheduler for advanced scheduling operations
         */
        [[nodiscard]] Scheduler<PersistentAllocator>& GetScheduler() noexcept
        {
            return scheduler_;
        }

        [[nodiscard]] const Scheduler<PersistentAllocator>& GetScheduler() const noexcept
        {
            return scheduler_;
        }

        /**
         * Get the parallel scheduler (may be nullptr if not yet created)
         */
        [[nodiscard]] ParallelScheduler<PersistentAllocator>* GetParallelScheduler() noexcept
        {
            return parallel_scheduler_;
        }

        [[nodiscard]] const ParallelScheduler<PersistentAllocator>* GetParallelScheduler() const noexcept
        {
            return parallel_scheduler_;
        }

        /**
         * Check if parallel scheduler is available
         */
        [[nodiscard]] bool HasParallelScheduler() const noexcept
        {
            return parallel_scheduler_ != nullptr;
        }

        /**
         * Invalidate the scheduler's dependency graph
         *
         * Call this when systems are added or modified to force rebuild.
         * Invalidates both sequential and parallel schedulers.
         */
        void InvalidateScheduler() noexcept
        {
            scheduler_.Invalidate();
            if (parallel_scheduler_ != nullptr)
            {
                parallel_scheduler_->Invalidate();
            }
        }

        /**
         * Get the thread-local command buffer collection
         *
         * Use this to get a CommandBuffer for the current thread.
         * Commands are automatically flushed at the end of Update().
         */
        [[nodiscard]] Commands<PersistentAllocator>& GetCommands() noexcept
        {
            return commands_;
        }

        [[nodiscard]] const Commands<PersistentAllocator>& GetCommands() const noexcept
        {
            return commands_;
        }

        // ─────────────────────────────────────────────────────────────
        // Events
        // ─────────────────────────────────────────────────────────────

        /**
         * Get the events registry
         */
        [[nodiscard]] Events<PersistentAllocator>& GetEvents() noexcept
        {
            return events_;
        }

        [[nodiscard]] const Events<PersistentAllocator>& GetEvents() const noexcept
        {
            return events_;
        }

        /**
         * Send an event (adds to current frame's queue)
         *
         * @tparam E Event type
         * @param event The event to send
         */
        template<typename E>
        void SendEvent(E&& event)
        {
            events_.template Send<std::decay_t<E>>(std::forward<E>(event));
        }

        /**
         * Get an EventWriter for a specific event type
         *
         * @tparam E Event type
         * @return EventWriter for sending events
         */
        template<typename E>
        [[nodiscard]] EventWriter<E, PersistentAllocator> EventWriter()
        {
            return events_.template Writer<E>();
        }

        /**
         * Get an EventReader for a specific event type
         *
         * @tparam E Event type
         * @return EventReader for reading events
         */
        template<typename E>
        [[nodiscard]] queen::EventReader<E, PersistentAllocator> EventReader()
        {
            return events_.template Reader<E>();
        }

        // ─────────────────────────────────────────────────────────────
        // Observers
        // ─────────────────────────────────────────────────────────────

        /**
         * Register an observer for structural changes
         *
         * @tparam TriggerEvent The trigger type (OnAdd<T>, OnRemove<T>, OnSet<T>)
         * @param name Observer name for debugging
         * @return ObserverBuilder for fluent configuration
         *
         * Example:
         * @code
         *   world.Observer<OnAdd<Health>>("LogSpawn")
         *       .Each([](Entity e, const Health& hp) {
         *           Log("Entity {} has {} HP", e.Index(), hp.value);
         *       });
         * @endcode
         */
        template<ObserverTrigger TriggerEvent>
        [[nodiscard]] ObserverBuilder<TriggerEvent, PersistentAllocator> Observer(const char* name)
        {
            return observers_.template Register<TriggerEvent>(*this, name);
        }

        /**
         * Get the observer storage for direct access
         */
        [[nodiscard]] ObserverStorage<PersistentAllocator>& GetObserverStorage() noexcept
        {
            return observers_;
        }

        [[nodiscard]] const ObserverStorage<PersistentAllocator>& GetObserverStorage() const noexcept
        {
            return observers_;
        }

        /**
         * Enable or disable an observer
         */
        void SetObserverEnabled(ObserverId id, bool enabled)
        {
            observers_.SetEnabled(id, enabled);
        }

        /**
         * Check if an observer is enabled
         */
        [[nodiscard]] bool IsObserverEnabled(ObserverId id) const
        {
            return observers_.IsEnabled(id);
        }

        /**
         * Get the number of registered observers
         */
        [[nodiscard]] size_t ObserverCount() const noexcept
        {
            return observers_.ObserverCount();
        }

        // ─────────────────────────────────────────────────────────────
        // Change Detection
        // ─────────────────────────────────────────────────────────────

        /**
         * Get the current world tick
         */
        [[nodiscard]] Tick CurrentTick() const noexcept
        {
            return current_tick_;
        }

        /**
         * Increment the world tick
         *
         * Called automatically at the start of Update(). Can also be called
         * manually to advance the tick without running systems.
         */
        void IncrementTick() noexcept
        {
            ++current_tick_;
        }

    private:
        friend class EntityBuilder;

        template<comb::Allocator OtherAlloc>
        friend class CommandBuffer;

        Entity AllocateEntity()
        {
            return entity_allocator_.Allocate();
        }

        void PlaceEntity(Entity entity, Archetype<ComponentAllocator>* archetype)
        {
            uint32_t row = archetype->AllocateRow(entity, current_tick_);
            entity_locations_.Set(entity, EntityRecord{archetype, row});
        }

        void RegisterNewArchetype(Archetype<ComponentAllocator>* archetype)
        {
            if (archetype->ComponentCount() > 0)
            {
                bool already_registered = false;
                const auto* list = component_index_.GetArchetypesWith(archetype->GetComponentTypes()[0]);
                if (list != nullptr)
                {
                    for (size_t i = 0; i < list->Size(); ++i)
                    {
                        if ((*list)[i] == archetype)
                        {
                            already_registered = true;
                            break;
                        }
                    }
                }

                if (!already_registered)
                {
                    component_index_.RegisterArchetype(archetype);
                }
            }
        }

        void MoveEntity(Entity entity, EntityRecord& record,
                       Archetype<ComponentAllocator>* old_arch, Archetype<ComponentAllocator>* new_arch)
        {
            uint32_t old_row = record.row;

            uint32_t new_row = new_arch->AllocateRow(entity, current_tick_);

            const auto& old_metas = old_arch->GetComponentMetas();

            for (size_t i = 0; i < old_metas.Size(); ++i)
            {
                TypeId type_id = old_metas[i].type_id;

                if (new_arch->HasComponent(type_id))
                {
                    void* src = old_arch->GetComponentRaw(old_row, type_id);
                    void* dst = new_arch->GetComponentRaw(new_row, type_id);
                    old_metas[i].move(dst, src);
                }
            }

            Entity moved = old_arch->FreeRow(old_row);

            if (!moved.IsNull() && !(moved == entity))
            {
                EntityRecord* moved_record = entity_locations_.Get(moved);
                if (moved_record != nullptr)
                {
                    moved_record->row = old_row;
                }
            }

            record.archetype = new_arch;
            record.row = new_row;
        }

        // IMPORTANT: allocators_ MUST be first for initialization order
        WorldAllocators allocators_;

        EntityAllocator<ComponentAllocator> entity_allocator_;
        EntityLocationMap<ComponentAllocator, Archetype<ComponentAllocator>> entity_locations_;
        ArchetypeGraph<ComponentAllocator> archetype_graph_;
        ComponentIndex<PersistentAllocator> component_index_;

        wax::HashMap<TypeId, void*, PersistentAllocator> resources_;
        wax::Vector<ComponentMeta, PersistentAllocator> resource_metas_;

        SystemStorage<PersistentAllocator> systems_;
        Scheduler<PersistentAllocator> scheduler_;
        ParallelScheduler<PersistentAllocator>* parallel_scheduler_{nullptr};
        Commands<PersistentAllocator> commands_;
        Events<PersistentAllocator> events_;
        ObserverStorage<PersistentAllocator> observers_;
        Tick current_tick_{1};  // Start at 1 so tick 0 means "never changed"
    };

    /**
     * Builder for spawning entities with components
     *
     * Example:
     * @code
     *   auto entity = world.Spawn()
     *       .With(Position{1.0f, 2.0f, 3.0f})
     *       .With(Velocity{0.1f, 0.0f, 0.0f})
     *       .Build();
     * @endcode
     */
    class EntityBuilder
    {
    public:
        EntityBuilder(World& world)
            : world_{&world}
            , pending_metas_{world.GetFrameAllocator()}
            , pending_data_{world.GetFrameAllocator()}
        {
        }

        template<typename T>
        EntityBuilder& With(T&& component)
        {
            TypeId type_id = TypeIdOf<T>();

            for (size_t i = 0; i < pending_metas_.Size(); ++i)
            {
                if (pending_metas_[i].type_id == type_id)
                {
                    T* existing = static_cast<T*>(pending_data_[i]);
                    *existing = std::forward<T>(component);
                    return *this;
                }
            }

            pending_metas_.PushBack(ComponentMeta::Of<T>());

            void* data = world_->GetFrameAllocator().Allocate(sizeof(T), alignof(T));
            new (data) T{std::forward<T>(component)};
            pending_data_.PushBack(data);

            return *this;
        }

        EntityBuilder& WithRaw(const ComponentMeta& meta, void* source_data)
        {
            TypeId type_id = meta.type_id;

            for (size_t i = 0; i < pending_metas_.Size(); ++i)
            {
                if (pending_metas_[i].type_id == type_id)
                {
                    if (meta.move != nullptr)
                    {
                        meta.move(pending_data_[i], source_data);
                    }
                    else
                    {
                        std::memcpy(pending_data_[i], source_data, meta.size);
                    }
                    return *this;
                }
            }

            pending_metas_.PushBack(meta);

            void* data = world_->GetFrameAllocator().Allocate(meta.size, meta.alignment);
            if (meta.move != nullptr)
            {
                meta.move(data, source_data);
            }
            else
            {
                std::memcpy(data, source_data, meta.size);
            }
            pending_data_.PushBack(data);

            return *this;
        }

        Entity Build()
        {
            Entity entity = world_->AllocateEntity();

            Archetype<ComponentAllocator>* archetype = world_->archetype_graph_.GetEmptyArchetype();

            for (size_t i = 0; i < pending_metas_.Size(); ++i)
            {
                archetype = world_->archetype_graph_.GetOrCreateAddTarget(*archetype, pending_metas_[i]);
            }

            world_->RegisterNewArchetype(archetype);

            world_->PlaceEntity(entity, archetype);

            auto* record = world_->entity_locations_.Get(entity);
            for (size_t i = 0; i < pending_metas_.Size(); ++i)
            {
                TypeId type_id = pending_metas_[i].type_id;
                archetype->SetComponent(record->row, type_id, pending_data_[i]);

                if (pending_metas_[i].destruct != nullptr)
                {
                    pending_metas_[i].destruct(pending_data_[i]);
                }
            }

            pending_metas_.Clear();
            pending_data_.Clear();

            return entity;
        }

    private:
        World* world_;
        wax::Vector<ComponentMeta, FrameAllocator> pending_metas_;
        wax::Vector<void*, FrameAllocator> pending_data_;
    };

    inline EntityBuilder World::Spawn()
    {
        return EntityBuilder{*this};
    }

    template<typename... Components>
    Entity World::Spawn(Components&&... components)
    {
        EntityBuilder builder{*this};
        (builder.With(std::forward<Components>(components)), ...);
        return builder.Build();
    }

    // ─────────────────────────────────────────────────────────────
    // CommandBuffer::Flush implementation (here to avoid circular dependency)
    // ─────────────────────────────────────────────────────────────

    template<comb::Allocator Allocator>
    void CommandBuffer<Allocator>::Flush(World& world)
    {
        spawned_entities_.Clear();
        spawned_entities_.Reserve(spawn_count_);

        for (uint32_t i = 0; i < spawn_count_; ++i)
        {
            spawned_entities_.PushBack(Entity::Invalid());
        }

        for (size_t i = 0; i < commands_.Size(); ++i)
        {
            const detail::Command& cmd = commands_[i];

            if (cmd.type == CommandType::Spawn)
            {
                uint32_t spawn_index = cmd.entity.Index();

                auto builder = world.Spawn();

                for (size_t j = i + 1; j < commands_.Size(); ++j)
                {
                    const detail::Command& other = commands_[j];
                    if (other.type == CommandType::AddComponent &&
                        IsPendingEntity(other.entity) &&
                        other.entity.Index() == spawn_index)
                    {
                        builder.WithRaw(other.meta, other.data);
                    }
                }

                Entity real_entity = builder.Build();
                spawned_entities_[spawn_index] = real_entity;
            }
        }

        for (size_t i = 0; i < commands_.Size(); ++i)
        {
            const detail::Command& cmd = commands_[i];

            switch (cmd.type)
            {
                case CommandType::Spawn:
                    break;

                case CommandType::Despawn:
                    world.Despawn(cmd.entity);
                    break;

                case CommandType::AddComponent:
                {
                    if (IsPendingEntity(cmd.entity))
                    {
                        break;
                    }

                    Entity entity = cmd.entity;
                    if (!world.IsAlive(entity))
                    {
                        break;
                    }

                    auto* record = world.entity_locations_.Get(entity);
                    if (record == nullptr || record->archetype == nullptr)
                    {
                        break;
                    }

                    auto* old_arch = record->archetype;

                    if (old_arch->HasComponent(cmd.component_type))
                    {
                        old_arch->SetComponent(record->row, cmd.component_type, cmd.data);
                    }
                    else
                    {
                        auto* new_arch = world.archetype_graph_.GetOrCreateAddTarget(*old_arch, cmd.meta);

                        if (new_arch != old_arch)
                        {
                            world.RegisterNewArchetype(new_arch);
                            world.MoveEntity(entity, *record, old_arch, new_arch);
                        }

                        new_arch->SetComponent(record->row, cmd.component_type, cmd.data);
                    }
                    break;
                }

                case CommandType::RemoveComponent:
                {
                    Entity entity = ResolveEntity(cmd.entity);
                    if (!world.IsAlive(entity))
                    {
                        break;
                    }

                    auto* record = world.entity_locations_.Get(entity);
                    if (record == nullptr || record->archetype == nullptr)
                    {
                        break;
                    }

                    auto* old_arch = record->archetype;

                    if (!old_arch->HasComponent(cmd.component_type))
                    {
                        break;
                    }

                    auto* new_arch = world.archetype_graph_.GetOrCreateRemoveTarget(*old_arch, cmd.component_type);

                    if (new_arch != old_arch)
                    {
                        world.RegisterNewArchetype(new_arch);
                        world.MoveEntity(entity, *record, old_arch, new_arch);
                    }
                    break;
                }

                case CommandType::SetComponent:
                {
                    Entity entity = ResolveEntity(cmd.entity);
                    if (!world.IsAlive(entity))
                    {
                        break;
                    }

                    auto* record = world.entity_locations_.Get(entity);
                    if (record == nullptr || record->archetype == nullptr)
                    {
                        break;
                    }

                    auto* old_arch = record->archetype;

                    if (old_arch->HasComponent(cmd.component_type))
                    {
                        old_arch->SetComponent(record->row, cmd.component_type, cmd.data);
                    }
                    else
                    {
                        auto* new_arch = world.archetype_graph_.GetOrCreateAddTarget(*old_arch, cmd.meta);

                        if (new_arch != old_arch)
                        {
                            world.RegisterNewArchetype(new_arch);
                            world.MoveEntity(entity, *record, old_arch, new_arch);
                        }

                        new_arch->SetComponent(record->row, cmd.component_type, cmd.data);
                    }
                    break;
                }
            }
        }

        // Clear commands but preserve spawned_entities_ for GetSpawnedEntity() calls
        for (size_t i = 0; i < commands_.Size(); ++i)
        {
            const detail::Command& cmd = commands_[i];
            if (cmd.data != nullptr && cmd.meta.destruct != nullptr)
            {
                cmd.meta.destruct(cmd.data);
            }
        }

        commands_.Clear();
        spawn_count_ = 0;
        ResetBlocks();
    }
}

// Include method implementations now that World is complete
#include <queen/system/system_builder_impl.h>
#include <queen/scheduler/scheduler_impl.h>
#include <queen/scheduler/parallel_scheduler_impl.h>
