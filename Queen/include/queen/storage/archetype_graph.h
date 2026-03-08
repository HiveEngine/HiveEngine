#pragma once

#include <comb/allocator_concepts.h>
#include <comb/new.h>

#include <wax/containers/hash_map.h>
#include <wax/containers/vector.h>

#include <queen/core/component_info.h>
#include <queen/core/type_id.h>
#include <queen/storage/archetype.h>

namespace queen
{
    /**
     * Graph of all archetypes with cached transitions
     *
     * Manages the complete set of archetypes in the ECS world. Provides O(1)
     * lookup by ArchetypeId and lazy creation of new archetypes when
     * components are added or removed from entities.
     *
     * Memory layout:
     * ┌──────────────────────────────────────────────────────────────┐
     * │ archetypes_: HashMap<ArchetypeId, Archetype*>                │
     * │ archetype_storage_: Vector of owned Archetype pointers       │
     * │ empty_archetype_: Archetype with no components               │
     * └──────────────────────────────────────────────────────────────┘
     *
     * Transition graph (edges cached in Archetype):
     * ┌──────────────┐    add<Velocity>    ┌──────────────────────┐
     * │ [Position]   │ ─────────────────→  │ [Position, Velocity] │
     * └──────────────┘                     └──────────────────────┘
     *        ↑                                       │
     *        │              remove<Velocity>         │
     *        └───────────────────────────────────────┘
     *
     * Performance characteristics:
     * - GetArchetype: O(1) hash lookup
     * - GetOrCreateAddTarget: O(1) cache hit, O(n) cache miss (n = components)
     * - GetOrCreateRemoveTarget: O(1) cache hit, O(n) cache miss
     *
     * Limitations:
     * - Not thread-safe
     * - Archetypes are never removed once created
     *
     * Example:
     * @code
     *   ArchetypeGraph<...> graph{alloc};
     *
     *   auto* empty = graph.GetEmptyArchetype();
     *   auto* with_pos = graph.GetOrCreateAddTarget<Position>(*empty);
     *   auto* with_pos_vel = graph.GetOrCreateAddTarget<Velocity>(*with_pos);
     *   auto* back_to_pos = graph.GetOrCreateRemoveTarget<Velocity>(*with_pos_vel);
     *   // back_to_pos == with_pos
     * @endcode
     */
    template <comb::Allocator Allocator> class ArchetypeGraph
    {
    public:
        explicit ArchetypeGraph(Allocator& allocator)
            : m_allocator{&allocator}
            , m_archetypes{allocator}
            , m_archetypeStorage{allocator} {
            CreateEmptyArchetype();
        }

        ~ArchetypeGraph() {
            for (size_t i = 0; i < m_archetypeStorage.Size(); ++i)
            {
                comb::Delete(*m_allocator, m_archetypeStorage[i]);
            }
        }

        ArchetypeGraph(const ArchetypeGraph&) = delete;
        ArchetypeGraph& operator=(const ArchetypeGraph&) = delete;

        ArchetypeGraph(ArchetypeGraph&& other) noexcept
            : m_allocator{other.m_allocator}
            , m_archetypes{static_cast<wax::HashMap<ArchetypeId, Archetype<Allocator>*>&&>(other.m_archetypes)}
            , m_archetypeStorage{static_cast<wax::Vector<Archetype<Allocator>*>&&>(other.m_archetypeStorage)}
            , m_emptyArchetype{other.m_emptyArchetype} {
            other.m_emptyArchetype = nullptr;
        }

        ArchetypeGraph& operator=(ArchetypeGraph&& other) noexcept {
            if (this != &other)
            {
                for (size_t i = 0; i < m_archetypeStorage.Size(); ++i)
                {
                    comb::Delete(*m_allocator, m_archetypeStorage[i]);
                }
                m_allocator = other.m_allocator;
                m_archetypes = static_cast<wax::HashMap<ArchetypeId, Archetype<Allocator>*>&&>(other.m_archetypes);
                m_archetypeStorage = static_cast<wax::Vector<Archetype<Allocator>*>&&>(other.m_archetypeStorage);
                m_emptyArchetype = other.m_emptyArchetype;
                other.m_emptyArchetype = nullptr;
            }
            return *this;
        }

        [[nodiscard]] Archetype<Allocator>* GetEmptyArchetype() noexcept { return m_emptyArchetype; }

        [[nodiscard]] Archetype<Allocator>* GetArchetype(ArchetypeId id) noexcept {
            Archetype<Allocator>** result = m_archetypes.Find(id);
            return result ? *result : nullptr;
        }

        [[nodiscard]] size_t ArchetypeCount() const noexcept { return m_archetypeStorage.Size(); }

        [[nodiscard]] const wax::Vector<Archetype<Allocator>*>& GetArchetypes() const noexcept {
            return m_archetypeStorage;
        }

        template <typename T> [[nodiscard]] Archetype<Allocator>* GetOrCreateAddTarget(Archetype<Allocator>& source) {
            return GetOrCreateAddTarget(source, ComponentMeta::Of<T>());
        }

        [[nodiscard]] Archetype<Allocator>* GetOrCreateAddTarget(Archetype<Allocator>& source,
                                                                 const ComponentMeta& newComponent) {
            TypeId typeId = newComponent.m_typeId;

            Archetype<Allocator>* cached = source.GetAddEdge(typeId);
            if (cached != nullptr)
            {
                return cached;
            }

            if (source.HasComponent(typeId))
            {
                return &source;
            }

            wax::Vector<ComponentMeta> newMetas{*m_allocator};
            const auto& sourceMetas = source.GetComponentMetas();
            newMetas.Reserve(sourceMetas.Size() + 1);

            for (size_t i = 0; i < sourceMetas.Size(); ++i)
            {
                newMetas.PushBack(sourceMetas[i]);
            }
            newMetas.PushBack(newComponent);

            Archetype<Allocator>* target = GetOrCreateArchetype(std::move(newMetas));

            source.SetAddEdge(typeId, target);
            target->SetRemoveEdge(typeId, &source);

            return target;
        }

        template <typename T>
        [[nodiscard]] Archetype<Allocator>* GetOrCreateRemoveTarget(Archetype<Allocator>& source) {
            return GetOrCreateRemoveTarget(source, TypeIdOf<T>());
        }

        [[nodiscard]] Archetype<Allocator>* GetOrCreateRemoveTarget(Archetype<Allocator>& source, TypeId typeId) {
            Archetype<Allocator>* cached = source.GetRemoveEdge(typeId);
            if (cached != nullptr)
            {
                return cached;
            }

            if (!source.HasComponent(typeId))
            {
                return &source;
            }

            wax::Vector<ComponentMeta> newMetas{*m_allocator};
            const auto& sourceMetas = source.GetComponentMetas();
            newMetas.Reserve(sourceMetas.Size() - 1);

            for (size_t i = 0; i < sourceMetas.Size(); ++i)
            {
                if (sourceMetas[i].m_typeId != typeId)
                {
                    newMetas.PushBack(sourceMetas[i]);
                }
            }

            Archetype<Allocator>* target = GetOrCreateArchetype(std::move(newMetas));

            source.SetRemoveEdge(typeId, target);
            target->SetAddEdge(typeId, &source);

            return target;
        }

    private:
        void CreateEmptyArchetype() {
            wax::Vector<ComponentMeta> emptyMetas{*m_allocator};
            m_emptyArchetype = CreateArchetype(std::move(emptyMetas));
        }

        Archetype<Allocator>* GetOrCreateArchetype(wax::Vector<ComponentMeta> metas) {
            wax::Vector<TypeId> typeIds{*m_allocator};
            typeIds.Reserve(metas.Size());
            for (size_t i = 0; i < metas.Size(); ++i)
            {
                typeIds.PushBack(metas[i].m_typeId);
            }

            SortTypeIds(typeIds);
            ArchetypeId id = detail::ComputeArchetypeId<Allocator>(typeIds);

            Archetype<Allocator>* existing = GetArchetype(id);
            if (existing != nullptr)
            {
                return existing;
            }

            return CreateArchetype(std::move(metas));
        }

        Archetype<Allocator>* CreateArchetype(wax::Vector<ComponentMeta> metas) {
            auto* archetype = comb::New<Archetype<Allocator>>(*m_allocator, *m_allocator, std::move(metas));
            m_archetypeStorage.PushBack(archetype);
            m_archetypes.Insert(archetype->GetId(), archetype);
            return archetype;
        }

        void SortTypeIds(wax::Vector<TypeId>& typeIds) {
            for (size_t i = 1; i < typeIds.Size(); ++i)
            {
                TypeId key = typeIds[i];
                size_t j = i;
                while (j > 0 && typeIds[j - 1] > key)
                {
                    typeIds[j] = typeIds[j - 1];
                    --j;
                }
                typeIds[j] = key;
            }
        }

        Allocator* m_allocator;
        wax::HashMap<ArchetypeId, Archetype<Allocator>*> m_archetypes;
        wax::Vector<Archetype<Allocator>*> m_archetypeStorage;
        Archetype<Allocator>* m_emptyArchetype{nullptr};
    };
} // namespace queen
