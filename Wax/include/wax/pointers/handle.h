#pragma once

#include <hive/core/assert.h>

#include <comb/allocator_concepts.h>
#include <comb/new.h>

#include <cstddef>
#include <cstdint>

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
    template <typename T> struct Handle
    {
        uint32_t m_index{0};
        uint32_t m_generation{0};

        [[nodiscard]] constexpr bool operator==(const Handle& other) const noexcept
        {
            return m_index == other.m_index && m_generation == other.m_generation;
        }

        [[nodiscard]] static constexpr Handle Invalid() noexcept
        {
            return Handle{UINT32_MAX, 0};
        }

        [[nodiscard]] constexpr bool IsNull() const noexcept
        {
            return m_index == UINT32_MAX;
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
    template <typename T, comb::Allocator Allocator> class HandlePool
    {
    private:
        struct Slot
        {
            alignas(T) unsigned char m_storage[sizeof(T)];
            uint32_t m_generation{0};
            uint32_t m_nextFree{UINT32_MAX};
            bool m_alive{false};
        };

    public:
        HandlePool(Allocator& allocator, size_t capacity)
            : m_allocator{&allocator}
            , m_capacity{capacity}
            , m_count{0}
            , m_firstFree{0}
        {
            hive::Assert(capacity > 0, "HandlePool capacity must be > 0");
            hive::Assert(capacity <= UINT32_MAX, "HandlePool capacity exceeds max");

            m_slots = static_cast<Slot*>(allocator.Allocate(sizeof(Slot) * capacity, alignof(Slot)));
            hive::Assert(m_slots != nullptr, "Failed to allocate HandlePool slots");

            for (size_t i = 0; i < m_capacity - 1; ++i)
            {
                m_slots[i].m_generation = 0;
                m_slots[i].m_nextFree = static_cast<uint32_t>(i + 1);
                m_slots[i].m_alive = false;
            }
            m_slots[m_capacity - 1].m_generation = 0;
            m_slots[m_capacity - 1].m_nextFree = UINT32_MAX;
            m_slots[m_capacity - 1].m_alive = false;
        }

        ~HandlePool()
        {
            for (size_t i = 0; i < m_capacity; ++i)
            {
                if (m_slots[i].m_alive)
                {
                    GetPtr(i)->~T();
                }
            }

            if (m_slots && m_allocator)
            {
                m_allocator->Deallocate(m_slots);
            }
        }

        HandlePool(const HandlePool&) = delete;
        HandlePool& operator=(const HandlePool&) = delete;

        HandlePool(HandlePool&& other) noexcept
            : m_allocator{other.m_allocator}
            , m_slots{other.m_slots}
            , m_capacity{other.m_capacity}
            , m_count{other.m_count}
            , m_firstFree{other.m_firstFree}
        {
            other.m_allocator = nullptr;
            other.m_slots = nullptr;
            other.m_capacity = 0;
            other.m_count = 0;
            other.m_firstFree = UINT32_MAX;
        }

        HandlePool& operator=(HandlePool&& other) noexcept
        {
            if (this != &other)
            {
                for (size_t i = 0; i < m_capacity; ++i)
                {
                    if (m_slots[i].m_alive)
                    {
                        GetPtr(i)->~T();
                    }
                }
                if (m_slots && m_allocator)
                {
                    m_allocator->Deallocate(m_slots);
                }

                m_allocator = other.m_allocator;
                m_slots = other.m_slots;
                m_capacity = other.m_capacity;
                m_count = other.m_count;
                m_firstFree = other.m_firstFree;

                other.m_allocator = nullptr;
                other.m_slots = nullptr;
                other.m_capacity = 0;
                other.m_count = 0;
                other.m_firstFree = UINT32_MAX;
            }
            return *this;
        }

        template <typename... Args> [[nodiscard]] Handle<T> Create(Args&&... args)
        {
            if (m_firstFree == UINT32_MAX)
            {
                return Handle<T>::Invalid();
            }

            uint32_t index = m_firstFree;
            Slot& slot = m_slots[index];
            m_firstFree = slot.m_nextFree;

            new (slot.m_storage) T(static_cast<Args&&>(args)...);
            slot.m_alive = true;
            ++m_count;

            return Handle<T>{index, slot.m_generation};
        }

        void Destroy(Handle<T> handle)
        {
            if (handle.IsNull() || handle.m_index >= m_capacity)
            {
                return;
            }

            Slot& slot = m_slots[handle.m_index];
            if (!slot.m_alive || slot.m_generation != handle.m_generation)
            {
                return;
            }

            GetPtr(handle.m_index)->~T();
            slot.m_alive = false;
            ++slot.m_generation;
            slot.m_nextFree = m_firstFree;
            m_firstFree = handle.m_index;
            --m_count;
        }

        [[nodiscard]] T* Get(Handle<T> handle) noexcept
        {
            if (handle.IsNull() || handle.m_index >= m_capacity)
            {
                return nullptr;
            }

            Slot& slot = m_slots[handle.m_index];
            if (!slot.m_alive || slot.m_generation != handle.m_generation)
            {
                return nullptr;
            }

            return GetPtr(handle.m_index);
        }

        [[nodiscard]] const T* Get(Handle<T> handle) const noexcept
        {
            if (handle.IsNull() || handle.m_index >= m_capacity)
            {
                return nullptr;
            }

            const Slot& slot = m_slots[handle.m_index];
            if (!slot.m_alive || slot.m_generation != handle.m_generation)
            {
                return nullptr;
            }

            return GetPtr(handle.m_index);
        }

        [[nodiscard]] bool IsValid(Handle<T> handle) const noexcept
        {
            if (handle.IsNull() || handle.m_index >= m_capacity)
            {
                return false;
            }

            const Slot& slot = m_slots[handle.m_index];
            return slot.m_alive && slot.m_generation == handle.m_generation;
        }

        [[nodiscard]] size_t Count() const noexcept
        {
            return m_count;
        }
        [[nodiscard]] size_t Capacity() const noexcept
        {
            return m_capacity;
        }
        [[nodiscard]] bool IsEmpty() const noexcept
        {
            return m_count == 0;
        }
        [[nodiscard]] bool IsFull() const noexcept
        {
            return m_firstFree == UINT32_MAX;
        }

    private:
        [[nodiscard]] T* GetPtr(size_t index) noexcept
        {
            return reinterpret_cast<T*>(m_slots[index].m_storage);
        }

        [[nodiscard]] const T* GetPtr(size_t index) const noexcept
        {
            return reinterpret_cast<const T*>(m_slots[index].m_storage);
        }

        Allocator* m_allocator;
        Slot* m_slots;
        size_t m_capacity;
        size_t m_count;
        uint32_t m_firstFree;
    };
} // namespace wax
