#pragma once

#include <comb/allocator_concepts.h>

#include <wax/containers/hash_map.h>
#include <wax/containers/vector.h>

#include <queen/core/type_id.h>
#include <queen/storage/archetype.h>

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
    template <comb::Allocator Allocator> class ComponentIndex
    {
    public:
        using ArchetypeList = wax::Vector<Archetype<Allocator>*>;

        explicit ComponentIndex(Allocator& allocator)
            : m_allocator{&allocator}
            , m_index{allocator}
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
                TypeId typeId = types[i];
                ArchetypeList* list = m_index.Find(typeId);
                if (list == nullptr)
                {
                    m_index.Insert(typeId, ArchetypeList{*m_allocator});
                    list = m_index.Find(typeId);
                }
                list->PushBack(archetype);
            }
        }

        template <typename T> [[nodiscard]] const ArchetypeList* GetArchetypesWith() const noexcept
        {
            return GetArchetypesWith(TypeIdOf<T>());
        }

        [[nodiscard]] const ArchetypeList* GetArchetypesWith(TypeId typeId) const noexcept
        {
            return m_index.Find(typeId);
        }

        template <typename... Types> [[nodiscard]] ArchetypeList GetArchetypesWithAll() const
        {
            ArchetypeList result{*m_allocator};

            if constexpr (sizeof...(Types) == 0)
            {
                return result;
            }
            else
            {
                TypeId typeIds[] = {TypeIdOf<Types>()...};
                return GetArchetypesWithAll(typeIds, sizeof...(Types));
            }
        }

        [[nodiscard]] ArchetypeList GetArchetypesWithAll(const TypeId* typeIds, size_t count) const
        {
            ArchetypeList result{*m_allocator};

            if (count == 0)
            {
                return result;
            }

            size_t smallestIdx = 0;
            size_t smallestSize = SIZE_MAX;
            for (size_t i = 0; i < count; ++i)
            {
                const ArchetypeList* list = GetArchetypesWith(typeIds[i]);
                if (list == nullptr)
                {
                    return result;
                }
                if (list->Size() < smallestSize)
                {
                    smallestSize = list->Size();
                    smallestIdx = i;
                }
            }

            const ArchetypeList* smallest = GetArchetypesWith(typeIds[smallestIdx]);
            for (size_t i = 0; i < smallest->Size(); ++i)
            {
                Archetype<Allocator>* arch = (*smallest)[i];
                bool hasAll = true;

                for (size_t j = 0; j < count && hasAll; ++j)
                {
                    if (!arch->HasComponent(typeIds[j]))
                    {
                        hasAll = false;
                    }
                }

                if (hasAll)
                {
                    result.PushBack(arch);
                }
            }

            return result;
        }

        [[nodiscard]] size_t ComponentTypeCount() const noexcept
        {
            return m_index.Count();
        }

    private:
        Allocator* m_allocator;
        wax::HashMap<TypeId, ArchetypeList> m_index;
    };
} // namespace queen
