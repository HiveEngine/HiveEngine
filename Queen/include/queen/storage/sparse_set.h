#pragma once

#include <hive/core/assert.h>

#include <comb/allocator_concepts.h>

#include <queen/core/entity.h>

#include <cstddef>
#include <cstdint>

namespace queen
{
    /**
     * Sparse set for entity-to-data mapping
     *
     * Provides O(1) insert, remove, lookup, and contains with dense iteration.
     * Uses a sparse array (indexed by entity) pointing to a dense array.
     *
     * Memory layout:
     * ┌─────────────────────────────────────────────────────────────┐
     * │ Sparse Array (indexed by Entity.Index()):                   │
     * │ [_, 0, _, 2, 1, _, ...]  → index into dense array           │
     * │                                                             │
     * │ Dense Array (packed entities):                              │
     * │ [e2, e5, e4]  → actual Entity values                        │
     * │                                                             │
     * │ Data Array (parallel to dense):                             │
     * │ [t2, t5, t4]  → component data                              │
     * └─────────────────────────────────────────────────────────────┘
     *
     * Performance characteristics:
     * - Insert: O(1) amortized
     * - Remove: O(1) (swap-and-pop)
     * - Contains: O(1)
     * - Get: O(1)
     * - Iteration: O(n) where n = count (dense)
     * - Memory: O(max_entity_index) + O(n)
     *
     * Limitations:
     * - Sparse array grows with max entity index
     * - Not thread-safe
     * - Data order not preserved after remove
     *
     * Use cases:
     * - Component storage for volatile components
     * - Entity sets for queries
     * - Relationship storage
     *
     * Example:
     * @code
     *   comb::LinearAllocator alloc{1_MB};
     *   queen::SparseSet<Position, comb::LinearAllocator> positions{alloc, 1000, 100};
     *
     *   Entity e = ...;
     *   positions.Insert(e, Position{1, 2, 3});
     *
     *   if (positions.Contains(e)) {
     *       Position& pos = positions.Get(e);
     *       pos.x += 1.0f;
     *   }
     *
     *   positions.Remove(e);
     * @endcode
     */
    template <typename T, comb::Allocator Allocator> class SparseSet
    {
    public:
        static constexpr uint32_t kInvalidIndex = UINT32_MAX;

        SparseSet(Allocator& allocator, size_t sparseCapacity, size_t denseCapacity)
            : m_allocator{&allocator}
            , m_sparseCapacity{sparseCapacity}
            , m_denseCapacity{denseCapacity}
            , m_count{0} {
            hive::Assert(sparseCapacity > 0, "Sparse capacity must be > 0");
            hive::Assert(denseCapacity > 0, "Dense capacity must be > 0");

            m_sparse = static_cast<uint32_t*>(allocator.Allocate(sizeof(uint32_t) * sparseCapacity, alignof(uint32_t)));
            hive::Assert(m_sparse != nullptr, "Failed to allocate sparse array");

            m_dense = static_cast<Entity*>(allocator.Allocate(sizeof(Entity) * denseCapacity, alignof(Entity)));
            hive::Assert(m_dense != nullptr, "Failed to allocate dense array");

            m_data = static_cast<T*>(allocator.Allocate(sizeof(T) * denseCapacity, alignof(T)));
            hive::Assert(m_data != nullptr, "Failed to allocate data array");

            for (size_t i = 0; i < sparseCapacity; ++i)
            {
                m_sparse[i] = kInvalidIndex;
            }
        }

        ~SparseSet() {
            Clear();

            if (m_sparse && m_allocator)
            {
                m_allocator->Deallocate(m_sparse);
            }
            if (m_dense && m_allocator)
            {
                m_allocator->Deallocate(m_dense);
            }
            if (m_data && m_allocator)
            {
                m_allocator->Deallocate(m_data);
            }
        }

        SparseSet(const SparseSet&) = delete;
        SparseSet& operator=(const SparseSet&) = delete;

        SparseSet(SparseSet&& other) noexcept
            : m_allocator{other.m_allocator}
            , m_sparse{other.m_sparse}
            , m_dense{other.m_dense}
            , m_data{other.m_data}
            , m_sparseCapacity{other.m_sparseCapacity}
            , m_denseCapacity{other.m_denseCapacity}
            , m_count{other.m_count} {
            other.m_allocator = nullptr;
            other.m_sparse = nullptr;
            other.m_dense = nullptr;
            other.m_data = nullptr;
            other.m_count = 0;
        }

        SparseSet& operator=(SparseSet&& other) noexcept {
            if (this != &other)
            {
                Clear();
                if (m_sparse)
                    m_allocator->Deallocate(m_sparse);
                if (m_dense)
                    m_allocator->Deallocate(m_dense);
                if (m_data)
                    m_allocator->Deallocate(m_data);

                m_allocator = other.m_allocator;
                m_sparse = other.m_sparse;
                m_dense = other.m_dense;
                m_data = other.m_data;
                m_sparseCapacity = other.m_sparseCapacity;
                m_denseCapacity = other.m_denseCapacity;
                m_count = other.m_count;

                other.m_allocator = nullptr;
                other.m_sparse = nullptr;
                other.m_dense = nullptr;
                other.m_data = nullptr;
                other.m_count = 0;
            }
            return *this;
        }

        bool Insert(Entity entity, const T& value) {
            if (Contains(entity))
            {
                return false;
            }

            uint32_t index = entity.Index();
            hive::Assert(index < m_sparseCapacity, "Entity index exceeds sparse capacity");
            hive::Assert(m_count < m_denseCapacity, "Dense array is full");

            m_sparse[index] = static_cast<uint32_t>(m_count);
            m_dense[m_count] = entity;
            new (&m_data[m_count]) T{value};
            ++m_count;

            return true;
        }

        bool Insert(Entity entity, T&& value) {
            if (Contains(entity))
            {
                return false;
            }

            uint32_t index = entity.Index();
            hive::Assert(index < m_sparseCapacity, "Entity index exceeds sparse capacity");
            hive::Assert(m_count < m_denseCapacity, "Dense array is full");

            m_sparse[index] = static_cast<uint32_t>(m_count);
            m_dense[m_count] = entity;
            new (&m_data[m_count]) T{static_cast<T&&>(value)};
            ++m_count;

            return true;
        }

        template <typename... Args> bool Emplace(Entity entity, Args&&... args) {
            if (Contains(entity))
            {
                return false;
            }

            uint32_t index = entity.Index();
            hive::Assert(index < m_sparseCapacity, "Entity index exceeds sparse capacity");
            hive::Assert(m_count < m_denseCapacity, "Dense array is full");

            m_sparse[index] = static_cast<uint32_t>(m_count);
            m_dense[m_count] = entity;
            new (&m_data[m_count]) T{static_cast<Args&&>(args)...};
            ++m_count;

            return true;
        }

        bool Remove(Entity entity) {
            if (!Contains(entity))
            {
                return false;
            }

            uint32_t index = entity.Index();
            uint32_t denseIndex = m_sparse[index];

            m_data[denseIndex].~T();

            if (denseIndex < m_count - 1)
            {
                Entity lastEntity = m_dense[m_count - 1];
                m_dense[denseIndex] = lastEntity;
                new (&m_data[denseIndex]) T{static_cast<T&&>(m_data[m_count - 1])};
                m_data[m_count - 1].~T();

                m_sparse[lastEntity.Index()] = denseIndex;
            }

            m_sparse[index] = kInvalidIndex;
            --m_count;

            return true;
        }

        [[nodiscard]] bool Contains(Entity entity) const noexcept {
            uint32_t index = entity.Index();
            if (index >= m_sparseCapacity)
            {
                return false;
            }

            uint32_t denseIndex = m_sparse[index];
            if (denseIndex >= m_count)
            {
                return false;
            }

            return m_dense[denseIndex] == entity;
        }

        [[nodiscard]] T* Get(Entity entity) noexcept {
            if (!Contains(entity))
            {
                return nullptr;
            }

            uint32_t denseIndex = m_sparse[entity.Index()];
            return &m_data[denseIndex];
        }

        [[nodiscard]] const T* Get(Entity entity) const noexcept {
            if (!Contains(entity))
            {
                return nullptr;
            }

            uint32_t denseIndex = m_sparse[entity.Index()];
            return &m_data[denseIndex];
        }

        [[nodiscard]] T& GetUnchecked(Entity entity) noexcept {
            hive::Assert(Contains(entity), "Entity not in sparse set");
            uint32_t denseIndex = m_sparse[entity.Index()];
            return m_data[denseIndex];
        }

        [[nodiscard]] const T& GetUnchecked(Entity entity) const noexcept {
            hive::Assert(Contains(entity), "Entity not in sparse set");
            uint32_t denseIndex = m_sparse[entity.Index()];
            return m_data[denseIndex];
        }

        void Clear() {
            if constexpr (!std::is_trivially_destructible_v<T>)
            {
                for (size_t i = 0; i < m_count; ++i)
                {
                    m_data[i].~T();
                }
            }

            for (size_t i = 0; i < m_count; ++i)
            {
                m_sparse[m_dense[i].Index()] = kInvalidIndex;
            }

            m_count = 0;
        }

        [[nodiscard]] size_t Count() const noexcept { return m_count; }
        [[nodiscard]] size_t DenseCapacity() const noexcept { return m_denseCapacity; }
        [[nodiscard]] size_t SparseCapacity() const noexcept { return m_sparseCapacity; }
        [[nodiscard]] bool IsEmpty() const noexcept { return m_count == 0; }
        [[nodiscard]] bool IsFull() const noexcept { return m_count >= m_denseCapacity; }

        [[nodiscard]] Entity* DenseBegin() noexcept { return m_dense; }
        [[nodiscard]] Entity* DenseEnd() noexcept { return m_dense + m_count; }
        [[nodiscard]] const Entity* DenseBegin() const noexcept { return m_dense; }
        [[nodiscard]] const Entity* DenseEnd() const noexcept { return m_dense + m_count; }

        [[nodiscard]] T* DataBegin() noexcept { return m_data; }
        [[nodiscard]] T* DataEnd() noexcept { return m_data + m_count; }
        [[nodiscard]] const T* DataBegin() const noexcept { return m_data; }
        [[nodiscard]] const T* DataEnd() const noexcept { return m_data + m_count; }

        [[nodiscard]] Entity EntityAt(size_t denseIndex) const noexcept {
            hive::Assert(denseIndex < m_count, "Dense index out of bounds");
            return m_dense[denseIndex];
        }

        [[nodiscard]] T& DataAt(size_t denseIndex) noexcept {
            hive::Assert(denseIndex < m_count, "Dense index out of bounds");
            return m_data[denseIndex];
        }

        [[nodiscard]] const T& DataAt(size_t denseIndex) const noexcept {
            hive::Assert(denseIndex < m_count, "Dense index out of bounds");
            return m_data[denseIndex];
        }

    private:
        Allocator* m_allocator;
        uint32_t* m_sparse;
        Entity* m_dense;
        T* m_data;
        size_t m_sparseCapacity;
        size_t m_denseCapacity;
        size_t m_count;
    };
} // namespace queen
