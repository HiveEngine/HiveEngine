#pragma once

#include <queen/core/type_id.h>
#include <queen/storage/archetype.h>
#include <comb/allocator_concepts.h>
#include <wax/containers/vector.h>
#include <wax/containers/hash_map.h>

namespace queen
{
    /**
     * Inverted index for fast archetype lookup by component
     *
     * Maps component TypeIds to lists of archetypes containing that component.
     * Used to efficiently resolve queries by finding archetypes that match
     * the query's component requirements.
     *
     * Memory layout:
     * ┌──────────────────────────────────────────────────────────────┐
     * │ index_: HashMap<TypeId, Vector<Archetype*>>                  │
     * │   TypeId_Position → [Archetype_1, Archetype_3, Archetype_7]  │
     * │   TypeId_Velocity → [Archetype_1, Archetype_5]               │
     * │   TypeId_Health   → [Archetype_3, Archetype_5, Archetype_7]  │
     * └──────────────────────────────────────────────────────────────┘
     *
     * Query resolution example:
     *   Query<Position, Velocity>:
     *   1. Get archetypes with Position: {1, 3, 7}
     *   2. Get archetypes with Velocity: {1, 5}
     *   3. Intersection: {1}
     *
     * Performance characteristics:
     * - RegisterArchetype: O(n) where n = component count
     * - GetArchetypesWith: O(1) hash lookup
     * - GetArchetypesWithAll: O(k*m) where k = types, m = avg archetypes
     *
     * Limitations:
     * - Not thread-safe
     * - Archetypes cannot be unregistered
     *
     * Example:
     * @code
     *   ComponentIndex<...> index{alloc};
     *   index.RegisterArchetype(archetype);
     *
     *   auto* archetypes = index.GetArchetypesWith<Position>();
     *   for (auto* arch : archetypes) {
     *       // iterate entities...
     *   }
     * @endcode
     */
    template<comb::Allocator Allocator>
    class ComponentIndex
    {
    public:
        using ArchetypeList = wax::Vector<Archetype<Allocator>*, Allocator>;

        explicit ComponentIndex(Allocator& allocator)
            : allocator_{&allocator}
            , index_{allocator}
        {
        }

        ~ComponentIndex() = default;

        ComponentIndex(const ComponentIndex&) = delete;
        ComponentIndex& operator=(const ComponentIndex&) = delete;
        ComponentIndex(ComponentIndex&&) = default;
        ComponentIndex& operator=(ComponentIndex&&) = default;

        void RegisterArchetype(Archetype<Allocator>* archetype)
        {
            const auto& types = archetype->GetComponentTypes();
            for (size_t i = 0; i < types.Size(); ++i)
            {
                TypeId type_id = types[i];
                ArchetypeList* list = index_.Find(type_id);
                if (list == nullptr)
                {
                    index_.Insert(type_id, ArchetypeList{*allocator_});
                    list = index_.Find(type_id);
                }
                list->PushBack(archetype);
            }
        }

        template<typename T>
        [[nodiscard]] const ArchetypeList* GetArchetypesWith() const noexcept
        {
            return GetArchetypesWith(TypeIdOf<T>());
        }

        [[nodiscard]] const ArchetypeList* GetArchetypesWith(TypeId type_id) const noexcept
        {
            return index_.Find(type_id);
        }

        template<typename... Types>
        [[nodiscard]] ArchetypeList GetArchetypesWithAll() const
        {
            ArchetypeList result{*allocator_};

            if constexpr (sizeof...(Types) == 0)
            {
                return result;
            }
            else
            {
                TypeId type_ids[] = {TypeIdOf<Types>()...};
                return GetArchetypesWithAll(type_ids, sizeof...(Types));
            }
        }

        [[nodiscard]] ArchetypeList GetArchetypesWithAll(const TypeId* type_ids, size_t count) const
        {
            ArchetypeList result{*allocator_};

            if (count == 0)
            {
                return result;
            }

            size_t smallest_idx = 0;
            size_t smallest_size = SIZE_MAX;
            for (size_t i = 0; i < count; ++i)
            {
                const ArchetypeList* list = GetArchetypesWith(type_ids[i]);
                if (list == nullptr)
                {
                    return result;
                }
                if (list->Size() < smallest_size)
                {
                    smallest_size = list->Size();
                    smallest_idx = i;
                }
            }

            const ArchetypeList* smallest = GetArchetypesWith(type_ids[smallest_idx]);
            for (size_t i = 0; i < smallest->Size(); ++i)
            {
                Archetype<Allocator>* arch = (*smallest)[i];
                bool has_all = true;

                for (size_t j = 0; j < count && has_all; ++j)
                {
                    if (!arch->HasComponent(type_ids[j]))
                    {
                        has_all = false;
                    }
                }

                if (has_all)
                {
                    result.PushBack(arch);
                }
            }

            return result;
        }

        [[nodiscard]] size_t ComponentTypeCount() const noexcept
        {
            return index_.Count();
        }

    private:
        Allocator* allocator_;
        wax::HashMap<TypeId, ArchetypeList, Allocator> index_;
    };
}
