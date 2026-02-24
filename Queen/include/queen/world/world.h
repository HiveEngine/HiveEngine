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
#include <queen/hierarchy/hierarchy.h>
#include <queen/world/world_allocators.h>
#include <comb/allocator_concepts.h>
#include <wax/containers/vector.h>
#include <wax/containers/hash_map.h>
#include <hive/core/assert.h>
#include <hive/profiling/profiler.h>
#include <cstring>
#include <mutex>

namespace queen
{
    class EntityBuilder;

    template<comb::Allocator Allocator>
    class CommandBuffer;

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
            : allocators_{
                config.persistent_size,
                config.component_size,
                config.frame_size,
                config.thread_frame_size,
                config.thread_count
            }
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
            if (parallel_scheduler_ != nullptr)
            {
                parallel_scheduler_->~ParallelScheduler<PersistentAllocator>();
                allocators_.Persistent().Deallocate(parallel_scheduler_);
                parallel_scheduler_ = nullptr;
            }

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
        World(World&&) = delete;  // WorldAllocators contains mutex (not movable)
        World& operator=(World&&) = delete;

        [[nodiscard]] EntityBuilder Spawn();

        template<typename... Components>
        [[nodiscard]] Entity Spawn(Components&&... components);

        void Despawn(Entity entity)
        {
            HIVE_PROFILE_SCOPE_N("World::Despawn");
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

        [[nodiscard]] bool IsAlive(Entity entity) const noexcept
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

        [[nodiscard]] bool HasComponent(Entity entity, TypeId type_id) const noexcept
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

            return record->archetype->HasComponent(type_id);
        }

        template<typename T>
        void Add(Entity entity, T&& component)
        {
            HIVE_PROFILE_SCOPE_N("World::Add");
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

            const T* comp = new_arch->template GetComponent<T>(record->row);
            observers_.template Trigger<OnAdd<T>>(*this, entity, comp);
        }

        template<typename T>
        void Remove(Entity entity)
        {
            HIVE_PROFILE_SCOPE_N("World::Remove");
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

        /**
         * Iterate over all non-empty archetypes
         *
         * @param callback Called for each archetype that contains at least one entity
         */
        template<typename F>
        void ForEachArchetype(F&& callback) const
        {
            const auto& archetypes = archetype_graph_.GetArchetypes();
            for (size_t i = 0; i < archetypes.Size(); ++i)
            {
                if (archetypes[i]->EntityCount() > 0)
                {
                    callback(*archetypes[i]);
                }
            }
        }

        /**
         * Get raw component data for an entity by TypeId
         *
         * @return Pointer to component data, or nullptr if entity doesn't have that component
         */
        [[nodiscard]] void* GetComponentRaw(Entity entity, TypeId type_id) noexcept
        {
            if (!IsAlive(entity)) return nullptr;

            EntityRecord* record = entity_locations_.Get(entity);
            if (record == nullptr || record->archetype == nullptr) return nullptr;

            return record->archetype->GetComponentRaw(record->row, type_id);
        }

        /**
         * Iterate all component TypeIds on an entity
         *
         * Callback receives each TypeId in the entity's archetype.
         * Useful for generic inspection (editor, serialization).
         */
        template<typename F>
        void ForEachComponentType(Entity entity, F&& callback) const
        {
            if (!IsAlive(entity)) return;
            const EntityRecord* record = entity_locations_.Get(entity);
            if (record == nullptr || record->archetype == nullptr) return;
            const auto& types = record->archetype->GetComponentTypes();
            for (size_t i = 0; i < types.Size(); ++i)
                callback(types[i]);
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
         * Thread-safe: Protected by mutex during query construction.
         * Query iteration (Each/EachWithEntity) is lock-free after construction.
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
            std::lock_guard<HIVE_PROFILE_LOCKABLE_BASE(std::mutex)> lock{allocators_.PersistentMutex()};
            return queen::Query<PersistentAllocator, Terms...>{allocators_.Persistent(), component_index_};
        }

        /**
         * Execute a callback with a query, holding the lock for the entire lifetime
         *
         * Thread-safe: The mutex is held during query construction, iteration, AND destruction.
         * Use this for parallel system execution to avoid race conditions.
         *
         * @param callback Function to call with the query (e.g., query.Each(...))
         */
        template<typename... Terms, typename Callback>
        void QueryEach(Callback&& callback)
        {
            std::lock_guard<HIVE_PROFILE_LOCKABLE_BASE(std::mutex)> lock{allocators_.PersistentMutex()};
            queen::Query<PersistentAllocator, Terms...> query{allocators_.Persistent(), component_index_};
            callback(query);
            // Query destructor runs here while still holding the lock
        }

        /**
         * Execute Each() on a query with full lock protection
         *
         * Thread-safe version for parallel execution.
         */
        template<typename... Terms, typename F>
        void QueryEachLocked(F&& func)
        {
            std::lock_guard<HIVE_PROFILE_LOCKABLE_BASE(std::mutex)> lock{allocators_.PersistentMutex()};
            queen::Query<PersistentAllocator, Terms...> query{allocators_.Persistent(), component_index_};
            query.Each(std::forward<F>(func));
        }

        /**
         * Execute EachWithEntity() on a query with full lock protection
         *
         * Thread-safe version for parallel execution.
         */
        template<typename... Terms, typename F>
        void QueryEachWithEntityLocked(F&& func)
        {
            std::lock_guard<HIVE_PROFILE_LOCKABLE_BASE(std::mutex)> lock{allocators_.PersistentMutex()};
            queen::Query<PersistentAllocator, Terms...> query{allocators_.Persistent(), component_index_};
            query.EachWithEntity(std::forward<F>(func));
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

        void RunSystem(SystemId id)
        {
            systems_.RunSystem(*this, id, current_tick_);
        }

        void RunAllSystems()
        {
            systems_.RunAll(*this, current_tick_);
        }

        [[nodiscard]] size_t SystemCount() const noexcept
        {
            return systems_.SystemCount();
        }

        void SetSystemEnabled(SystemId id, bool enabled)
        {
            systems_.SetSystemEnabled(id, enabled);
        }

        [[nodiscard]] bool IsSystemEnabled(SystemId id) const noexcept
        {
            return systems_.IsSystemEnabled(id);
        }

        [[nodiscard]] SystemStorage<PersistentAllocator>& GetSystemStorage() noexcept
        {
            return systems_;
        }

        /**
         * Advance the world by one tick (run all systems, no frame marker)
         *
         * Same as Update() but without HIVE_PROFILE_FRAME. Use this in
         * fixed-timestep loops where multiple advances happen per rendered frame.
         * The caller is responsible for emitting HIVE_PROFILE_FRAME once per frame.
         */
        void Advance()
        {
            HIVE_PROFILE_SCOPE_N("World::Advance");
            IncrementTick();
            events_.SwapBuffers();
            scheduler_.RunAll(*this, systems_);
            allocators_.ResetFrame();
            HIVE_PROFILE_PLOT("World::EntityCount", static_cast<int64_t>(EntityCount()));
            HIVE_PROFILE_PLOT("World::ArchetypeCount", static_cast<int64_t>(ArchetypeCount()));
        }

        /**
         * Advance the world using parallel execution (no frame marker)
         *
         * Same as UpdateParallel() but without HIVE_PROFILE_FRAME.
         *
         * @param worker_count Number of worker threads (0 = auto-detect)
         */
        void AdvanceParallel(size_t worker_count = 0)
        {
            HIVE_PROFILE_SCOPE_N("World::AdvanceParallel");
            IncrementTick();
            events_.SwapBuffers();

            if (parallel_scheduler_ == nullptr)
            {
                void* mem = allocators_.Persistent().Allocate(
                    sizeof(ParallelScheduler<PersistentAllocator>),
                    alignof(ParallelScheduler<PersistentAllocator>)
                );
                parallel_scheduler_ = new (mem) ParallelScheduler<PersistentAllocator>{allocators_.Persistent(), worker_count};
            }

            parallel_scheduler_->RunAll(*this, systems_);
            allocators_.ResetFrame();
            allocators_.ResetThreadFrames();
            HIVE_PROFILE_PLOT("World::EntityCount", static_cast<int64_t>(EntityCount()));
            HIVE_PROFILE_PLOT("World::ArchetypeCount", static_cast<int64_t>(ArchetypeCount()));
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
            Advance();
            HIVE_PROFILE_FRAME;
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
            AdvanceParallel(worker_count);
            HIVE_PROFILE_FRAME;
        }

        [[nodiscard]] Scheduler<PersistentAllocator>& GetScheduler() noexcept
        {
            return scheduler_;
        }

        [[nodiscard]] const Scheduler<PersistentAllocator>& GetScheduler() const noexcept
        {
            return scheduler_;
        }

        [[nodiscard]] ParallelScheduler<PersistentAllocator>* GetParallelScheduler() noexcept
        {
            return parallel_scheduler_;
        }

        [[nodiscard]] const ParallelScheduler<PersistentAllocator>* GetParallelScheduler() const noexcept
        {
            return parallel_scheduler_;
        }

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

        [[nodiscard]] ObserverStorage<PersistentAllocator>& GetObserverStorage() noexcept
        {
            return observers_;
        }

        [[nodiscard]] const ObserverStorage<PersistentAllocator>& GetObserverStorage() const noexcept
        {
            return observers_;
        }

        void SetObserverEnabled(ObserverId id, bool enabled)
        {
            observers_.SetEnabled(id, enabled);
        }

        [[nodiscard]] bool IsObserverEnabled(ObserverId id) const noexcept
        {
            return observers_.IsEnabled(id);
        }

        [[nodiscard]] size_t ObserverCount() const noexcept
        {
            return observers_.ObserverCount();
        }

        // ─────────────────────────────────────────────────────────────
        // Hierarchy
        // ─────────────────────────────────────────────────────────────

        /**
         * Set the parent of an entity
         *
         * If the entity already has a parent, it is removed from the
         * old parent's children list first.
         *
         * @param child Entity to set parent for
         * @param parent Parent entity (must be alive)
         */
        void SetParent(Entity child, Entity parent)
        {
            hive::Assert(IsAlive(child), "Child entity must be alive");
            hive::Assert(IsAlive(parent), "Parent entity must be alive");
            hive::Assert(!(child == parent), "Entity cannot be its own parent");
            hive::Assert(!IsDescendantOf(parent, child), "SetParent would create a cycle");

            // 1. If child already has a parent, remove from old parent's children
            if (Parent* old_parent_comp = Get<Parent>(child))
            {
                Entity old_parent = old_parent_comp->entity;
                if (IsAlive(old_parent))
                {
                    if (Children* old_children = Get<Children>(old_parent))
                    {
                        old_children->Remove(child);

                        if (old_children->IsEmpty())
                        {
                            Remove<Children>(old_parent);
                        }
                    }
                }

                // Update existing Parent component (triggers OnSet)
                Set<Parent>(child, Parent{parent});
            }
            else
            {
                // Add new Parent component (triggers OnAdd)
                Add<Parent>(child, Parent{parent});
            }

            if (Children* children = Get<Children>(parent))
            {
                children->Add(child);
            }
            else
            {
                Children new_children{allocators_.Persistent()};
                new_children.Add(child);
                Add<Children>(parent, std::move(new_children));
            }
        }

        /**
         * Remove the parent from an entity (make it a root)
         *
         * @param child Entity to remove parent from
         */
        void RemoveParent(Entity child)
        {
            if (!IsAlive(child))
            {
                return;
            }

            Parent* parent_comp = Get<Parent>(child);
            if (parent_comp == nullptr)
            {
                return;
            }

            Entity parent = parent_comp->entity;

            if (IsAlive(parent))
            {
                if (Children* children = Get<Children>(parent))
                {
                    children->Remove(child);

                    if (children->IsEmpty())
                    {
                        Remove<Children>(parent);
                    }
                }
            }

            Remove<Parent>(child);
        }

        /**
         * Get the parent of an entity
         *
         * @param child Entity to get parent of
         * @return Parent entity, or Entity::Invalid() if no parent
         */
        [[nodiscard]] Entity GetParent(Entity child) const noexcept
        {
            const Parent* parent_comp = Get<Parent>(child);
            if (parent_comp == nullptr)
            {
                return Entity::Invalid();
            }
            return parent_comp->entity;
        }

        [[nodiscard]] bool HasParent(Entity child) const noexcept
        {
            return Has<Parent>(child);
        }

        /**
         * Get children component of an entity
         *
         * @param parent Entity to get children of
         * @return Pointer to Children component, or nullptr if no children
         */
        [[nodiscard]] Children* GetChildren(Entity parent) noexcept
        {
            return Get<Children>(parent);
        }

        [[nodiscard]] const Children* GetChildren(Entity parent) const noexcept
        {
            return Get<Children>(parent);
        }

        [[nodiscard]] size_t ChildCount(Entity parent) const noexcept
        {
            const Children* children = Get<Children>(parent);
            if (children == nullptr)
            {
                return 0;
            }
            return children->Count();
        }

        /**
         * Iterate over all children of an entity
         *
         * @param parent Parent entity
         * @param callback Called for each child
         */
        template<typename F>
        void ForEachChild(Entity parent, F&& callback)
        {
            const Children* children = Get<Children>(parent);
            if (children == nullptr)
            {
                return;
            }

            for (size_t i = 0; i < children->Count(); ++i)
            {
                callback(children->At(i));
            }
        }

        /**
         * Iterate over all descendants (recursive, depth-first)
         *
         * @param root Root entity
         * @param callback Called for each descendant (not including root)
         */
        template<typename F>
        void ForEachDescendant(Entity root, F&& callback)
        {
            // Depth-first traversal using frame allocator stack
            wax::Vector<Entity, FrameAllocator> stack{GetFrameAllocator()};

            const Children* root_children = Get<Children>(root);
            if (root_children != nullptr)
            {
                for (size_t i = 0; i < root_children->Count(); ++i)
                {
                    stack.PushBack(root_children->At(i));
                }
            }

            while (!stack.IsEmpty())
            {
                Entity current = stack.Back();
                stack.PopBack();

                callback(current);

                const Children* children = Get<Children>(current);
                if (children != nullptr)
                {
                    for (size_t i = 0; i < children->Count(); ++i)
                    {
                        stack.PushBack(children->At(i));
                    }
                }
            }
        }

        /**
         * Check if an entity is a descendant of another
         */
        [[nodiscard]] bool IsDescendantOf(Entity entity, Entity ancestor) const noexcept
        {
            static constexpr uint32_t kMaxHierarchyDepth = 1024;
            Entity current = entity;
            for (uint32_t i = 0; i < kMaxHierarchyDepth; ++i)
            {
                const Parent* parent_comp = Get<Parent>(current);
                if (parent_comp == nullptr || !parent_comp->IsValid())
                {
                    return false;
                }

                if (parent_comp->entity == ancestor)
                {
                    return true;
                }

                current = parent_comp->entity;
            }
            hive::Assert(false, "Hierarchy depth exceeds maximum - possible cycle");
            return false;
        }

        [[nodiscard]] Entity GetRoot(Entity entity) const noexcept
        {
            static constexpr uint32_t kMaxHierarchyDepth = 1024;
            Entity current = entity;
            for (uint32_t i = 0; i < kMaxHierarchyDepth; ++i)
            {
                const Parent* parent_comp = Get<Parent>(current);
                if (parent_comp == nullptr || !parent_comp->IsValid())
                {
                    return current;
                }
                current = parent_comp->entity;
            }
            hive::Assert(false, "Hierarchy depth exceeds maximum - possible cycle");
            return entity;
        }

        [[nodiscard]] uint32_t GetDepth(Entity entity) const noexcept
        {
            static constexpr uint32_t kMaxHierarchyDepth = 1024;
            uint32_t depth = 0;
            Entity current = entity;
            for (uint32_t i = 0; i < kMaxHierarchyDepth; ++i)
            {
                const Parent* parent_comp = Get<Parent>(current);
                if (parent_comp == nullptr || !parent_comp->IsValid())
                {
                    return depth;
                }
                ++depth;
                current = parent_comp->entity;
            }
            hive::Assert(false, "Hierarchy depth exceeds maximum - possible cycle");
            return depth;
        }

        /**
         * Despawn an entity and all its descendants
         *
         * Children are despawned first (depth-first), then the entity itself.
         */
        void DespawnRecursive(Entity entity)
        {
            HIVE_PROFILE_SCOPE_N("World::DespawnRecursive");
            if (!IsAlive(entity))
            {
                return;
            }

            // Collect all descendants first to avoid modification during iteration
            wax::Vector<Entity, FrameAllocator> to_despawn{GetFrameAllocator()};
            ForEachDescendant(entity, [&to_despawn](Entity descendant) {
                to_despawn.PushBack(descendant);
            });

            // Despawn in reverse order (deepest first)
            for (size_t i = to_despawn.Size(); i > 0; --i)
            {
                Entity descendant = to_despawn[i - 1];
                // Remove from parent's children (parent is still alive at this point)
                RemoveParent(descendant);
                Despawn(descendant);
            }

            RemoveParent(entity);
            Despawn(entity);
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
            HIVE_PROFILE_SCOPE_N("World::MoveEntity");
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
        explicit EntityBuilder(World& world)
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
            HIVE_PROFILE_SCOPE_N("World::Spawn");
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
        HIVE_PROFILE_SCOPE_N("CommandBuffer::Flush");
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
        ClearBlocks();
    }
}

// Include method implementations now that World is complete
#include <queen/system/system_builder_impl.h>
#include <queen/observer/observer_storage_impl.h>
#include <queen/scheduler/scheduler_impl.h>
#include <queen/scheduler/parallel_scheduler_impl.h>
