#pragma once

#include <hive/core/assert.h>

#include <comb/allocator_concepts.h>

#include <wax/containers/vector.h>

#include <queen/core/entity.h>

namespace queen
{
    /**
     * Entity ID allocator with generation-based recycling
     *
     * Manages entity ID allocation and deallocation with generation counters
     * to detect use-after-free. Maintains a free list for O(1) recycling.
     *
     * Memory layout:
     * ┌─────────────────────────────────────────────────────────────┐
     * │ generations_: [gen0, gen1, gen2, ...]  (per-index)          │
     * │ free_list_:   [idx5, idx2, idx0]       (recycled indices)   │
     * │ next_index_:  6                        (next fresh ID)      │
     * └─────────────────────────────────────────────────────────────┘
     *
     * Performance characteristics:
     * - Allocate: O(1)
     * - Deallocate: O(1)
     * - IsAlive: O(1)
     * - Memory: O(max_allocated_entities)
     *
     * Limitations:
     * - Generation wraps at 65536 deallocations of same index (rare false positives)
     * - Not thread-safe
     * - Generations array grows monotonically (never shrinks)
     *
     * Example:
     * @code
     *   comb::LinearAllocator alloc{1_MB};
     *   queen::EntityAllocator<comb::LinearAllocator> allocator{alloc, 10000};
     *
     *   Entity e = allocator.Allocate();
     *   // ... use entity
     *   allocator.Deallocate(e);
     *
     *   Entity recycled = allocator.Allocate();  // reuses e.Index()
     *   assert(recycled.Generation() > e.Generation());
     * @endcode
     */
    template <comb::Allocator Allocator> class EntityAllocator
    {
    public:
        explicit EntityAllocator(Allocator& allocator, size_t initialCapacity = 1000)
            : m_generations{allocator}
            , m_freeList{allocator}
            , m_nextIndex{0}
        {
            m_generations.Reserve(initialCapacity);
            m_freeList.Reserve(initialCapacity / 4);
        }

        Entity Allocate()
        {
            if (!m_freeList.IsEmpty())
            {
                uint32_t index = m_freeList.Back();
                m_freeList.PopBack();

                Entity::GenerationType gen = m_generations[index];
                return Entity{index, gen, Entity::Flags::kAlive};
            }

            uint32_t index = m_nextIndex++;
            hive::Assert(index <= Entity::kMaxIndex, "Entity index overflow");

            if (index >= m_generations.Size())
            {
                m_generations.PushBack(0);
            }

            return Entity{index, 0, Entity::Flags::kAlive};
        }

        void Deallocate(Entity entity)
        {
            if (!IsAlive(entity))
            {
                return;
            }

            uint32_t index = entity.Index();

            Entity::GenerationType& gen = m_generations[index];
            ++gen;

            m_freeList.PushBack(index);
        }

        [[nodiscard]] bool IsAlive(Entity entity) const noexcept
        {
            if (entity.IsNull())
            {
                return false;
            }

            uint32_t index = entity.Index();
            if (index >= m_generations.Size())
            {
                return false;
            }

            return m_generations[index] == entity.Generation();
        }

        [[nodiscard]] size_t AliveCount() const noexcept
        {
            return m_nextIndex - m_freeList.Size();
        }

        [[nodiscard]] size_t Capacity() const noexcept
        {
            return m_generations.Capacity();
        }

        [[nodiscard]] size_t TotalAllocated() const noexcept
        {
            return m_nextIndex;
        }

        [[nodiscard]] size_t FreeListSize() const noexcept
        {
            return m_freeList.Size();
        }

        void Clear()
        {
            m_generations.Clear();
            m_freeList.Clear();
            m_nextIndex = 0;
        }

    private:
        wax::Vector<Entity::GenerationType> m_generations;
        wax::Vector<uint32_t> m_freeList;
        uint32_t m_nextIndex;
    };
} // namespace queen
