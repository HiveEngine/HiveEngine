#pragma once

#include <hive/core/assert.h>
#include <hive/profiling/profiler.h>

#include <comb/allocator_concepts.h>

#include <wax/containers/hash_map.h>
#include <wax/containers/vector.h>

#include <queen/command/commands.h>
#include <queen/core/component_info.h>
#include <queen/core/entity.h>
#include <queen/core/entity_allocator.h>
#include <queen/core/entity_location.h>
#include <queen/core/tick.h>
#include <queen/core/type_id.h>
#include <queen/event/events.h>
#include <queen/hierarchy/hierarchy.h>
#include <queen/observer/observers.h>
#include <queen/query/query.h>
#include <queen/scheduler/parallel_scheduler.h>
#include <queen/scheduler/scheduler.h>
#include <queen/storage/archetype.h>
#include <queen/storage/archetype_graph.h>
#include <queen/storage/component_index.h>
#include <queen/system/system_storage.h>
#include <queen/world/world_allocators.h>

#include <cstring>
#include <mutex>

namespace queen
{
    class EntityBuilder;

    template <comb::Allocator Allocator> class CommandBuffer;

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
     *   world.UpdateParallel(jobs);  // Parallel via Drone job system
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
            : m_allocators{config.m_persistentSize, config.m_componentSize, config.m_frameSize,
                           config.m_threadFrameSize, config.m_threadCount}
            , m_entityAllocator{m_allocators.Components()}
            , m_entityLocations{m_allocators.Components()}
            , m_archetypeGraph{m_allocators.Components()}
            , m_componentIndex{m_allocators.Persistent()}
            , m_resources{m_allocators.Persistent()}
            , m_resourceMetas{m_allocators.Persistent()}
            , m_systems{m_allocators.Persistent()}
            , m_scheduler{m_allocators.Persistent()}
            , m_commands{m_allocators.Persistent()}
            , m_events{m_allocators.Persistent()}
            , m_observers{m_allocators.Persistent()}
        {
            m_componentIndex.RegisterArchetype(m_archetypeGraph.GetEmptyArchetype());
        }

        ~World()
        {
            if (m_parallelScheduler != nullptr)
            {
                m_parallelScheduler->~ParallelScheduler<PersistentAllocator>();
                m_allocators.Persistent().Deallocate(m_parallelScheduler);
                m_parallelScheduler = nullptr;
            }

            for (auto it = m_resources.Begin(); it != m_resources.End(); ++it)
            {
                TypeId typeId = it.Key();
                void* data = it.Value();

                for (size_t i = 0; i < m_resourceMetas.Size(); ++i)
                {
                    if (m_resourceMetas[i].m_typeId == typeId)
                    {
                        if (m_resourceMetas[i].m_destruct != nullptr)
                        {
                            m_resourceMetas[i].m_destruct(data);
                        }
                        break;
                    }
                }

                m_allocators.Persistent().Deallocate(data);
            }
        }

        World(const World&) = delete;
        World& operator=(const World&) = delete;
        World(World&&) = delete; // WorldAllocators contains mutex (not movable)
        World& operator=(World&&) = delete;

        [[nodiscard]] EntityBuilder Spawn();

        template <typename... Components> [[nodiscard]] Entity Spawn(Components&&... components);

        void Despawn(Entity entity)
        {
            HIVE_PROFILE_SCOPE_N("World::Despawn");
            if (!IsAlive(entity))
            {
                return;
            }

            EntityRecord* record = m_entityLocations.Get(entity);
            if (record == nullptr)
            {
                return;
            }

            Archetype<ComponentAllocator>* archetype = record->m_archetype;
            uint32_t row = record->m_row;

            Entity moved = archetype->FreeRow(row);

            if (!moved.IsNull() && !(moved == entity))
            {
                EntityRecord* movedRecord = m_entityLocations.Get(moved);
                if (movedRecord != nullptr)
                {
                    movedRecord->m_row = row;
                }
            }

            m_entityLocations.Remove(entity);
            m_entityAllocator.Deallocate(entity);
        }

        [[nodiscard]] bool IsAlive(Entity entity) const noexcept
        {
            return m_entityAllocator.IsAlive(entity);
        }

        template <typename T> [[nodiscard]] T* Get(Entity entity) noexcept
        {
            if (!IsAlive(entity))
            {
                return nullptr;
            }

            EntityRecord* record = m_entityLocations.Get(entity);
            if (record == nullptr || record->m_archetype == nullptr)
            {
                return nullptr;
            }

            return record->m_archetype->template GetComponent<T>(record->m_row);
        }

        template <typename T> [[nodiscard]] const T* Get(Entity entity) const noexcept
        {
            if (!IsAlive(entity))
            {
                return nullptr;
            }

            const EntityRecord* record = m_entityLocations.Get(entity);
            if (record == nullptr || record->m_archetype == nullptr)
            {
                return nullptr;
            }

            return record->m_archetype->template GetComponent<T>(record->m_row);
        }

        template <typename T> [[nodiscard]] bool Has(Entity entity) const noexcept
        {
            if (!IsAlive(entity))
            {
                return false;
            }

            const EntityRecord* record = m_entityLocations.Get(entity);
            if (record == nullptr || record->m_archetype == nullptr)
            {
                return false;
            }

            return record->m_archetype->template HasComponent<T>();
        }

        [[nodiscard]] bool HasComponent(Entity entity, TypeId typeId) const noexcept
        {
            if (!IsAlive(entity))
            {
                return false;
            }

            const EntityRecord* record = m_entityLocations.Get(entity);
            if (record == nullptr || record->m_archetype == nullptr)
            {
                return false;
            }

            return record->m_archetype->HasComponent(typeId);
        }

        template <typename T> void Add(Entity entity, T&& component)
        {
            HIVE_PROFILE_SCOPE_N("World::Add");
            if (!IsAlive(entity))
            {
                return;
            }

            EntityRecord* record = m_entityLocations.Get(entity);
            if (record == nullptr || record->m_archetype == nullptr)
            {
                return;
            }

            Archetype<ComponentAllocator>* oldArch = record->m_archetype;

            if (oldArch->template HasComponent<T>())
            {
                oldArch->template SetComponent<T>(record->m_row, std::forward<T>(component));
                const T* comp = oldArch->template GetComponent<T>(record->m_row);
                m_observers.template Trigger<OnSet<T>>(*this, entity, comp);
                return;
            }

            Archetype<ComponentAllocator>* newArch = m_archetypeGraph.template GetOrCreateAddTarget<T>(*oldArch);

            if (newArch != oldArch)
            {
                RegisterNewArchetype(newArch);
            }

            MoveEntity(entity, *record, oldArch, newArch);

            newArch->template SetComponent<T>(record->m_row, std::forward<T>(component));

            const T* comp = newArch->template GetComponent<T>(record->m_row);
            m_observers.template Trigger<OnAdd<T>>(*this, entity, comp);
        }

        template <typename T> void Remove(Entity entity)
        {
            HIVE_PROFILE_SCOPE_N("World::Remove");
            if (!IsAlive(entity))
            {
                return;
            }

            EntityRecord* record = m_entityLocations.Get(entity);
            if (record == nullptr || record->m_archetype == nullptr)
            {
                return;
            }

            Archetype<ComponentAllocator>* oldArch = record->m_archetype;

            if (!oldArch->template HasComponent<T>())
            {
                return;
            }

            // Trigger OnRemove observers BEFORE removing (so they can access the data)
            const T* comp = oldArch->template GetComponent<T>(record->m_row);
            m_observers.template Trigger<OnRemove<T>>(*this, entity, comp);

            Archetype<ComponentAllocator>* newArch = m_archetypeGraph.template GetOrCreateRemoveTarget<T>(*oldArch);

            if (newArch != oldArch)
            {
                RegisterNewArchetype(newArch);
            }

            MoveEntity(entity, *record, oldArch, newArch);
        }

        template <typename T> void Set(Entity entity, T&& component)
        {
            Add<T>(entity, std::forward<T>(component));
        }

        [[nodiscard]] size_t EntityCount() const noexcept
        {
            return m_entityAllocator.AliveCount();
        }

        [[nodiscard]] size_t ArchetypeCount() const noexcept
        {
            return m_archetypeGraph.ArchetypeCount();
        }

        /**
         * Iterate over all non-empty archetypes
         *
         * @param callback Called for each archetype that contains at least one entity
         */
        template <typename F> void ForEachArchetype(F&& callback) const
        {
            const auto& archetypes = m_archetypeGraph.GetArchetypes();
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
        [[nodiscard]] void* GetComponentRaw(Entity entity, TypeId typeId) noexcept
        {
            if (!IsAlive(entity))
                return nullptr;

            EntityRecord* record = m_entityLocations.Get(entity);
            if (record == nullptr || record->m_archetype == nullptr)
                return nullptr;

            return record->m_archetype->GetComponentRaw(record->m_row, typeId);
        }

        /**
         * Iterate all component TypeIds on an entity
         *
         * Callback receives each TypeId in the entity's archetype.
         * Useful for generic inspection (editor, serialization).
         */
        template <typename F> void ForEachComponentType(Entity entity, F&& callback) const
        {
            if (!IsAlive(entity))
                return;
            const EntityRecord* record = m_entityLocations.Get(entity);
            if (record == nullptr || record->m_archetype == nullptr)
                return;
            const auto& types = record->m_archetype->GetComponentTypes();
            for (size_t i = 0; i < types.Size(); ++i)
                callback(types[i]);
        }

        // Resources (global singletons)

        template <typename T> void InsertResource(T&& resource)
        {
            using DecayedT = std::decay_t<T>;
            TypeId typeId = TypeIdOf<DecayedT>();

            void** existing = m_resources.Find(typeId);
            if (existing != nullptr)
            {
                auto* typed = static_cast<DecayedT*>(*existing);
                *typed = std::forward<T>(resource);
                return;
            }

            void* data = m_allocators.Persistent().Allocate(sizeof(DecayedT), alignof(DecayedT));
            hive::Assert(data != nullptr, "Failed to allocate resource");

            new (data) DecayedT{std::forward<T>(resource)};

            m_resources.Insert(typeId, data);
            m_resourceMetas.PushBack(ComponentMeta::Of<DecayedT>());
        }

        template <typename T> [[nodiscard]] T* Resource() noexcept
        {
            TypeId typeId = TypeIdOf<T>();

            void** found = m_resources.Find(typeId);
            if (found == nullptr)
            {
                return nullptr;
            }

            return static_cast<T*>(*found);
        }

        template <typename T> [[nodiscard]] const T* Resource() const noexcept
        {
            TypeId typeId = TypeIdOf<T>();

            void* const* found = m_resources.Find(typeId);
            if (found == nullptr)
            {
                return nullptr;
            }

            return static_cast<const T*>(*found);
        }

        template <typename T> [[nodiscard]] bool HasResource() const noexcept
        {
            TypeId typeId = TypeIdOf<T>();
            return m_resources.Contains(typeId);
        }

        template <typename T> void RemoveResource()
        {
            TypeId typeId = TypeIdOf<T>();

            void** found = m_resources.Find(typeId);
            if (found == nullptr)
            {
                return;
            }

            void* data = *found;

            for (size_t i = 0; i < m_resourceMetas.Size(); ++i)
            {
                if (m_resourceMetas[i].m_typeId == typeId)
                {
                    if (m_resourceMetas[i].m_destruct != nullptr)
                    {
                        m_resourceMetas[i].m_destruct(data);
                    }

                    if (i < m_resourceMetas.Size() - 1)
                    {
                        m_resourceMetas[i] = m_resourceMetas[m_resourceMetas.Size() - 1];
                    }
                    m_resourceMetas.PopBack();
                    break;
                }
            }

            m_allocators.Persistent().Deallocate(data);
            m_resources.Remove(typeId);
        }

        [[nodiscard]] size_t ResourceCount() const noexcept
        {
            return m_resources.Count();
        }

        // Allocator Access

        [[nodiscard]] WorldAllocators& GetAllocators() noexcept
        {
            return m_allocators;
        }

        [[nodiscard]] const WorldAllocators& GetAllocators() const noexcept
        {
            return m_allocators;
        }

        /**
         * Get the persistent allocator (for long-lived metadata)
         */
        [[nodiscard]] PersistentAllocator& GetPersistentAllocator() noexcept
        {
            return m_allocators.Persistent();
        }

        /**
         * Get the component allocator (for entity data)
         */
        [[nodiscard]] ComponentAllocator& GetComponentAllocator() noexcept
        {
            return m_allocators.Components();
        }

        /**
         * Get the frame allocator (for per-frame temporary data)
         */
        [[nodiscard]] FrameAllocator& GetFrameAllocator() noexcept
        {
            return m_allocators.Frame();
        }

        /**
         * Get a thread-local allocator for parallel execution
         */
        [[nodiscard]] FrameAllocator& GetThreadAllocator(size_t threadIndex) noexcept
        {
            return m_allocators.ThreadFrame(threadIndex);
        }

        [[nodiscard]] ArchetypeGraph<ComponentAllocator>& GetArchetypeGraph() noexcept
        {
            return m_archetypeGraph;
        }

        [[nodiscard]] ComponentIndex<PersistentAllocator>& GetComponentIndex() noexcept
        {
            return m_componentIndex;
        }

        // Queries

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
        template <typename... Terms> [[nodiscard]] queen::Query<PersistentAllocator, Terms...> Query()
        {
            std::lock_guard<HIVE_PROFILE_LOCKABLE_BASE(std::mutex)> lock{m_allocators.PersistentMutex()};
            return queen::Query<PersistentAllocator, Terms...>{m_allocators.Persistent(), m_componentIndex};
        }

        /**
         * Execute a callback with a query, holding the lock for the entire lifetime
         *
         * Thread-safe: The mutex is held during query construction, iteration, AND destruction.
         * Use this for parallel system execution to avoid race conditions.
         *
         * @param callback Function to call with the query (e.g., query.Each(...))
         */
        template <typename... Terms, typename Callback> void QueryEach(Callback&& callback)
        {
            std::lock_guard<HIVE_PROFILE_LOCKABLE_BASE(std::mutex)> lock{m_allocators.PersistentMutex()};
            queen::Query<PersistentAllocator, Terms...> query{m_allocators.Persistent(), m_componentIndex};
            callback(query);
            // Query destructor runs here while still holding the lock
        }

        /**
         * Execute Each() on a query with full lock protection
         *
         * Thread-safe version for parallel execution.
         */
        template <typename... Terms, typename F> void QueryEachLocked(F&& func)
        {
            std::lock_guard<HIVE_PROFILE_LOCKABLE_BASE(std::mutex)> lock{m_allocators.PersistentMutex()};
            queen::Query<PersistentAllocator, Terms...> query{m_allocators.Persistent(), m_componentIndex};
            query.Each(std::forward<F>(func));
        }

        /**
         * Execute EachWithEntity() on a query with full lock protection
         *
         * Thread-safe version for parallel execution.
         */
        template <typename... Terms, typename F> void QueryEachWithEntityLocked(F&& func)
        {
            std::lock_guard<HIVE_PROFILE_LOCKABLE_BASE(std::mutex)> lock{m_allocators.PersistentMutex()};
            queen::Query<PersistentAllocator, Terms...> query{m_allocators.Persistent(), m_componentIndex};
            query.EachWithEntity(std::forward<F>(func));
        }

        // Systems

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
        template <typename... Terms> [[nodiscard]] SystemBuilder<PersistentAllocator, Terms...> System(const char* name)
        {
            return m_systems.template Register<Terms...>(*this, name);
        }

        void RunSystem(SystemId id)
        {
            m_systems.RunSystem(*this, id, m_currentTick);
        }

        void RunAllSystems()
        {
            m_systems.RunAll(*this, m_currentTick);
        }

        [[nodiscard]] size_t SystemCount() const noexcept
        {
            return m_systems.SystemCount();
        }

        void SetSystemEnabled(SystemId id, bool enabled)
        {
            m_systems.SetSystemEnabled(id, enabled);
        }

        [[nodiscard]] bool IsSystemEnabled(SystemId id) const noexcept
        {
            return m_systems.IsSystemEnabled(id);
        }

        void RemoveSystem(SystemId id)
        {
            m_systems.RemoveSystem(id);
            InvalidateScheduler();
        }

        bool RemoveSystemByName(const char* name)
        {
            if (m_systems.RemoveSystemByName(name))
            {
                InvalidateScheduler();
                return true;
            }
            return false;
        }

        void ClearSystems()
        {
            m_systems.Clear();
            InvalidateScheduler();
        }

        [[nodiscard]] SystemStorage<PersistentAllocator>& GetSystemStorage() noexcept
        {
            return m_systems;
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
            m_events.SwapBuffers();
            m_scheduler.RunAll(*this, m_systems);
            m_allocators.ResetFrame();
            HIVE_PROFILE_PLOT("World::EntityCount", static_cast<int64_t>(EntityCount()));
            HIVE_PROFILE_PLOT("World::ArchetypeCount", static_cast<int64_t>(ArchetypeCount()));
        }

        /**
         * Advance the world using parallel execution (no frame marker)
         *
         * Same as UpdateParallel() but without HIVE_PROFILE_FRAME.
         *
         * @param jobs Drone job submitter for parallel execution
         */
        void AdvanceParallel(drone::JobSubmitter jobs)
        {
            HIVE_PROFILE_SCOPE_N("World::AdvanceParallel");
            IncrementTick();
            m_events.SwapBuffers();

            if (m_parallelScheduler == nullptr)
            {
                void* mem = m_allocators.Persistent().Allocate(sizeof(ParallelScheduler<PersistentAllocator>),
                                                               alignof(ParallelScheduler<PersistentAllocator>));
                m_parallelScheduler = new (mem) ParallelScheduler<PersistentAllocator>{m_allocators.Persistent(), jobs};
            }

            m_parallelScheduler->RunAll(*this, m_systems);
            m_allocators.ResetFrame();
            m_allocators.ResetThreadFrames();
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
         * @param jobs Drone job submitter for parallel execution
         */
        void UpdateParallel(drone::JobSubmitter jobs)
        {
            AdvanceParallel(jobs);
            HIVE_PROFILE_FRAME;
        }

        [[nodiscard]] Scheduler<PersistentAllocator>& GetScheduler() noexcept
        {
            return m_scheduler;
        }

        [[nodiscard]] const Scheduler<PersistentAllocator>& GetScheduler() const noexcept
        {
            return m_scheduler;
        }

        [[nodiscard]] ParallelScheduler<PersistentAllocator>* GetParallelScheduler() noexcept
        {
            return m_parallelScheduler;
        }

        [[nodiscard]] const ParallelScheduler<PersistentAllocator>* GetParallelScheduler() const noexcept
        {
            return m_parallelScheduler;
        }

        [[nodiscard]] bool HasParallelScheduler() const noexcept
        {
            return m_parallelScheduler != nullptr;
        }

        /**
         * Invalidate the scheduler's dependency graph
         *
         * Call this when systems are added or modified to force rebuild.
         * Invalidates both sequential and parallel schedulers.
         */
        void InvalidateScheduler() noexcept
        {
            m_scheduler.Invalidate();
            if (m_parallelScheduler != nullptr)
            {
                m_parallelScheduler->Invalidate();
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
            return m_commands;
        }

        [[nodiscard]] const Commands<PersistentAllocator>& GetCommands() const noexcept
        {
            return m_commands;
        }

        // Events

        [[nodiscard]] Events<PersistentAllocator>& GetEvents() noexcept
        {
            return m_events;
        }

        [[nodiscard]] const Events<PersistentAllocator>& GetEvents() const noexcept
        {
            return m_events;
        }

        /**
         * Send an event (adds to current frame's queue)
         *
         * @tparam E Event type
         * @param event The event to send
         */
        template <typename E> void SendEvent(E&& event)
        {
            m_events.template Send<std::decay_t<E>>(std::forward<E>(event));
        }

        /**
         * Get an EventWriter for a specific event type
         *
         * @tparam E Event type
         * @return EventWriter for sending events
         */
        template <typename E> [[nodiscard]] EventWriter<E, PersistentAllocator> EventWriter()
        {
            return m_events.template Writer<E>();
        }

        /**
         * Get an EventReader for a specific event type
         *
         * @tparam E Event type
         * @return EventReader for reading events
         */
        template <typename E> [[nodiscard]] queen::EventReader<E, PersistentAllocator> EventReader()
        {
            return m_events.template Reader<E>();
        }

        // Observers

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
        template <ObserverTrigger TriggerEvent>
        [[nodiscard]] ObserverBuilder<TriggerEvent, PersistentAllocator> Observer(const char* name)
        {
            return m_observers.template Register<TriggerEvent>(*this, name);
        }

        [[nodiscard]] ObserverStorage<PersistentAllocator>& GetObserverStorage() noexcept
        {
            return m_observers;
        }

        [[nodiscard]] const ObserverStorage<PersistentAllocator>& GetObserverStorage() const noexcept
        {
            return m_observers;
        }

        void SetObserverEnabled(ObserverId id, bool enabled)
        {
            m_observers.SetEnabled(id, enabled);
        }

        [[nodiscard]] bool IsObserverEnabled(ObserverId id) const noexcept
        {
            return m_observers.IsEnabled(id);
        }

        [[nodiscard]] size_t ObserverCount() const noexcept
        {
            return m_observers.ObserverCount();
        }

        // Hierarchy

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
            if (Parent* oldParentComp = Get<Parent>(child))
            {
                Entity oldParent = oldParentComp->m_entity;
                if (IsAlive(oldParent))
                {
                    if (Children* oldChildren = Get<Children>(oldParent))
                    {
                        oldChildren->Remove(child);

                        if (oldChildren->IsEmpty())
                        {
                            Remove<Children>(oldParent);
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
                Children newChildren{m_allocators.Persistent()};
                newChildren.Add(child);
                Add<Children>(parent, std::move(newChildren));
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

            Parent* parentComp = Get<Parent>(child);
            if (parentComp == nullptr)
            {
                return;
            }

            Entity parent = parentComp->m_entity;

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
            const Parent* parentComp = Get<Parent>(child);
            if (parentComp == nullptr)
            {
                return Entity::Invalid();
            }
            return parentComp->m_entity;
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
        template <typename F> void ForEachChild(Entity parent, F&& callback)
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
        template <typename F> void ForEachDescendant(Entity root, F&& callback)
        {
            // Depth-first traversal using frame allocator stack
            wax::Vector<Entity> stack{GetFrameAllocator()};

            const Children* rootChildren = Get<Children>(root);
            if (rootChildren != nullptr)
            {
                for (size_t i = 0; i < rootChildren->Count(); ++i)
                {
                    stack.PushBack(rootChildren->At(i));
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
                const Parent* parentComp = Get<Parent>(current);
                if (parentComp == nullptr || !parentComp->IsValid())
                {
                    return false;
                }

                if (parentComp->m_entity == ancestor)
                {
                    return true;
                }

                current = parentComp->m_entity;
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
                const Parent* parentComp = Get<Parent>(current);
                if (parentComp == nullptr || !parentComp->IsValid())
                {
                    return current;
                }
                current = parentComp->m_entity;
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
                const Parent* parentComp = Get<Parent>(current);
                if (parentComp == nullptr || !parentComp->IsValid())
                {
                    return depth;
                }
                ++depth;
                current = parentComp->m_entity;
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
            wax::Vector<Entity> toDespawn{GetFrameAllocator()};
            ForEachDescendant(entity, [&toDespawn](Entity descendant) { toDespawn.PushBack(descendant); });

            // Despawn in reverse order (deepest first)
            for (size_t i = toDespawn.Size(); i > 0; --i)
            {
                Entity descendant = toDespawn[i - 1];
                // Remove from parent's children (parent is still alive at this point)
                RemoveParent(descendant);
                Despawn(descendant);
            }

            RemoveParent(entity);
            Despawn(entity);
        }

        // Change Detection

        /**
         * Get the current world tick
         */
        [[nodiscard]] Tick CurrentTick() const noexcept
        {
            return m_currentTick;
        }

        /**
         * Increment the world tick
         *
         * Called automatically at the start of Update(). Can also be called
         * manually to advance the tick without running systems.
         */
        void IncrementTick() noexcept
        {
            ++m_currentTick;
        }

    private:
        friend class EntityBuilder;

        template <comb::Allocator OtherAlloc> friend class CommandBuffer;

        Entity AllocateEntity()
        {
            return m_entityAllocator.Allocate();
        }

        void PlaceEntity(Entity entity, Archetype<ComponentAllocator>* archetype)
        {
            uint32_t row = archetype->AllocateRow(entity, m_currentTick);
            m_entityLocations.Set(entity, EntityRecord{archetype, row});
        }

        void RegisterNewArchetype(Archetype<ComponentAllocator>* archetype)
        {
            if (archetype->ComponentCount() > 0)
            {
                bool alreadyRegistered = false;
                const auto* list = m_componentIndex.GetArchetypesWith(archetype->GetComponentTypes()[0]);
                if (list != nullptr)
                {
                    for (size_t i = 0; i < list->Size(); ++i)
                    {
                        if ((*list)[i] == archetype)
                        {
                            alreadyRegistered = true;
                            break;
                        }
                    }
                }

                if (!alreadyRegistered)
                {
                    m_componentIndex.RegisterArchetype(archetype);
                }
            }
        }

        void MoveEntity(Entity entity, EntityRecord& record, Archetype<ComponentAllocator>* oldArch,
                        Archetype<ComponentAllocator>* newArch)
        {
            HIVE_PROFILE_SCOPE_N("World::MoveEntity");
            uint32_t oldRow = record.m_row;

            uint32_t newRow = newArch->AllocateRow(entity, m_currentTick);

            const auto& oldMetas = oldArch->GetComponentMetas();

            for (size_t i = 0; i < oldMetas.Size(); ++i)
            {
                TypeId typeId = oldMetas[i].m_typeId;

                if (newArch->HasComponent(typeId))
                {
                    void* src = oldArch->GetComponentRaw(oldRow, typeId);
                    void* dst = newArch->GetComponentRaw(newRow, typeId);
                    if (oldMetas[i].m_move != nullptr)
                        oldMetas[i].m_move(dst, src);
                    else
                        std::memcpy(dst, src, oldMetas[i].m_size);
                }
            }

            Entity moved = oldArch->FreeRow(oldRow);

            if (!moved.IsNull() && !(moved == entity))
            {
                EntityRecord* movedRecord = m_entityLocations.Get(moved);
                if (movedRecord != nullptr)
                {
                    movedRecord->m_row = oldRow;
                }
            }

            record.m_archetype = newArch;
            record.m_row = newRow;
        }

        // IMPORTANT: allocators_ MUST be first for initialization order
        WorldAllocators m_allocators;

        EntityAllocator<ComponentAllocator> m_entityAllocator;
        EntityLocationMap<ComponentAllocator, Archetype<ComponentAllocator>> m_entityLocations;
        ArchetypeGraph<ComponentAllocator> m_archetypeGraph;
        ComponentIndex<PersistentAllocator> m_componentIndex;

        wax::HashMap<TypeId, void*> m_resources;
        wax::Vector<ComponentMeta> m_resourceMetas;

        SystemStorage<PersistentAllocator> m_systems;
        Scheduler<PersistentAllocator> m_scheduler;
        ParallelScheduler<PersistentAllocator>* m_parallelScheduler{nullptr};
        Commands<PersistentAllocator> m_commands;
        Events<PersistentAllocator> m_events;
        ObserverStorage<PersistentAllocator> m_observers;
        Tick m_currentTick{1}; // Start at 1 so tick 0 means "never changed"
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
            : m_world{&world}
            , m_pendingMetas{world.GetFrameAllocator()}
            , m_pendingData{world.GetFrameAllocator()}
        {
        }

        template <typename T> EntityBuilder& With(T&& component)
        {
            TypeId typeId = TypeIdOf<T>();

            for (size_t i = 0; i < m_pendingMetas.Size(); ++i)
            {
                if (m_pendingMetas[i].m_typeId == typeId)
                {
                    T* existing = static_cast<T*>(m_pendingData[i]);
                    *existing = std::forward<T>(component);
                    return *this;
                }
            }

            m_pendingMetas.PushBack(ComponentMeta::Of<T>());

            void* data = m_world->GetFrameAllocator().Allocate(sizeof(T), alignof(T));
            new (data) T{std::forward<T>(component)};
            m_pendingData.PushBack(data);

            return *this;
        }

        EntityBuilder& WithRaw(const ComponentMeta& meta, void* sourceData)
        {
            TypeId typeId = meta.m_typeId;

            for (size_t i = 0; i < m_pendingMetas.Size(); ++i)
            {
                if (m_pendingMetas[i].m_typeId == typeId)
                {
                    if (meta.m_move != nullptr)
                    {
                        meta.m_move(m_pendingData[i], sourceData);
                    }
                    else
                    {
                        std::memcpy(m_pendingData[i], sourceData, meta.m_size);
                    }
                    return *this;
                }
            }

            m_pendingMetas.PushBack(meta);

            void* data = m_world->GetFrameAllocator().Allocate(meta.m_size, meta.m_alignment);
            if (meta.m_move != nullptr)
            {
                meta.m_move(data, sourceData);
            }
            else
            {
                std::memcpy(data, sourceData, meta.m_size);
            }
            m_pendingData.PushBack(data);

            return *this;
        }

        Entity Build()
        {
            HIVE_PROFILE_SCOPE_N("World::Spawn");
            Entity entity = m_world->AllocateEntity();

            Archetype<ComponentAllocator>* archetype = m_world->m_archetypeGraph.GetEmptyArchetype();

            for (size_t i = 0; i < m_pendingMetas.Size(); ++i)
            {
                archetype = m_world->m_archetypeGraph.GetOrCreateAddTarget(*archetype, m_pendingMetas[i]);
            }

            m_world->RegisterNewArchetype(archetype);

            m_world->PlaceEntity(entity, archetype);

            auto* record = m_world->m_entityLocations.Get(entity);
            for (size_t i = 0; i < m_pendingMetas.Size(); ++i)
            {
                TypeId typeId = m_pendingMetas[i].m_typeId;
                archetype->SetComponent(record->m_row, typeId, m_pendingData[i]);

                if (m_pendingMetas[i].m_destruct != nullptr)
                {
                    m_pendingMetas[i].m_destruct(m_pendingData[i]);
                }
            }

            m_pendingMetas.Clear();
            m_pendingData.Clear();

            return entity;
        }

    private:
        World* m_world;
        wax::Vector<ComponentMeta> m_pendingMetas;
        wax::Vector<void*> m_pendingData;
    };

    inline EntityBuilder World::Spawn()
    {
        return EntityBuilder{*this};
    }

    template <typename... Components> Entity World::Spawn(Components&&... components)
    {
        EntityBuilder builder{*this};
        (builder.With(std::forward<Components>(components)), ...);
        return builder.Build();
    }

    // CommandBuffer::Flush implementation (here to avoid circular dependency)

    template <comb::Allocator Allocator> void CommandBuffer<Allocator>::Flush(World& world)
    {
        HIVE_PROFILE_SCOPE_N("CommandBuffer::Flush");
        m_spawnedEntities.Clear();
        m_spawnedEntities.Reserve(m_spawnCount);

        for (uint32_t i = 0; i < m_spawnCount; ++i)
        {
            m_spawnedEntities.PushBack(Entity::Invalid());
        }

        for (size_t i = 0; i < m_commands.Size(); ++i)
        {
            const detail::Command& cmd = m_commands[i];

            if (cmd.m_type == CommandType::SPAWN)
            {
                uint32_t spawnIndex = cmd.m_entity.Index();

                auto builder = world.Spawn();

                for (size_t j = i + 1; j < m_commands.Size(); ++j)
                {
                    const detail::Command& other = m_commands[j];
                    if (other.m_type == CommandType::ADD_COMPONENT && IsPendingEntity(other.m_entity) &&
                        other.m_entity.Index() == spawnIndex)
                    {
                        builder.WithRaw(other.m_meta, other.m_data);
                    }
                }

                Entity realEntity = builder.Build();
                m_spawnedEntities[spawnIndex] = realEntity;
            }
        }

        for (size_t i = 0; i < m_commands.Size(); ++i)
        {
            const detail::Command& cmd = m_commands[i];

            switch (cmd.m_type)
            {
                case CommandType::SPAWN:
                    break;

                case CommandType::DESPAWN:
                    world.Despawn(cmd.m_entity);
                    break;

                case CommandType::ADD_COMPONENT: {
                    if (IsPendingEntity(cmd.m_entity))
                    {
                        break;
                    }

                    Entity entity = cmd.m_entity;
                    if (!world.IsAlive(entity))
                    {
                        break;
                    }

                    auto* record = world.m_entityLocations.Get(entity);
                    if (record == nullptr || record->m_archetype == nullptr)
                    {
                        break;
                    }

                    auto* oldArch = record->m_archetype;

                    if (oldArch->HasComponent(cmd.m_componentType))
                    {
                        oldArch->SetComponent(record->m_row, cmd.m_componentType, cmd.m_data);
                    }
                    else
                    {
                        auto* newArch = world.m_archetypeGraph.GetOrCreateAddTarget(*oldArch, cmd.m_meta);

                        if (newArch != oldArch)
                        {
                            world.RegisterNewArchetype(newArch);
                            world.MoveEntity(entity, *record, oldArch, newArch);
                        }

                        newArch->SetComponent(record->m_row, cmd.m_componentType, cmd.m_data);
                    }
                    break;
                }

                case CommandType::REMOVE_COMPONENT: {
                    Entity entity = ResolveEntity(cmd.m_entity);
                    if (!world.IsAlive(entity))
                    {
                        break;
                    }

                    auto* record = world.m_entityLocations.Get(entity);
                    if (record == nullptr || record->m_archetype == nullptr)
                    {
                        break;
                    }

                    auto* oldArch = record->m_archetype;

                    if (!oldArch->HasComponent(cmd.m_componentType))
                    {
                        break;
                    }

                    auto* newArch = world.m_archetypeGraph.GetOrCreateRemoveTarget(*oldArch, cmd.m_componentType);

                    if (newArch != oldArch)
                    {
                        world.RegisterNewArchetype(newArch);
                        world.MoveEntity(entity, *record, oldArch, newArch);
                    }
                    break;
                }

                case CommandType::SET_COMPONENT: {
                    Entity entity = ResolveEntity(cmd.m_entity);
                    if (!world.IsAlive(entity))
                    {
                        break;
                    }

                    auto* record = world.m_entityLocations.Get(entity);
                    if (record == nullptr || record->m_archetype == nullptr)
                    {
                        break;
                    }

                    auto* oldArch = record->m_archetype;

                    if (oldArch->HasComponent(cmd.m_componentType))
                    {
                        oldArch->SetComponent(record->m_row, cmd.m_componentType, cmd.m_data);
                    }
                    else
                    {
                        auto* newArch = world.m_archetypeGraph.GetOrCreateAddTarget(*oldArch, cmd.m_meta);

                        if (newArch != oldArch)
                        {
                            world.RegisterNewArchetype(newArch);
                            world.MoveEntity(entity, *record, oldArch, newArch);
                        }

                        newArch->SetComponent(record->m_row, cmd.m_componentType, cmd.m_data);
                    }
                    break;
                }
            }
        }

        // Clear commands but preserve spawned_entities_ for GetSpawnedEntity() calls
        for (size_t i = 0; i < m_commands.Size(); ++i)
        {
            const detail::Command& cmd = m_commands[i];
            if (cmd.m_data != nullptr && cmd.m_meta.m_destruct != nullptr)
            {
                cmd.m_meta.m_destruct(cmd.m_data);
            }
        }

        m_commands.Clear();
        m_spawnCount = 0;
        ClearBlocks();
    }
} // namespace queen

// Include method implementations now that World is complete
#include <queen/observer/observer_storage_impl.h>
#include <queen/scheduler/parallel_scheduler_impl.h>
#include <queen/scheduler/scheduler_impl.h>
#include <queen/system/system_builder_impl.h>
