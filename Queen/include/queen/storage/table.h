#pragma once

#include <queen/core/type_id.h>
#include <queen/core/entity.h>
#include <queen/core/tick.h>
#include <queen/core/component_info.h>
#include <queen/storage/column.h>
#include <comb/allocator_concepts.h>
#include <wax/containers/hash_map.h>
#include <wax/containers/vector.h>
#include <hive/core/assert.h>

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
    template<comb::Allocator Allocator>
    class Table
    {
    public:
        Table(Allocator& allocator, const wax::Vector<ComponentMeta, Allocator>& component_metas, size_t initial_capacity = 64)
            : allocator_{&allocator}
            , entities_{allocator}
            , columns_{allocator}
            , type_to_column_index_{allocator}
        {
            entities_.Reserve(initial_capacity);

            for (size_t i = 0; i < component_metas.Size(); ++i)
            {
                const ComponentMeta& meta = component_metas[i];
                type_to_column_index_.Insert(meta.type_id, columns_.Size());
                columns_.EmplaceBack(allocator, meta, initial_capacity);
            }
        }

        ~Table() = default;

        Table(const Table&) = delete;
        Table& operator=(const Table&) = delete;
        Table(Table&&) = default;
        Table& operator=(Table&&) = default;

        uint32_t AllocateRow(Entity entity, Tick current_tick = Tick{0})
        {
            hive::Assert(!entity.IsNull(), "Cannot allocate row for null entity");

            uint32_t row = static_cast<uint32_t>(entities_.Size());
            entities_.PushBack(entity);

            for (size_t i = 0; i < columns_.Size(); ++i)
            {
                columns_[i].PushDefault(current_tick);
            }

            return row;
        }

        Entity FreeRow(uint32_t row)
        {
            hive::Assert(row < entities_.Size(), "Row index out of bounds");

            Entity moved_entity{};
            uint32_t last_row = static_cast<uint32_t>(entities_.Size() - 1);

            if (row != last_row)
            {
                moved_entity = entities_[last_row];
                entities_[row] = moved_entity;

                for (size_t i = 0; i < columns_.Size(); ++i)
                {
                    columns_[i].SwapRemove(row);
                }
            }
            else
            {
                for (size_t i = 0; i < columns_.Size(); ++i)
                {
                    columns_[i].Pop();
                }
            }

            entities_.PopBack();

            return moved_entity;
        }

        [[nodiscard]] Column<Allocator>* GetColumnByTypeId(TypeId type_id) noexcept
        {
            size_t* index = type_to_column_index_.Find(type_id);
            if (index == nullptr)
            {
                return nullptr;
            }
            return &columns_[*index];
        }

        [[nodiscard]] const Column<Allocator>* GetColumnByTypeId(TypeId type_id) const noexcept
        {
            const size_t* index = type_to_column_index_.Find(type_id);
            if (index == nullptr)
            {
                return nullptr;
            }
            return &columns_[*index];
        }

        template<typename T>
        [[nodiscard]] Column<Allocator>* GetColumn() noexcept
        {
            return GetColumnByTypeId(TypeIdOf<T>());
        }

        template<typename T>
        [[nodiscard]] const Column<Allocator>* GetColumn() const noexcept
        {
            return GetColumnByTypeId(TypeIdOf<T>());
        }

        [[nodiscard]] bool HasComponent(TypeId type_id) const noexcept
        {
            return type_to_column_index_.Find(type_id) != nullptr;
        }

        template<typename T>
        [[nodiscard]] bool HasComponent() const noexcept
        {
            return HasComponent(TypeIdOf<T>());
        }

        [[nodiscard]] Entity GetEntity(uint32_t row) const noexcept
        {
            hive::Assert(row < entities_.Size(), "Row index out of bounds");
            return entities_[row];
        }

        [[nodiscard]] const Entity* GetEntities() const noexcept
        {
            return entities_.Data();
        }

        void SetComponent(uint32_t row, TypeId type_id, const void* data)
        {
            Column<Allocator>* column = GetColumnByTypeId(type_id);
            hive::Assert(column != nullptr, "Component type not in table");
            hive::Assert(row < entities_.Size(), "Row index out of bounds");

            void* dst = column->GetRaw(row);
            const ComponentMeta& meta = column->GetMeta();

            if (meta.destruct != nullptr)
            {
                meta.destruct(dst);
            }

            if (meta.copy != nullptr)
            {
                meta.copy(dst, data);
            }
            else
            {
                std::memcpy(dst, data, meta.size);
            }
        }

        template<typename T>
        void SetComponent(uint32_t row, const T& value)
        {
            SetComponent(row, TypeIdOf<T>(), &value);
        }

        void MoveComponent(uint32_t row, TypeId type_id, void* data)
        {
            Column<Allocator>* column = GetColumnByTypeId(type_id);
            hive::Assert(column != nullptr, "Component type not in table");
            hive::Assert(row < entities_.Size(), "Row index out of bounds");

            void* dst = column->GetRaw(row);
            const ComponentMeta& meta = column->GetMeta();

            if (meta.destruct != nullptr)
            {
                meta.destruct(dst);
            }

            if (meta.move != nullptr)
            {
                meta.move(dst, data);
            }
            else
            {
                std::memcpy(dst, data, meta.size);
            }
        }

        [[nodiscard]] size_t RowCount() const noexcept { return entities_.Size(); }
        [[nodiscard]] size_t ColumnCount() const noexcept { return columns_.Size(); }
        [[nodiscard]] bool IsEmpty() const noexcept { return entities_.IsEmpty(); }

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
        size_t MoveRowTo(uint32_t source_row, Table& target, uint32_t dest_row)
        {
            hive::Assert(source_row < entities_.Size(), "Source row out of bounds");
            hive::Assert(dest_row < target.entities_.Size(), "Destination row out of bounds");

            size_t moved_count = 0;

            for (size_t i = 0; i < columns_.Size(); ++i)
            {
                Column<Allocator>& src_col = columns_[i];
                TypeId type_id = src_col.GetMeta().type_id;

                Column<Allocator>* dst_col = target.GetColumnByTypeId(type_id);
                if (dst_col != nullptr)
                {
                    void* src = src_col.GetRaw(source_row);
                    void* dst = dst_col->GetRaw(dest_row);
                    const ComponentMeta& meta = src_col.GetMeta();

                    if (meta.destruct != nullptr)
                    {
                        meta.destruct(dst);
                    }

                    if (meta.move != nullptr)
                    {
                        meta.move(dst, src);
                    }
                    else
                    {
                        std::memcpy(dst, src, meta.size);
                    }
                    ++moved_count;
                }
            }

            return moved_count;
        }

        /**
         * Get all TypeIds present in this table
         */
        [[nodiscard]] wax::Vector<TypeId, Allocator> GetTypeIds() const
        {
            wax::Vector<TypeId, Allocator> result{*allocator_};
            result.Reserve(columns_.Size());
            for (size_t i = 0; i < columns_.Size(); ++i)
            {
                result.PushBack(columns_[i].GetMeta().type_id);
            }
            return result;
        }

    private:
        Allocator* allocator_;
        wax::Vector<Entity, Allocator> entities_;
        wax::Vector<Column<Allocator>, Allocator> columns_;
        wax::HashMap<TypeId, size_t, Allocator> type_to_column_index_;
    };
}
