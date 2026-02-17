#pragma once

#include <queen/core/entity.h>
#include <comb/allocator_concepts.h>
#include <wax/containers/vector.h>
#include <hive/core/assert.h>

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
    template<comb::Allocator Allocator>
    class EntityAllocator
    {
    public:
        explicit EntityAllocator(Allocator& allocator, size_t initial_capacity = 1000)
            : generations_{allocator}
            , free_list_{allocator}
            , next_index_{0}
        {
            generations_.Reserve(initial_capacity);
            free_list_.Reserve(initial_capacity / 4);
        }

        Entity Allocate()
        {
            if (!free_list_.IsEmpty())
            {
                uint32_t index = free_list_.Back();
                free_list_.PopBack();

                Entity::GenerationType gen = generations_[index];
                return Entity{index, gen, Entity::Flags::kAlive};
            }

            uint32_t index = next_index_++;
            hive::Assert(index <= Entity::kMaxIndex, "Entity index overflow");

            if (index >= generations_.Size())
            {
                generations_.PushBack(0);
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

            Entity::GenerationType& gen = generations_[index];
            ++gen;

            free_list_.PushBack(index);
        }

        [[nodiscard]] bool IsAlive(Entity entity) const noexcept
        {
            if (entity.IsNull())
            {
                return false;
            }

            uint32_t index = entity.Index();
            if (index >= generations_.Size())
            {
                return false;
            }

            return generations_[index] == entity.Generation();
        }

        [[nodiscard]] size_t AliveCount() const noexcept
        {
            return next_index_ - free_list_.Size();
        }

        [[nodiscard]] size_t Capacity() const noexcept
        {
            return generations_.Capacity();
        }

        [[nodiscard]] size_t TotalAllocated() const noexcept
        {
            return next_index_;
        }

        [[nodiscard]] size_t FreeListSize() const noexcept
        {
            return free_list_.Size();
        }

        void Clear()
        {
            generations_.Clear();
            free_list_.Clear();
            next_index_ = 0;
        }

    private:
        wax::Vector<Entity::GenerationType, Allocator> generations_;
        wax::Vector<uint32_t, Allocator> free_list_;
        uint32_t next_index_;
    };
}
