#pragma once

#include <queen/core/entity.h>
#include <queen/core/type_id.h>
#include <comb/allocator_concepts.h>
#include <wax/containers/vector.h>
#include <hive/core/assert.h>

namespace queen
{
    /**
     * Record of where an entity's data is stored
     *
     * Used by the World to locate an entity's components in O(1).
     * Stores archetype ID and position within the archetype's table.
     */
    struct EntityRecord
    {
        static constexpr TypeId kInvalidArchetype = 0;
        static constexpr uint32_t kInvalidRow = UINT32_MAX;

        TypeId archetype_id = kInvalidArchetype;
        uint32_t row = kInvalidRow;

        [[nodiscard]] constexpr bool IsValid() const noexcept
        {
            return archetype_id != kInvalidArchetype && row != kInvalidRow;
        }

        void Invalidate() noexcept
        {
            archetype_id = kInvalidArchetype;
            row = kInvalidRow;
        }
    };

    /**
     * Maps entities to their storage location
     *
     * Provides O(1) lookup from Entity to (Archetype, Row) for fast
     * component access. Indexed by entity index, so grows with max entity.
     *
     * Memory layout:
     * ┌─────────────────────────────────────────────────────────────┐
     * │ records_: [EntityRecord0, EntityRecord1, EntityRecord2, ...] │
     * │                                                             │
     * │ EntityRecord:                                               │
     * │   archetype_id: TypeId (8 bytes)                            │
     * │   row: uint32_t (4 bytes)                                   │
     * └─────────────────────────────────────────────────────────────┘
     *
     * Performance characteristics:
     * - Get: O(1) - direct array access
     * - Set: O(1) - direct array access
     * - Memory: O(max_entity_index) * sizeof(EntityRecord)
     *
     * Limitations:
     * - Memory grows with highest entity index ever used
     * - Not thread-safe
     *
     * Example:
     * @code
     *   EntityLocationMap<comb::LinearAllocator> locations{alloc, 10000};
     *
     *   locations.Set(entity, archetype_id, row_in_table);
     *
     *   EntityRecord* record = locations.Get(entity);
     *   if (record && record->IsValid()) {
     *       Archetype* arch = world.GetArchetype(record->archetype_id);
     *       T* component = arch->GetColumn<T>()[record->row];
     *   }
     * @endcode
     */
    template<comb::Allocator Allocator>
    class EntityLocationMap
    {
    public:
        explicit EntityLocationMap(Allocator& allocator, size_t initial_capacity = 1000)
            : records_{allocator}
        {
            records_.Reserve(initial_capacity);
        }

        void Set(Entity entity, TypeId archetype_id, uint32_t row)
        {
            hive::Assert(!entity.IsNull(), "Cannot set location for null entity");

            uint32_t index = entity.Index();
            EnsureCapacity(index + 1);

            records_[index].archetype_id = archetype_id;
            records_[index].row = row;
        }

        void Invalidate(Entity entity)
        {
            if (entity.IsNull())
            {
                return;
            }

            uint32_t index = entity.Index();
            if (index < records_.Size())
            {
                records_[index].Invalidate();
            }
        }

        [[nodiscard]] EntityRecord* Get(Entity entity) noexcept
        {
            if (entity.IsNull())
            {
                return nullptr;
            }

            uint32_t index = entity.Index();
            if (index >= records_.Size())
            {
                return nullptr;
            }

            return &records_[index];
        }

        [[nodiscard]] const EntityRecord* Get(Entity entity) const noexcept
        {
            if (entity.IsNull())
            {
                return nullptr;
            }

            uint32_t index = entity.Index();
            if (index >= records_.Size())
            {
                return nullptr;
            }

            return &records_[index];
        }

        [[nodiscard]] bool HasValidLocation(Entity entity) const noexcept
        {
            const EntityRecord* record = Get(entity);
            return record && record->IsValid();
        }

        void Clear()
        {
            records_.Clear();
        }

        [[nodiscard]] size_t Size() const noexcept
        {
            return records_.Size();
        }

        [[nodiscard]] size_t Capacity() const noexcept
        {
            return records_.Capacity();
        }

    private:
        void EnsureCapacity(size_t required)
        {
            while (records_.Size() < required)
            {
                records_.PushBack(EntityRecord{});
            }
        }

        wax::Vector<EntityRecord, Allocator> records_;
    };
}
