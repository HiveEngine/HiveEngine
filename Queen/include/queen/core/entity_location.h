#pragma once

#include <queen/core/entity.h>
#include <comb/allocator_concepts.h>
#include <wax/containers/vector.h>
#include <hive/core/assert.h>

namespace queen
{
    /**
     * Record of where an entity's data is stored
     *
     * Used by the World to locate an entity's components in O(1).
     * Stores pointer to archetype and position within the archetype's table.
     * Template parameter ArchetypeType allows forward declaration.
     */
    template<typename ArchetypeType>
    struct EntityRecordT
    {
        static constexpr uint32_t kInvalidRow = UINT32_MAX;

        ArchetypeType* archetype = nullptr;
        uint32_t row = kInvalidRow;

        [[nodiscard]] constexpr bool IsValid() const noexcept
        {
            return archetype != nullptr && row != kInvalidRow;
        }

        void Invalidate() noexcept
        {
            archetype = nullptr;
            row = kInvalidRow;
        }
    };

    /**
     * Maps entities to their storage location (with archetype pointer)
     *
     * Provides O(1) lookup from Entity to (Archetype*, Row) for fast
     * component access. Indexed by entity index, so grows with max entity.
     *
     * Performance characteristics:
     * - Get: O(1) - direct array access
     * - Set: O(1) - direct array access
     * - Memory: O(max_entity_index) * sizeof(EntityRecordT)
     *
     * Limitations:
     * - Memory grows with highest entity index ever used
     * - Not thread-safe
     */
    template<comb::Allocator Allocator, typename ArchetypeType>
    class EntityLocationMap
    {
    public:
        using Record = EntityRecordT<ArchetypeType>;

        explicit EntityLocationMap(Allocator& allocator, size_t initial_capacity = 1000)
            : records_{allocator}
        {
            records_.Reserve(initial_capacity);
        }

        void Set(Entity entity, const Record& record)
        {
            hive::Assert(!entity.IsNull(), "Cannot set location for null entity");

            uint32_t index = entity.Index();
            EnsureCapacity(index + 1);

            records_[index] = record;
        }

        void Remove(Entity entity)
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

        [[nodiscard]] Record* Get(Entity entity) noexcept
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

        [[nodiscard]] const Record* Get(Entity entity) const noexcept
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
            const Record* record = Get(entity);
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
                records_.PushBack(Record{});
            }
        }

        wax::Vector<Record, Allocator> records_;
    };
}
