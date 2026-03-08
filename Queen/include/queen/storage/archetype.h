#pragma once

#include <hive/core/assert.h>

#include <comb/allocator_concepts.h>

#include <wax/containers/hash_map.h>
#include <wax/containers/vector.h>

#include <queen/core/component_info.h>
#include <queen/core/entity.h>
#include <queen/core/type_id.h>
#include <queen/storage/table.h>

#include <type_traits>

namespace queen
{
    using ArchetypeId = TypeId;

    namespace detail
    {
        template <comb::Allocator Allocator> ArchetypeId ComputeArchetypeId(const wax::Vector<TypeId>& sortedTypes)
        {
            ArchetypeId hash = kFnv1aOffset;
            for (size_t i = 0; i < sortedTypes.Size(); ++i)
            {
                hash ^= sortedTypes[i];
                hash *= kFnv1aPrime;
            }
            return hash;
        }
    } // namespace detail

    /**
     * Archetype definition and storage
     *
     * An archetype represents a unique combination of component types.
     * All entities with the exact same set of components share the same archetype.
     * The archetype owns its Table which stores the actual component data.
     *
     * Memory layout:
     * ┌────────────────────────────────────────────────────────────┐
     * │ id_: ArchetypeId (hash of sorted TypeIds)                  │
     * │ component_types_: sorted [TypeId_A, TypeId_B, ...]         │
     * │ component_metas_: [Meta_A, Meta_B, ...]                    │
     * │ table_: Table<Allocator> (owns component storage)          │
     * │ add_edges_: HashMap<TypeId, Archetype*> (transitions)      │
     * │ remove_edges_: HashMap<TypeId, Archetype*> (transitions)   │
     * └────────────────────────────────────────────────────────────┘
     *
     * Performance characteristics:
     * - HasComponent: O(log N) binary search on sorted types
     * - GetColumnIndex: O(log N) binary search
     * - Edge lookup: O(1) hash map
     * - Entity count: O(1)
     *
     * Limitations:
     * - Component set is fixed after construction
     * - Not thread-safe
     *
     * Example:
     * @code
     *   wax::Vector<ComponentMeta, ...> metas{alloc};
     *   metas.PushBack(ComponentMeta::Of<Position>());
     *   metas.PushBack(ComponentMeta::Of<Velocity>());
     *
     *   Archetype arch{alloc, metas};
     *
     *   uint32_t row = arch.AllocateRow(entity);
     *   arch.SetComponent<Position>(row, Position{1.0f, 2.0f, 3.0f});
     * @endcode
     */
    template <comb::Allocator Allocator> class Archetype
    {
    public:
        Archetype(Allocator& allocator, wax::Vector<ComponentMeta> componentMetas, size_t initialCapacity = 64)
            : m_allocator{&allocator}
            , m_componentTypes{allocator}
            , m_componentMetas{std::move(componentMetas)}
            , m_table{allocator, m_componentMetas, initialCapacity}
            , m_addEdges{allocator}
            , m_removeEdges{allocator}
        {
            m_componentTypes.Reserve(m_componentMetas.Size());
            for (size_t i = 0; i < m_componentMetas.Size(); ++i)
            {
                m_componentTypes.PushBack(m_componentMetas[i].m_typeId);
            }

            SortComponentTypes();
            m_id = detail::ComputeArchetypeId<Allocator>(m_componentTypes);
        }

        ~Archetype() = default;

        Archetype(const Archetype&) = delete;
        Archetype& operator=(const Archetype&) = delete;
        Archetype(Archetype&&) = default;
        Archetype& operator=(Archetype&&) = default;

        [[nodiscard]] ArchetypeId GetId() const noexcept
        {
            return m_id;
        }

        [[nodiscard]] bool HasComponent(TypeId typeId) const noexcept
        {
            return BinarySearch(typeId) != SIZE_MAX;
        }

        template <typename T> [[nodiscard]] bool HasComponent() const noexcept
        {
            return HasComponent(TypeIdOf<T>());
        }

        [[nodiscard]] size_t GetColumnIndex(TypeId typeId) const noexcept
        {
            return BinarySearch(typeId);
        }

        template <typename T> [[nodiscard]] size_t GetColumnIndex() const noexcept
        {
            return GetColumnIndex(TypeIdOf<T>());
        }

        uint32_t AllocateRow(Entity entity, Tick currentTick = Tick{0})
        {
            return m_table.AllocateRow(entity, currentTick);
        }

        Entity FreeRow(uint32_t row)
        {
            return m_table.FreeRow(row);
        }

        template <typename T> void SetComponent(uint32_t row, const T& value)
        {
            m_table.template SetComponent<T>(row, value);
        }

        template <typename T>
        void SetComponent(uint32_t row, T&& value)
            requires(!std::is_lvalue_reference_v<T &&>)
        {
            m_table.template SetComponent<T>(row, std::forward<T>(value));
        }

        void SetComponent(uint32_t row, TypeId typeId, const void* data)
        {
            m_table.SetComponent(row, typeId, data);
        }

        template <typename T> [[nodiscard]] T* GetComponent(uint32_t row) noexcept
        {
            auto* column = m_table.template GetColumn<T>();
            if (column == nullptr)
            {
                return nullptr;
            }
            return column->template Get<T>(row);
        }

        template <typename T> [[nodiscard]] const T* GetComponent(uint32_t row) const noexcept
        {
            const auto* column = m_table.template GetColumn<T>();
            if (column == nullptr)
            {
                return nullptr;
            }
            return column->template Get<T>(row);
        }

        [[nodiscard]] void* GetComponentRaw(uint32_t row, TypeId typeId) noexcept
        {
            auto* column = m_table.GetColumnByTypeId(typeId);
            if (column == nullptr)
            {
                return nullptr;
            }
            return column->GetRaw(row);
        }

        [[nodiscard]] const void* GetComponentRaw(uint32_t row, TypeId typeId) const noexcept
        {
            const auto* column = m_table.GetColumnByTypeId(typeId);
            if (column == nullptr)
            {
                return nullptr;
            }
            return column->GetRaw(row);
        }

        [[nodiscard]] Entity GetEntity(uint32_t row) const noexcept
        {
            return m_table.GetEntity(row);
        }

        [[nodiscard]] const Entity* GetEntities() const noexcept
        {
            return m_table.GetEntities();
        }

        [[nodiscard]] Column<Allocator>* GetColumn(TypeId typeId) noexcept
        {
            return m_table.GetColumnByTypeId(typeId);
        }

        template <typename T> [[nodiscard]] Column<Allocator>* GetColumn() noexcept
        {
            return m_table.template GetColumn<T>();
        }

        [[nodiscard]] size_t EntityCount() const noexcept
        {
            return m_table.RowCount();
        }
        [[nodiscard]] size_t ComponentCount() const noexcept
        {
            return m_componentTypes.Size();
        }
        [[nodiscard]] bool IsEmpty() const noexcept
        {
            return m_table.IsEmpty();
        }

        [[nodiscard]] const wax::Vector<TypeId>& GetComponentTypes() const noexcept
        {
            return m_componentTypes;
        }

        [[nodiscard]] const wax::Vector<ComponentMeta>& GetComponentMetas() const noexcept
        {
            return m_componentMetas;
        }

        void SetAddEdge(TypeId typeId, Archetype* target)
        {
            m_addEdges.Insert(typeId, target);
        }

        void SetRemoveEdge(TypeId typeId, Archetype* target)
        {
            m_removeEdges.Insert(typeId, target);
        }

        [[nodiscard]] Archetype* GetAddEdge(TypeId typeId) noexcept
        {
            Archetype** result = m_addEdges.Find(typeId);
            return result ? *result : nullptr;
        }

        [[nodiscard]] Archetype* GetRemoveEdge(TypeId typeId) noexcept
        {
            Archetype** result = m_removeEdges.Find(typeId);
            return result ? *result : nullptr;
        }

        [[nodiscard]] Table<Allocator>& GetTable() noexcept
        {
            return m_table;
        }
        [[nodiscard]] const Table<Allocator>& GetTable() const noexcept
        {
            return m_table;
        }

    private:
        void SortComponentTypes()
        {
            for (size_t i = 1; i < m_componentTypes.Size(); ++i)
            {
                TypeId key = m_componentTypes[i];
                size_t j = i;
                while (j > 0 && m_componentTypes[j - 1] > key)
                {
                    m_componentTypes[j] = m_componentTypes[j - 1];
                    --j;
                }
                m_componentTypes[j] = key;
            }
        }

        [[nodiscard]] size_t BinarySearch(TypeId typeId) const noexcept
        {
            if (m_componentTypes.IsEmpty())
            {
                return SIZE_MAX;
            }

            size_t left = 0;
            size_t right = m_componentTypes.Size();

            while (left < right)
            {
                size_t mid = left + (right - left) / 2;
                if (m_componentTypes[mid] == typeId)
                {
                    return mid;
                }
                if (m_componentTypes[mid] < typeId)
                {
                    left = mid + 1;
                }
                else
                {
                    right = mid;
                }
            }

            return SIZE_MAX;
        }

        Allocator* m_allocator;
        ArchetypeId m_id;
        wax::Vector<TypeId> m_componentTypes;
        wax::Vector<ComponentMeta> m_componentMetas;
        Table<Allocator> m_table;
        wax::HashMap<TypeId, Archetype*> m_addEdges;
        wax::HashMap<TypeId, Archetype*> m_removeEdges;
    };
} // namespace queen
