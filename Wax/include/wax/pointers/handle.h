#pragma once

#include <hive/core/assert.h>
#include <comb/allocator_concepts.h>
#include <comb/new.h>
#include <cstdint>
#include <cstddef>

namespace wax
{
    /**
     * Generational handle for safe entity/resource references
     *
     * Combines an index with a generation counter to detect use-after-free.
     * When an object is destroyed, its slot's generation increments, invalidating
     * all existing handles to that slot.
     *
     * Use cases:
     * - ECS entity IDs (safe references across frames)
     * - Resource handles (textures, meshes, sounds)
     * - Object pools with stable IDs
     *
     * Layout: [32-bit index][32-bit generation] = 64 bits total
     *
     * Performance:
     * - Create/Destroy: O(1)
     * - Get: O(1) with generation check
     * - IsValid: O(1)
     * - Memory: 8 bytes per handle
     *
     * Limitations:
     * - Max 4 billion objects per pool
     * - Generation wraps after 4 billion destroy/create cycles per slot
     *
     * Example:
     * @code
     *   comb::PoolAllocator<Entity> pool{1000};
     *   wax::HandlePool<Entity> entities{pool, 1000};
     *
     *   auto handle = entities.Create(args...);
     *   if (Entity* e = entities.Get(handle)) {
     *       e->Update();
     *   }
     *
     *   entities.Destroy(handle);
     *   // handle.IsValid() still true, but entities.Get(handle) returns nullptr
     * @endcode
     */
    template<typename T>
    struct Handle
    {
        uint32_t index{0};
        uint32_t generation{0};

        [[nodiscard]] constexpr bool operator==(const Handle& other) const noexcept
        {
            return index == other.index && generation == other.generation;
        }

        [[nodiscard]] constexpr bool operator!=(const Handle& other) const noexcept
        {
            return !(*this == other);
        }

        [[nodiscard]] static constexpr Handle Invalid() noexcept
        {
            return Handle{UINT32_MAX, 0};
        }

        [[nodiscard]] constexpr bool IsNull() const noexcept
        {
            return index == UINT32_MAX;
        }
    };

    /**
     * Pool that manages objects with generational handles
     *
     * Provides O(1) create, destroy, and lookup with use-after-free detection.
     * Uses a free-list for slot reuse.
     *
     * @tparam T Object type to store
     * @tparam Allocator Comb allocator type
     */
    template<typename T, comb::Allocator Allocator>
    class HandlePool
    {
    private:
        struct Slot
        {
            alignas(T) unsigned char storage[sizeof(T)];
            uint32_t generation{0};
            uint32_t next_free{UINT32_MAX};
            bool alive{false};
        };

    public:
        HandlePool(Allocator& allocator, size_t capacity)
            : allocator_{&allocator}
            , capacity_{capacity}
            , count_{0}
            , first_free_{0}
        {
            hive::Assert(capacity > 0, "HandlePool capacity must be > 0");
            hive::Assert(capacity <= UINT32_MAX, "HandlePool capacity exceeds max");

            slots_ = static_cast<Slot*>(allocator.Allocate(sizeof(Slot) * capacity, alignof(Slot)));
            hive::Assert(slots_ != nullptr, "Failed to allocate HandlePool slots");

            for (size_t i = 0; i < capacity_ - 1; ++i)
            {
                slots_[i].generation = 0;
                slots_[i].next_free = static_cast<uint32_t>(i + 1);
                slots_[i].alive = false;
            }
            slots_[capacity_ - 1].generation = 0;
            slots_[capacity_ - 1].next_free = UINT32_MAX;
            slots_[capacity_ - 1].alive = false;
        }

        ~HandlePool()
        {
            for (size_t i = 0; i < capacity_; ++i)
            {
                if (slots_[i].alive)
                {
                    GetPtr(i)->~T();
                }
            }

            if (slots_ && allocator_)
            {
                allocator_->Deallocate(slots_);
            }
        }

        HandlePool(const HandlePool&) = delete;
        HandlePool& operator=(const HandlePool&) = delete;

        HandlePool(HandlePool&& other) noexcept
            : allocator_{other.allocator_}
            , slots_{other.slots_}
            , capacity_{other.capacity_}
            , count_{other.count_}
            , first_free_{other.first_free_}
        {
            other.allocator_ = nullptr;
            other.slots_ = nullptr;
            other.capacity_ = 0;
            other.count_ = 0;
            other.first_free_ = UINT32_MAX;
        }

        HandlePool& operator=(HandlePool&& other) noexcept
        {
            if (this != &other)
            {
                for (size_t i = 0; i < capacity_; ++i)
                {
                    if (slots_[i].alive)
                    {
                        GetPtr(i)->~T();
                    }
                }
                if (slots_ && allocator_)
                {
                    allocator_->Deallocate(slots_);
                }

                allocator_ = other.allocator_;
                slots_ = other.slots_;
                capacity_ = other.capacity_;
                count_ = other.count_;
                first_free_ = other.first_free_;

                other.allocator_ = nullptr;
                other.slots_ = nullptr;
                other.capacity_ = 0;
                other.count_ = 0;
                other.first_free_ = UINT32_MAX;
            }
            return *this;
        }

        template<typename... Args>
        [[nodiscard]] Handle<T> Create(Args&&... args)
        {
            if (first_free_ == UINT32_MAX)
            {
                return Handle<T>::Invalid();
            }

            uint32_t index = first_free_;
            Slot& slot = slots_[index];
            first_free_ = slot.next_free;

            new (slot.storage) T(static_cast<Args&&>(args)...);
            slot.alive = true;
            ++count_;

            return Handle<T>{index, slot.generation};
        }

        void Destroy(Handle<T> handle)
        {
            if (handle.IsNull() || handle.index >= capacity_)
            {
                return;
            }

            Slot& slot = slots_[handle.index];
            if (!slot.alive || slot.generation != handle.generation)
            {
                return;
            }

            GetPtr(handle.index)->~T();
            slot.alive = false;
            ++slot.generation;
            slot.next_free = first_free_;
            first_free_ = handle.index;
            --count_;
        }

        [[nodiscard]] T* Get(Handle<T> handle) noexcept
        {
            if (handle.IsNull() || handle.index >= capacity_)
            {
                return nullptr;
            }

            Slot& slot = slots_[handle.index];
            if (!slot.alive || slot.generation != handle.generation)
            {
                return nullptr;
            }

            return GetPtr(handle.index);
        }

        [[nodiscard]] const T* Get(Handle<T> handle) const noexcept
        {
            if (handle.IsNull() || handle.index >= capacity_)
            {
                return nullptr;
            }

            const Slot& slot = slots_[handle.index];
            if (!slot.alive || slot.generation != handle.generation)
            {
                return nullptr;
            }

            return GetPtr(handle.index);
        }

        [[nodiscard]] bool IsValid(Handle<T> handle) const noexcept
        {
            if (handle.IsNull() || handle.index >= capacity_)
            {
                return false;
            }

            const Slot& slot = slots_[handle.index];
            return slot.alive && slot.generation == handle.generation;
        }

        [[nodiscard]] size_t Count() const noexcept { return count_; }
        [[nodiscard]] size_t Capacity() const noexcept { return capacity_; }
        [[nodiscard]] bool IsEmpty() const noexcept { return count_ == 0; }
        [[nodiscard]] bool IsFull() const noexcept { return first_free_ == UINT32_MAX; }

    private:
        [[nodiscard]] T* GetPtr(size_t index) noexcept
        {
            return reinterpret_cast<T*>(slots_[index].storage);
        }

        [[nodiscard]] const T* GetPtr(size_t index) const noexcept
        {
            return reinterpret_cast<const T*>(slots_[index].storage);
        }

        Allocator* allocator_;
        Slot* slots_;
        size_t capacity_;
        size_t count_;
        uint32_t first_free_;
    };
}
