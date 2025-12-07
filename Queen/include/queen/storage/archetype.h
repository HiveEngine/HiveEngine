#pragma once

#include <queen/core/type_id.h>
#include <queen/core/entity.h>
#include <queen/core/component_info.h>
#include <queen/storage/table.h>
#include <comb/allocator_concepts.h>
#include <wax/containers/vector.h>
#include <wax/containers/hash_map.h>
#include <hive/core/assert.h>
#include <algorithm>

namespace queen
{
    using ArchetypeId = TypeId;

    namespace detail
    {
        template<comb::Allocator Allocator>
        ArchetypeId ComputeArchetypeId(const wax::Vector<TypeId, Allocator>& sorted_types)
        {
            ArchetypeId hash = 14695981039346656037ULL;
            for (size_t i = 0; i < sorted_types.Size(); ++i)
            {
                hash ^= sorted_types[i];
                hash *= 1099511628211ULL;
            }
            return hash;
        }
    }

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
    template<comb::Allocator Allocator>
    class Archetype
    {
    public:
        Archetype(Allocator& allocator, wax::Vector<ComponentMeta, Allocator> component_metas, size_t initial_capacity = 64)
            : allocator_{&allocator}
            , component_types_{allocator}
            , component_metas_{std::move(component_metas)}
            , table_{allocator, component_metas_, initial_capacity}
            , add_edges_{allocator}
            , remove_edges_{allocator}
        {
            component_types_.Reserve(component_metas_.Size());
            for (size_t i = 0; i < component_metas_.Size(); ++i)
            {
                component_types_.PushBack(component_metas_[i].type_id);
            }

            SortComponentTypes();
            id_ = detail::ComputeArchetypeId<Allocator>(component_types_);
        }

        ~Archetype() = default;

        Archetype(const Archetype&) = delete;
        Archetype& operator=(const Archetype&) = delete;
        Archetype(Archetype&&) = default;
        Archetype& operator=(Archetype&&) = default;

        [[nodiscard]] ArchetypeId GetId() const noexcept { return id_; }

        [[nodiscard]] bool HasComponent(TypeId type_id) const noexcept
        {
            return BinarySearch(type_id) != SIZE_MAX;
        }

        template<typename T>
        [[nodiscard]] bool HasComponent() const noexcept
        {
            return HasComponent(TypeIdOf<T>());
        }

        [[nodiscard]] size_t GetColumnIndex(TypeId type_id) const noexcept
        {
            return BinarySearch(type_id);
        }

        template<typename T>
        [[nodiscard]] size_t GetColumnIndex() const noexcept
        {
            return GetColumnIndex(TypeIdOf<T>());
        }

        uint32_t AllocateRow(Entity entity, Tick current_tick = Tick{0})
        {
            return table_.AllocateRow(entity, current_tick);
        }

        Entity FreeRow(uint32_t row)
        {
            return table_.FreeRow(row);
        }

        template<typename T>
        void SetComponent(uint32_t row, const T& value)
        {
            table_.template SetComponent<T>(row, value);
        }

        void SetComponent(uint32_t row, TypeId type_id, const void* data)
        {
            table_.SetComponent(row, type_id, data);
        }

        template<typename T>
        [[nodiscard]] T* GetComponent(uint32_t row) noexcept
        {
            auto* column = table_.template GetColumn<T>();
            if (column == nullptr)
            {
                return nullptr;
            }
            return column->template Get<T>(row);
        }

        template<typename T>
        [[nodiscard]] const T* GetComponent(uint32_t row) const noexcept
        {
            const auto* column = table_.template GetColumn<T>();
            if (column == nullptr)
            {
                return nullptr;
            }
            return column->template Get<T>(row);
        }

        [[nodiscard]] void* GetComponentRaw(uint32_t row, TypeId type_id) noexcept
        {
            auto* column = table_.GetColumnByTypeId(type_id);
            if (column == nullptr)
            {
                return nullptr;
            }
            return column->GetRaw(row);
        }

        [[nodiscard]] Entity GetEntity(uint32_t row) const noexcept
        {
            return table_.GetEntity(row);
        }

        [[nodiscard]] const Entity* GetEntities() const noexcept
        {
            return table_.GetEntities();
        }

        [[nodiscard]] Column<Allocator>* GetColumn(TypeId type_id) noexcept
        {
            return table_.GetColumnByTypeId(type_id);
        }

        template<typename T>
        [[nodiscard]] Column<Allocator>* GetColumn() noexcept
        {
            return table_.template GetColumn<T>();
        }

        [[nodiscard]] size_t EntityCount() const noexcept { return table_.RowCount(); }
        [[nodiscard]] size_t ComponentCount() const noexcept { return component_types_.Size(); }
        [[nodiscard]] bool IsEmpty() const noexcept { return table_.IsEmpty(); }

        [[nodiscard]] const wax::Vector<TypeId, Allocator>& GetComponentTypes() const noexcept
        {
            return component_types_;
        }

        [[nodiscard]] const wax::Vector<ComponentMeta, Allocator>& GetComponentMetas() const noexcept
        {
            return component_metas_;
        }

        void SetAddEdge(TypeId type_id, Archetype* target)
        {
            add_edges_.Insert(type_id, target);
        }

        void SetRemoveEdge(TypeId type_id, Archetype* target)
        {
            remove_edges_.Insert(type_id, target);
        }

        [[nodiscard]] Archetype* GetAddEdge(TypeId type_id) noexcept
        {
            Archetype** result = add_edges_.Find(type_id);
            return result ? *result : nullptr;
        }

        [[nodiscard]] Archetype* GetRemoveEdge(TypeId type_id) noexcept
        {
            Archetype** result = remove_edges_.Find(type_id);
            return result ? *result : nullptr;
        }

        [[nodiscard]] Table<Allocator>& GetTable() noexcept { return table_; }
        [[nodiscard]] const Table<Allocator>& GetTable() const noexcept { return table_; }

    private:
        void SortComponentTypes()
        {
            for (size_t i = 1; i < component_types_.Size(); ++i)
            {
                TypeId key = component_types_[i];
                size_t j = i;
                while (j > 0 && component_types_[j - 1] > key)
                {
                    component_types_[j] = component_types_[j - 1];
                    --j;
                }
                component_types_[j] = key;
            }
        }

        [[nodiscard]] size_t BinarySearch(TypeId type_id) const noexcept
        {
            if (component_types_.IsEmpty())
            {
                return SIZE_MAX;
            }

            size_t left = 0;
            size_t right = component_types_.Size();

            while (left < right)
            {
                size_t mid = left + (right - left) / 2;
                if (component_types_[mid] == type_id)
                {
                    return mid;
                }
                if (component_types_[mid] < type_id)
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

        Allocator* allocator_;
        ArchetypeId id_;
        wax::Vector<TypeId, Allocator> component_types_;
        wax::Vector<ComponentMeta, Allocator> component_metas_;
        Table<Allocator> table_;
        wax::HashMap<TypeId, Archetype*, Allocator> add_edges_;
        wax::HashMap<TypeId, Archetype*, Allocator> remove_edges_;
    };
}
