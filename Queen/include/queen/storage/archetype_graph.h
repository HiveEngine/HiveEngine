#pragma once

#include <queen/core/type_id.h>
#include <queen/core/component_info.h>
#include <queen/storage/archetype.h>
#include <comb/allocator_concepts.h>
#include <wax/containers/vector.h>
#include <wax/containers/hash_map.h>

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
    template<comb::Allocator Allocator>
    class ArchetypeGraph
    {
    public:
        explicit ArchetypeGraph(Allocator& allocator)
            : allocator_{&allocator}
            , archetypes_{allocator}
            , archetype_storage_{allocator}
        {
            CreateEmptyArchetype();
        }

        ~ArchetypeGraph()
        {
            for (size_t i = 0; i < archetype_storage_.Size(); ++i)
            {
                comb::Delete(*allocator_, archetype_storage_[i]);
            }
        }

        ArchetypeGraph(const ArchetypeGraph&) = delete;
        ArchetypeGraph& operator=(const ArchetypeGraph&) = delete;

        ArchetypeGraph(ArchetypeGraph&& other) noexcept
            : allocator_{other.allocator_}
            , archetypes_{static_cast<wax::HashMap<ArchetypeId, Archetype<Allocator>*, Allocator>&&>(other.archetypes_)}
            , archetype_storage_{static_cast<wax::Vector<Archetype<Allocator>*, Allocator>&&>(other.archetype_storage_)}
            , empty_archetype_{other.empty_archetype_}
        {
            other.empty_archetype_ = nullptr;
        }

        ArchetypeGraph& operator=(ArchetypeGraph&& other) noexcept
        {
            if (this != &other)
            {
                for (size_t i = 0; i < archetype_storage_.Size(); ++i)
                {
                    comb::Delete(*allocator_, archetype_storage_[i]);
                }
                allocator_ = other.allocator_;
                archetypes_ = static_cast<wax::HashMap<ArchetypeId, Archetype<Allocator>*, Allocator>&&>(other.archetypes_);
                archetype_storage_ = static_cast<wax::Vector<Archetype<Allocator>*, Allocator>&&>(other.archetype_storage_);
                empty_archetype_ = other.empty_archetype_;
                other.empty_archetype_ = nullptr;
            }
            return *this;
        }

        [[nodiscard]] Archetype<Allocator>* GetEmptyArchetype() noexcept
        {
            return empty_archetype_;
        }

        [[nodiscard]] Archetype<Allocator>* GetArchetype(ArchetypeId id) noexcept
        {
            Archetype<Allocator>** result = archetypes_.Find(id);
            return result ? *result : nullptr;
        }

        [[nodiscard]] size_t ArchetypeCount() const noexcept
        {
            return archetype_storage_.Size();
        }

        template<typename T>
        [[nodiscard]] Archetype<Allocator>* GetOrCreateAddTarget(Archetype<Allocator>& source)
        {
            return GetOrCreateAddTarget(source, ComponentMeta::Of<T>());
        }

        [[nodiscard]] Archetype<Allocator>* GetOrCreateAddTarget(
            Archetype<Allocator>& source,
            const ComponentMeta& new_component)
        {
            TypeId type_id = new_component.type_id;

            Archetype<Allocator>* cached = source.GetAddEdge(type_id);
            if (cached != nullptr)
            {
                return cached;
            }

            if (source.HasComponent(type_id))
            {
                return &source;
            }

            wax::Vector<ComponentMeta, Allocator> new_metas{*allocator_};
            const auto& source_metas = source.GetComponentMetas();
            new_metas.Reserve(source_metas.Size() + 1);

            for (size_t i = 0; i < source_metas.Size(); ++i)
            {
                new_metas.PushBack(source_metas[i]);
            }
            new_metas.PushBack(new_component);

            Archetype<Allocator>* target = GetOrCreateArchetype(std::move(new_metas));

            source.SetAddEdge(type_id, target);
            target->SetRemoveEdge(type_id, &source);

            return target;
        }

        template<typename T>
        [[nodiscard]] Archetype<Allocator>* GetOrCreateRemoveTarget(Archetype<Allocator>& source)
        {
            return GetOrCreateRemoveTarget(source, TypeIdOf<T>());
        }

        [[nodiscard]] Archetype<Allocator>* GetOrCreateRemoveTarget(
            Archetype<Allocator>& source,
            TypeId type_id)
        {
            Archetype<Allocator>* cached = source.GetRemoveEdge(type_id);
            if (cached != nullptr)
            {
                return cached;
            }

            if (!source.HasComponent(type_id))
            {
                return &source;
            }

            wax::Vector<ComponentMeta, Allocator> new_metas{*allocator_};
            const auto& source_metas = source.GetComponentMetas();
            new_metas.Reserve(source_metas.Size() - 1);

            for (size_t i = 0; i < source_metas.Size(); ++i)
            {
                if (source_metas[i].type_id != type_id)
                {
                    new_metas.PushBack(source_metas[i]);
                }
            }

            Archetype<Allocator>* target = GetOrCreateArchetype(std::move(new_metas));

            source.SetRemoveEdge(type_id, target);
            target->SetAddEdge(type_id, &source);

            return target;
        }

    private:
        void CreateEmptyArchetype()
        {
            wax::Vector<ComponentMeta, Allocator> empty_metas{*allocator_};
            empty_archetype_ = CreateArchetype(std::move(empty_metas));
        }

        Archetype<Allocator>* GetOrCreateArchetype(wax::Vector<ComponentMeta, Allocator> metas)
        {
            wax::Vector<TypeId, Allocator> type_ids{*allocator_};
            type_ids.Reserve(metas.Size());
            for (size_t i = 0; i < metas.Size(); ++i)
            {
                type_ids.PushBack(metas[i].type_id);
            }

            SortTypeIds(type_ids);
            ArchetypeId id = detail::ComputeArchetypeId<Allocator>(type_ids);

            Archetype<Allocator>* existing = GetArchetype(id);
            if (existing != nullptr)
            {
                return existing;
            }

            return CreateArchetype(std::move(metas));
        }

        Archetype<Allocator>* CreateArchetype(wax::Vector<ComponentMeta, Allocator> metas)
        {
            auto* archetype = comb::New<Archetype<Allocator>>(*allocator_, *allocator_, std::move(metas));
            archetype_storage_.PushBack(archetype);
            archetypes_.Insert(archetype->GetId(), archetype);
            return archetype;
        }

        void SortTypeIds(wax::Vector<TypeId, Allocator>& type_ids)
        {
            for (size_t i = 1; i < type_ids.Size(); ++i)
            {
                TypeId key = type_ids[i];
                size_t j = i;
                while (j > 0 && type_ids[j - 1] > key)
                {
                    type_ids[j] = type_ids[j - 1];
                    --j;
                }
                type_ids[j] = key;
            }
        }

        Allocator* allocator_;
        wax::HashMap<ArchetypeId, Archetype<Allocator>*, Allocator> archetypes_;
        wax::Vector<Archetype<Allocator>*, Allocator> archetype_storage_;
        Archetype<Allocator>* empty_archetype_{nullptr};
    };
}
