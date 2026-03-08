#pragma once

#include <hive/core/assert.h>

#include <comb/allocator_concepts.h>

#include <wax/containers/vector.h>

#include <queen/core/entity.h>

namespace queen
{
    /**
     * Record of where an entity's data is stored
     *
     * Used by the World to locate an entity's components in O(1).
     * Stores pointer to archetype and position within the archetype's table.
     * Template parameter ArchetypeType allows forward declaration.
     */
    template <typename ArchetypeType> struct EntityRecordT
    {
        static constexpr uint32_t kInvalidRow = UINT32_MAX;

        ArchetypeType* m_archetype = nullptr;
        uint32_t m_row = kInvalidRow;

        [[nodiscard]] constexpr bool IsValid() const noexcept { return m_archetype != nullptr && m_row != kInvalidRow; }

        void Invalidate() noexcept {
            m_archetype = nullptr;
            m_row = kInvalidRow;
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
    template <comb::Allocator Allocator, typename ArchetypeType> class EntityLocationMap
    {
    public:
        using Record = EntityRecordT<ArchetypeType>;

        explicit EntityLocationMap(Allocator& allocator, size_t initialCapacity = 1000)
            : m_records{allocator} {
            m_records.Reserve(initialCapacity);
        }

        void Set(Entity entity, const Record& record) {
            hive::Assert(!entity.IsNull(), "Cannot set location for null entity");

            uint32_t index = entity.Index();
            EnsureCapacity(index + 1);

            m_records[index] = record;
        }

        void Remove(Entity entity) {
            if (entity.IsNull())
            {
                return;
            }

            uint32_t index = entity.Index();
            if (index < m_records.Size())
            {
                m_records[index].Invalidate();
            }
        }

        [[nodiscard]] Record* Get(Entity entity) noexcept {
            if (entity.IsNull())
            {
                return nullptr;
            }

            uint32_t index = entity.Index();
            if (index >= m_records.Size())
            {
                return nullptr;
            }

            return &m_records[index];
        }

        [[nodiscard]] const Record* Get(Entity entity) const noexcept {
            if (entity.IsNull())
            {
                return nullptr;
            }

            uint32_t index = entity.Index();
            if (index >= m_records.Size())
            {
                return nullptr;
            }

            return &m_records[index];
        }

        [[nodiscard]] bool HasValidLocation(Entity entity) const noexcept {
            const Record* record = Get(entity);
            return record && record->IsValid();
        }

        void Clear() { m_records.Clear(); }

        [[nodiscard]] size_t Size() const noexcept { return m_records.Size(); }

        [[nodiscard]] size_t Capacity() const noexcept { return m_records.Capacity(); }

    private:
        void EnsureCapacity(size_t required) {
            while (m_records.Size() < required)
            {
                m_records.PushBack(Record{});
            }
        }

        wax::Vector<Record> m_records;
    };
} // namespace queen
