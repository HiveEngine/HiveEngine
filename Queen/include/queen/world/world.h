#pragma once

#include <queen/core/type_id.h>
#include <queen/core/entity.h>
#include <queen/core/entity_allocator.h>
#include <queen/core/entity_location.h>
#include <queen/core/component_info.h>
#include <queen/storage/archetype.h>
#include <queen/storage/archetype_graph.h>
#include <queen/storage/component_index.h>
#include <queen/query/query.h>
#include <comb/allocator_concepts.h>
#include <wax/containers/vector.h>
#include <wax/containers/hash_map.h>
#include <hive/core/assert.h>

namespace queen
{
    template<comb::Allocator Allocator>
    class EntityBuilder;

    /**
     * Central ECS world containing all entities, components, and resources
     *
     * The World is the main entry point for the ECS. It manages entity lifecycle,
     * component storage, resources (global singletons), and provides access to
     * queries. All memory is allocated through the provided allocator.
     *
     * Memory layout:
     * ┌────────────────────────────────────────────────────────────┐
     * │ entity_allocator_: Entity ID allocation and recycling      │
     * │ entity_locations_: Entity -> (Archetype, Row) mapping      │
     * │ archetype_graph_: All archetypes with transitions          │
     * │ component_index_: TypeId -> Archetypes reverse lookup      │
     * │ resources_: TypeId -> void* global singleton storage       │
     * │ resource_metas_: Lifecycle info for resource cleanup       │
     * └────────────────────────────────────────────────────────────┘
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
     * - Not thread-safe
     * - No deferred operations (immediate mutation)
     *
     * Example:
     * @code
     *   comb::LinearAllocator alloc{10_MB};
     *   queen::World world{alloc};
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
     * @endcode
     */
    template<comb::Allocator Allocator>
    class World
    {
    public:
        using EntityRecord = EntityRecordT<Archetype<Allocator>>;

        explicit World(Allocator& allocator)
            : allocator_{&allocator}
            , entity_allocator_{allocator}
            , entity_locations_{allocator}
            , archetype_graph_{allocator}
            , component_index_{allocator}
            , resources_{allocator}
            , resource_metas_{allocator}
        {
            component_index_.RegisterArchetype(archetype_graph_.GetEmptyArchetype());
        }

        ~World()
        {
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

                allocator_->Deallocate(data);
            }
        }

        World(const World&) = delete;
        World& operator=(const World&) = delete;
        World(World&&) = default;
        World& operator=(World&&) = default;

        [[nodiscard]] EntityBuilder<Allocator> Spawn();

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

            Archetype<Allocator>* archetype = record->archetype;
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

            Archetype<Allocator>* old_arch = record->archetype;

            if (old_arch->template HasComponent<T>())
            {
                old_arch->template SetComponent<T>(record->row, std::forward<T>(component));
                return;
            }

            Archetype<Allocator>* new_arch = archetype_graph_.template GetOrCreateAddTarget<T>(*old_arch);

            if (new_arch != old_arch)
            {
                RegisterNewArchetype(new_arch);
            }

            MoveEntity(entity, *record, old_arch, new_arch);

            new_arch->template SetComponent<T>(record->row, std::forward<T>(component));
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

            Archetype<Allocator>* old_arch = record->archetype;

            if (!old_arch->template HasComponent<T>())
            {
                return;
            }

            Archetype<Allocator>* new_arch = archetype_graph_.template GetOrCreateRemoveTarget<T>(*old_arch);

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

            void* data = allocator_->Allocate(sizeof(DecayedT), alignof(DecayedT));
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

            allocator_->Deallocate(data);
            resources_.Remove(type_id);
        }

        [[nodiscard]] size_t ResourceCount() const noexcept
        {
            return resources_.Count();
        }

        [[nodiscard]] Allocator& GetAllocator() noexcept
        {
            return *allocator_;
        }

        [[nodiscard]] ArchetypeGraph<Allocator>& GetArchetypeGraph() noexcept
        {
            return archetype_graph_;
        }

        [[nodiscard]] ComponentIndex<Allocator>& GetComponentIndex() noexcept
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
        [[nodiscard]] queen::Query<Allocator, Terms...> Query()
        {
            return queen::Query<Allocator, Terms...>{*allocator_, component_index_};
        }

    private:
        friend class EntityBuilder<Allocator>;

        Entity AllocateEntity()
        {
            return entity_allocator_.Allocate();
        }

        void PlaceEntity(Entity entity, Archetype<Allocator>* archetype)
        {
            uint32_t row = archetype->AllocateRow(entity);
            entity_locations_.Set(entity, EntityRecord{archetype, row});
        }

        void RegisterNewArchetype(Archetype<Allocator>* archetype)
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
                       Archetype<Allocator>* old_arch, Archetype<Allocator>* new_arch)
        {
            uint32_t old_row = record.row;

            uint32_t new_row = new_arch->AllocateRow(entity);

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

        Allocator* allocator_;
        EntityAllocator<Allocator> entity_allocator_;
        EntityLocationMap<Allocator, Archetype<Allocator>> entity_locations_;
        ArchetypeGraph<Allocator> archetype_graph_;
        ComponentIndex<Allocator> component_index_;

        wax::HashMap<TypeId, void*, Allocator> resources_;
        wax::Vector<ComponentMeta, Allocator> resource_metas_;
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
    template<comb::Allocator Allocator>
    class EntityBuilder
    {
    public:
        EntityBuilder(World<Allocator>& world)
            : world_{&world}
            , pending_metas_{world.GetAllocator()}
            , pending_data_{world.GetAllocator()}
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

            void* data = world_->allocator_->Allocate(sizeof(T), alignof(T));
            new (data) T{std::forward<T>(component)};
            pending_data_.PushBack(data);

            return *this;
        }

        Entity Build()
        {
            Entity entity = world_->AllocateEntity();

            Archetype<Allocator>* archetype = world_->archetype_graph_.GetEmptyArchetype();

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
        World<Allocator>* world_;
        wax::Vector<ComponentMeta, Allocator> pending_metas_;
        wax::Vector<void*, Allocator> pending_data_;
    };

    template<comb::Allocator Allocator>
    EntityBuilder<Allocator> World<Allocator>::Spawn()
    {
        return EntityBuilder<Allocator>{*this};
    }

    template<comb::Allocator Allocator>
    template<typename... Components>
    Entity World<Allocator>::Spawn(Components&&... components)
    {
        EntityBuilder<Allocator> builder{*this};
        (builder.With(std::forward<Components>(components)), ...);
        return builder.Build();
    }
}
