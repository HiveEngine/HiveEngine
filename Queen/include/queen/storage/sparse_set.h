#pragma once

#include <queen/core/entity.h>
#include <comb/allocator_concepts.h>
#include <hive/core/assert.h>
#include <cstdint>
#include <cstddef>

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
    template<typename T, comb::Allocator Allocator>
    class SparseSet
    {
    public:
        static constexpr uint32_t kInvalidIndex = UINT32_MAX;

        SparseSet(Allocator& allocator, size_t sparse_capacity, size_t dense_capacity)
            : allocator_{&allocator}
            , sparse_capacity_{sparse_capacity}
            , dense_capacity_{dense_capacity}
            , count_{0}
        {
            hive::Assert(sparse_capacity > 0, "Sparse capacity must be > 0");
            hive::Assert(dense_capacity > 0, "Dense capacity must be > 0");

            sparse_ = static_cast<uint32_t*>(
                allocator.Allocate(sizeof(uint32_t) * sparse_capacity, alignof(uint32_t))
            );
            hive::Assert(sparse_ != nullptr, "Failed to allocate sparse array");

            dense_ = static_cast<Entity*>(
                allocator.Allocate(sizeof(Entity) * dense_capacity, alignof(Entity))
            );
            hive::Assert(dense_ != nullptr, "Failed to allocate dense array");

            data_ = static_cast<T*>(
                allocator.Allocate(sizeof(T) * dense_capacity, alignof(T))
            );
            hive::Assert(data_ != nullptr, "Failed to allocate data array");

            for (size_t i = 0; i < sparse_capacity; ++i)
            {
                sparse_[i] = kInvalidIndex;
            }
        }

        ~SparseSet()
        {
            Clear();

            if (sparse_ && allocator_)
            {
                allocator_->Deallocate(sparse_);
            }
            if (dense_ && allocator_)
            {
                allocator_->Deallocate(dense_);
            }
            if (data_ && allocator_)
            {
                allocator_->Deallocate(data_);
            }
        }

        SparseSet(const SparseSet&) = delete;
        SparseSet& operator=(const SparseSet&) = delete;

        SparseSet(SparseSet&& other) noexcept
            : allocator_{other.allocator_}
            , sparse_{other.sparse_}
            , dense_{other.dense_}
            , data_{other.data_}
            , sparse_capacity_{other.sparse_capacity_}
            , dense_capacity_{other.dense_capacity_}
            , count_{other.count_}
        {
            other.allocator_ = nullptr;
            other.sparse_ = nullptr;
            other.dense_ = nullptr;
            other.data_ = nullptr;
            other.count_ = 0;
        }

        SparseSet& operator=(SparseSet&& other) noexcept
        {
            if (this != &other)
            {
                Clear();
                if (sparse_) allocator_->Deallocate(sparse_);
                if (dense_) allocator_->Deallocate(dense_);
                if (data_) allocator_->Deallocate(data_);

                allocator_ = other.allocator_;
                sparse_ = other.sparse_;
                dense_ = other.dense_;
                data_ = other.data_;
                sparse_capacity_ = other.sparse_capacity_;
                dense_capacity_ = other.dense_capacity_;
                count_ = other.count_;

                other.allocator_ = nullptr;
                other.sparse_ = nullptr;
                other.dense_ = nullptr;
                other.data_ = nullptr;
                other.count_ = 0;
            }
            return *this;
        }

        bool Insert(Entity entity, const T& value)
        {
            if (Contains(entity))
            {
                return false;
            }

            uint32_t index = entity.Index();
            hive::Assert(index < sparse_capacity_, "Entity index exceeds sparse capacity");
            hive::Assert(count_ < dense_capacity_, "Dense array is full");

            sparse_[index] = static_cast<uint32_t>(count_);
            dense_[count_] = entity;
            new (&data_[count_]) T(value);
            ++count_;

            return true;
        }

        bool Insert(Entity entity, T&& value)
        {
            if (Contains(entity))
            {
                return false;
            }

            uint32_t index = entity.Index();
            hive::Assert(index < sparse_capacity_, "Entity index exceeds sparse capacity");
            hive::Assert(count_ < dense_capacity_, "Dense array is full");

            sparse_[index] = static_cast<uint32_t>(count_);
            dense_[count_] = entity;
            new (&data_[count_]) T(static_cast<T&&>(value));
            ++count_;

            return true;
        }

        template<typename... Args>
        bool Emplace(Entity entity, Args&&... args)
        {
            if (Contains(entity))
            {
                return false;
            }

            uint32_t index = entity.Index();
            hive::Assert(index < sparse_capacity_, "Entity index exceeds sparse capacity");
            hive::Assert(count_ < dense_capacity_, "Dense array is full");

            sparse_[index] = static_cast<uint32_t>(count_);
            dense_[count_] = entity;
            new (&data_[count_]) T(static_cast<Args&&>(args)...);
            ++count_;

            return true;
        }

        bool Remove(Entity entity)
        {
            if (!Contains(entity))
            {
                return false;
            }

            uint32_t index = entity.Index();
            uint32_t dense_index = sparse_[index];

            data_[dense_index].~T();

            if (dense_index < count_ - 1)
            {
                Entity last_entity = dense_[count_ - 1];
                dense_[dense_index] = last_entity;
                new (&data_[dense_index]) T(static_cast<T&&>(data_[count_ - 1]));
                data_[count_ - 1].~T();

                sparse_[last_entity.Index()] = dense_index;
            }

            sparse_[index] = kInvalidIndex;
            --count_;

            return true;
        }

        [[nodiscard]] bool Contains(Entity entity) const noexcept
        {
            uint32_t index = entity.Index();
            if (index >= sparse_capacity_)
            {
                return false;
            }

            uint32_t dense_index = sparse_[index];
            if (dense_index >= count_)
            {
                return false;
            }

            return dense_[dense_index] == entity;
        }

        [[nodiscard]] T* Get(Entity entity) noexcept
        {
            if (!Contains(entity))
            {
                return nullptr;
            }

            uint32_t dense_index = sparse_[entity.Index()];
            return &data_[dense_index];
        }

        [[nodiscard]] const T* Get(Entity entity) const noexcept
        {
            if (!Contains(entity))
            {
                return nullptr;
            }

            uint32_t dense_index = sparse_[entity.Index()];
            return &data_[dense_index];
        }

        [[nodiscard]] T& GetUnchecked(Entity entity) noexcept
        {
            hive::Assert(Contains(entity), "Entity not in sparse set");
            uint32_t dense_index = sparse_[entity.Index()];
            return data_[dense_index];
        }

        [[nodiscard]] const T& GetUnchecked(Entity entity) const noexcept
        {
            hive::Assert(Contains(entity), "Entity not in sparse set");
            uint32_t dense_index = sparse_[entity.Index()];
            return data_[dense_index];
        }

        void Clear()
        {
            if constexpr (!std::is_trivially_destructible_v<T>)
            {
                for (size_t i = 0; i < count_; ++i)
                {
                    data_[i].~T();
                }
            }

            for (size_t i = 0; i < count_; ++i)
            {
                sparse_[dense_[i].Index()] = kInvalidIndex;
            }

            count_ = 0;
        }

        [[nodiscard]] size_t Count() const noexcept { return count_; }
        [[nodiscard]] size_t DenseCapacity() const noexcept { return dense_capacity_; }
        [[nodiscard]] size_t SparseCapacity() const noexcept { return sparse_capacity_; }
        [[nodiscard]] bool IsEmpty() const noexcept { return count_ == 0; }
        [[nodiscard]] bool IsFull() const noexcept { return count_ >= dense_capacity_; }

        [[nodiscard]] Entity* DenseBegin() noexcept { return dense_; }
        [[nodiscard]] Entity* DenseEnd() noexcept { return dense_ + count_; }
        [[nodiscard]] const Entity* DenseBegin() const noexcept { return dense_; }
        [[nodiscard]] const Entity* DenseEnd() const noexcept { return dense_ + count_; }

        [[nodiscard]] T* DataBegin() noexcept { return data_; }
        [[nodiscard]] T* DataEnd() noexcept { return data_ + count_; }
        [[nodiscard]] const T* DataBegin() const noexcept { return data_; }
        [[nodiscard]] const T* DataEnd() const noexcept { return data_ + count_; }

        [[nodiscard]] Entity EntityAt(size_t dense_index) const noexcept
        {
            hive::Assert(dense_index < count_, "Dense index out of bounds");
            return dense_[dense_index];
        }

        [[nodiscard]] T& DataAt(size_t dense_index) noexcept
        {
            hive::Assert(dense_index < count_, "Dense index out of bounds");
            return data_[dense_index];
        }

        [[nodiscard]] const T& DataAt(size_t dense_index) const noexcept
        {
            hive::Assert(dense_index < count_, "Dense index out of bounds");
            return data_[dense_index];
        }

    private:
        Allocator* allocator_;
        uint32_t* sparse_;
        Entity* dense_;
        T* data_;
        size_t sparse_capacity_;
        size_t dense_capacity_;
        size_t count_;
    };
}
