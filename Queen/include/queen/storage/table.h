#pragma once

#include <hive/core/assert.h>

#include <comb/allocator_concepts.h>

#include <wax/containers/hash_map.h>
#include <wax/containers/vector.h>

#include <queen/core/component_info.h>
#include <queen/core/entity.h>
#include <queen/core/tick.h>
#include <queen/core/type_id.h>
#include <queen/storage/column.h>

#include <type_traits>

namespace queen
{
    /**
     * Archetype storage table
     *
     * Stores entities and their components in a Structure-of-Arrays layout.
     * Each component type has its own Column for cache-friendly iteration.
     * Entities are stored in a separate column for entity-to-row mapping.
     *
     * Memory layout:
     * ┌────────────────────────────────────────────────────────────┐
     * │ entities_: [Entity0, Entity1, Entity2, ...]                │
     * │                                                            │
     * │ columns_: HashMap<TypeId, Column>                          │
     * │   TypeId_A -> [A0, A1, A2, ...]                            │
     * │   TypeId_B -> [B0, B1, B2, ...]                            │
     * │   TypeId_C -> [C0, C1, C2, ...]                            │
     * │                                                            │
     * │ Row i contains: entities_[i], columns_[A][i], ...          │
     * └────────────────────────────────────────────────────────────┘
     *
     * Performance characteristics:
     * - AllocateRow: O(C) where C = number of columns
     * - FreeRow (swap-and-pop): O(C)
     * - GetColumn: O(1) hash lookup
     * - Iteration: O(N) cache-friendly per column
     *
     * Limitations:
     * - Fixed set of component types after construction
     * - Not thread-safe
     * - Swap-and-pop changes row indices
     *
     * Example:
     * @code
     *   comb::LinearAllocator alloc{1_MB};
     *   wax::Vector<ComponentMeta, ...> metas{alloc};
     *   metas.PushBack(ComponentMeta::Of<Position>());
     *   metas.PushBack(ComponentMeta::Of<Velocity>());
     *
     *   Table table{alloc, metas, 1000};
     *
     *   uint32_t row = table.AllocateRow(entity);
     *   table.GetColumn<Position>()->Get<Position>(row)->x = 1.0f;
     * @endcode
     */
    template <comb::Allocator Allocator> class Table
    {
    public:
        Table(Allocator& allocator, const wax::Vector<ComponentMeta>& componentMetas, size_t initialCapacity = 64)
            : m_allocator{&allocator}
            , m_entities{allocator}
            , m_columns{allocator}
            , m_typeToColumnIndex{allocator}
        {
            m_entities.Reserve(initialCapacity);

            for (size_t i = 0; i < componentMetas.Size(); ++i)
            {
                const ComponentMeta& meta = componentMetas[i];
                m_typeToColumnIndex.Insert(meta.m_typeId, m_columns.Size());
                m_columns.EmplaceBack(allocator, meta, initialCapacity);
            }
        }

        ~Table() = default;

        Table(const Table&) = delete;
        Table& operator=(const Table&) = delete;
        Table(Table&&) = default;
        Table& operator=(Table&&) = default;

        uint32_t AllocateRow(Entity entity, Tick currentTick = Tick{0})
        {
            hive::Assert(!entity.IsNull(), "Cannot allocate row for null entity");

            uint32_t row = static_cast<uint32_t>(m_entities.Size());
            m_entities.PushBack(entity);

            for (size_t i = 0; i < m_columns.Size(); ++i)
            {
                m_columns[i].PushDefault(currentTick);
            }

            return row;
        }

        Entity FreeRow(uint32_t row)
        {
            hive::Assert(row < m_entities.Size(), "Row index out of bounds");

            Entity movedEntity{};
            uint32_t lastRow = static_cast<uint32_t>(m_entities.Size() - 1);

            if (row != lastRow)
            {
                movedEntity = m_entities[lastRow];
                m_entities[row] = movedEntity;

                for (size_t i = 0; i < m_columns.Size(); ++i)
                {
                    m_columns[i].SwapRemove(row);
                }
            }
            else
            {
                for (size_t i = 0; i < m_columns.Size(); ++i)
                {
                    m_columns[i].Pop();
                }
            }

            m_entities.PopBack();

            return movedEntity;
        }

        [[nodiscard]] Column<Allocator>* GetColumnByTypeId(TypeId typeId) noexcept
        {
            size_t* index = m_typeToColumnIndex.Find(typeId);
            if (index == nullptr)
            {
                return nullptr;
            }
            return &m_columns[*index];
        }

        [[nodiscard]] const Column<Allocator>* GetColumnByTypeId(TypeId typeId) const noexcept
        {
            const size_t* index = m_typeToColumnIndex.Find(typeId);
            if (index == nullptr)
            {
                return nullptr;
            }
            return &m_columns[*index];
        }

        template <typename T> [[nodiscard]] Column<Allocator>* GetColumn() noexcept
        {
            return GetColumnByTypeId(TypeIdOf<T>());
        }

        template <typename T> [[nodiscard]] const Column<Allocator>* GetColumn() const noexcept
        {
            return GetColumnByTypeId(TypeIdOf<T>());
        }

        [[nodiscard]] bool HasComponent(TypeId typeId) const noexcept
        {
            return m_typeToColumnIndex.Find(typeId) != nullptr;
        }

        template <typename T> [[nodiscard]] bool HasComponent() const noexcept
        {
            return HasComponent(TypeIdOf<T>());
        }

        [[nodiscard]] Entity GetEntity(uint32_t row) const noexcept
        {
            hive::Assert(row < m_entities.Size(), "Row index out of bounds");
            return m_entities[row];
        }

        [[nodiscard]] const Entity* GetEntities() const noexcept
        {
            return m_entities.Data();
        }

        void SetComponent(uint32_t row, TypeId typeId, const void* data)
        {
            Column<Allocator>* column = GetColumnByTypeId(typeId);
            hive::Assert(column != nullptr, "Component type not in table");
            hive::Assert(row < m_entities.Size(), "Row index out of bounds");

            void* dst = column->GetRaw(row);
            const ComponentMeta& meta = column->GetMeta();

            if (meta.m_destruct != nullptr)
            {
                meta.m_destruct(dst);
            }

            if (meta.m_copy != nullptr)
            {
                meta.m_copy(dst, data);
            }
            else
            {
                std::memcpy(dst, data, meta.m_size);
            }
        }

        template <typename T> void SetComponent(uint32_t row, const T& value)
        {
            SetComponent(row, TypeIdOf<T>(), &value);
        }

        template <typename T>
        void SetComponent(uint32_t row, T&& value)
            requires(!std::is_lvalue_reference_v<T &&>)
        {
            MoveComponent(row, TypeIdOf<std::remove_cvref_t<T>>(), &value);
        }

        void MoveComponent(uint32_t row, TypeId typeId, void* data)
        {
            Column<Allocator>* column = GetColumnByTypeId(typeId);
            hive::Assert(column != nullptr, "Component type not in table");
            hive::Assert(row < m_entities.Size(), "Row index out of bounds");

            void* dst = column->GetRaw(row);
            const ComponentMeta& meta = column->GetMeta();

            if (meta.m_destruct != nullptr)
            {
                meta.m_destruct(dst);
            }

            if (meta.m_move != nullptr)
            {
                meta.m_move(dst, data);
            }
            else
            {
                std::memcpy(dst, data, meta.m_size);
            }
        }

        [[nodiscard]] size_t RowCount() const noexcept
        {
            return m_entities.Size();
        }
        [[nodiscard]] size_t ColumnCount() const noexcept
        {
            return m_columns.Size();
        }
        [[nodiscard]] bool IsEmpty() const noexcept
        {
            return m_entities.IsEmpty();
        }

        /**
         * Move a row to another table
         *
         * Moves all common components from source_row in this table to dest_row in target.
         * Components that exist only in this table are destroyed.
         * Components that exist only in target must be initialized separately.
         *
         * @param source_row Row index in this table
         * @param target Target table to move to
         * @param dest_row Row index in target table
         * @return Number of components moved
         */
        size_t MoveRowTo(uint32_t sourceRow, Table& target, uint32_t destRow)
        {
            hive::Assert(sourceRow < m_entities.Size(), "Source row out of bounds");
            hive::Assert(destRow < target.m_entities.Size(), "Destination row out of bounds");

            size_t movedCount = 0;

            for (size_t i = 0; i < m_columns.Size(); ++i)
            {
                Column<Allocator>& srcCol = m_columns[i];
                TypeId typeId = srcCol.GetMeta().m_typeId;

                Column<Allocator>* dstCol = target.GetColumnByTypeId(typeId);
                if (dstCol != nullptr)
                {
                    void* src = srcCol.GetRaw(sourceRow);
                    void* dst = dstCol->GetRaw(destRow);
                    const ComponentMeta& meta = srcCol.GetMeta();

                    if (meta.m_destruct != nullptr)
                    {
                        meta.m_destruct(dst);
                    }

                    if (meta.m_move != nullptr)
                    {
                        meta.m_move(dst, src);
                    }
                    else
                    {
                        std::memcpy(dst, src, meta.m_size);
                    }

                    dstCol->GetTicks(destRow) = srcCol.GetTicks(sourceRow);
                    ++movedCount;
                }
            }

            return movedCount;
        }

        /**
         * Get all TypeIds present in this table
         */
        [[nodiscard]] wax::Vector<TypeId> GetTypeIds() const
        {
            wax::Vector<TypeId> result{*m_allocator};
            result.Reserve(m_columns.Size());
            for (size_t i = 0; i < m_columns.Size(); ++i)
            {
                result.PushBack(m_columns[i].GetMeta().m_typeId);
            }
            return result;
        }

    private:
        Allocator* m_allocator;
        wax::Vector<Entity> m_entities;
        wax::Vector<Column<Allocator>> m_columns;
        wax::HashMap<TypeId, size_t> m_typeToColumnIndex;
    };
} // namespace queen
