#pragma once

#include <queen/core/type_id.h>
#include <queen/core/component_info.h>
#include <queen/core/tick.h>
#include <comb/allocator_concepts.h>
#include <hive/core/assert.h>
#include <cstddef>
#include <cstring>

namespace queen
{
    /**
     * Type-erased component array
     *
     * Stores components of a single type in a contiguous, aligned array.
     * Used by Table to store one column per component type. Supports
     * type-erased operations via ComponentMeta function pointers.
     *
     * Memory layout:
     * ┌────────────────────────────────────────────────────────────┐
     * │ data_: aligned byte array                                  │
     * │   [Component0, Component1, Component2, ...]                │
     * │                                                            │
     * │ ticks_: ComponentTicks array (for change detection)        │
     * │   [Ticks0, Ticks1, Ticks2, ...]                            │
     * │                                                            │
     * │ Each component at: data_ + (index * stride_)               │
     * │ Stride includes alignment padding                          │
     * └────────────────────────────────────────────────────────────┘
     *
     * Performance characteristics:
     * - Push: O(1) amortized (may reallocate)
     * - Pop: O(1)
     * - SwapRemove: O(1)
     * - Get: O(1) - direct index access
     * - Memory: O(capacity * component_size)
     *
     * Limitations:
     * - Single component type per column
     * - Not thread-safe
     * - Requires ComponentMeta for lifecycle operations
     *
     * Example:
     * @code
     *   comb::LinearAllocator alloc{1_MB};
     *   Column column{alloc, ComponentMeta::Of<Position>(), 100};
     *
     *   Position pos{1.0f, 2.0f, 3.0f};
     *   column.PushCopy(&pos);
     *
     *   Position* p = column.Get<Position>(0);
     * @endcode
     */
    template<comb::Allocator Allocator>
    class Column
    {
    public:
        Column(Allocator& allocator, ComponentMeta meta, size_t initial_capacity = 64)
            : allocator_{&allocator}
            , meta_{meta}
            , data_{nullptr}
            , ticks_{nullptr}
            , size_{0}
            , capacity_{0}
        {
            hive::Assert(meta.IsValid(), "Column requires valid ComponentMeta");
            Reserve(initial_capacity);
        }

        ~Column()
        {
            Clear();
            if (data_ != nullptr)
            {
                allocator_->Deallocate(data_);
                data_ = nullptr;
            }
            if (ticks_ != nullptr)
            {
                allocator_->Deallocate(ticks_);
                ticks_ = nullptr;
            }
        }

        Column(const Column&) = delete;
        Column& operator=(const Column&) = delete;

        Column(Column&& other) noexcept
            : allocator_{other.allocator_}
            , meta_{other.meta_}
            , data_{other.data_}
            , ticks_{other.ticks_}
            , size_{other.size_}
            , capacity_{other.capacity_}
        {
            other.data_ = nullptr;
            other.ticks_ = nullptr;
            other.size_ = 0;
            other.capacity_ = 0;
        }

        Column& operator=(Column&& other) noexcept
        {
            if (this != &other)
            {
                Clear();
                if (data_ != nullptr)
                {
                    allocator_->Deallocate(data_);
                }
                if (ticks_ != nullptr)
                {
                    allocator_->Deallocate(ticks_);
                }

                allocator_ = other.allocator_;
                meta_ = other.meta_;
                data_ = other.data_;
                ticks_ = other.ticks_;
                size_ = other.size_;
                capacity_ = other.capacity_;

                other.data_ = nullptr;
                other.ticks_ = nullptr;
                other.size_ = 0;
                other.capacity_ = 0;
            }
            return *this;
        }

        void PushDefault(Tick current_tick = Tick{0})
        {
            EnsureCapacity(size_ + 1);

            void* dst = GetRaw(size_);
            if (meta_.construct != nullptr)
            {
                meta_.construct(dst);
            }
            else
            {
                std::memset(dst, 0, meta_.size);
            }
            ticks_[size_].SetAdded(current_tick);
            ++size_;
        }

        void PushCopy(const void* src, Tick current_tick = Tick{0})
        {
            hive::Assert(src != nullptr, "Cannot push null source");
            EnsureCapacity(size_ + 1);

            void* dst = GetRaw(size_);
            if (meta_.copy != nullptr)
            {
                meta_.copy(dst, src);
            }
            else
            {
                std::memcpy(dst, src, meta_.size);
            }
            ticks_[size_].SetAdded(current_tick);
            ++size_;
        }

        void PushMove(void* src, Tick current_tick = Tick{0})
        {
            hive::Assert(src != nullptr, "Cannot push null source");
            EnsureCapacity(size_ + 1);

            void* dst = GetRaw(size_);
            if (meta_.move != nullptr)
            {
                meta_.move(dst, src);
            }
            else
            {
                std::memcpy(dst, src, meta_.size);
            }
            ticks_[size_].SetAdded(current_tick);
            ++size_;
        }

        void Pop()
        {
            hive::Assert(size_ > 0, "Cannot pop from empty column");

            --size_;
            void* ptr = GetRaw(size_);
            if (meta_.destruct != nullptr)
            {
                meta_.destruct(ptr);
            }
        }

        void SwapRemove(size_t index)
        {
            hive::Assert(index < size_, "Index out of bounds");

            if (index != size_ - 1)
            {
                void* dst = GetRaw(index);
                void* src = GetRaw(size_ - 1);

                if (meta_.destruct != nullptr)
                {
                    meta_.destruct(dst);
                }

                if (meta_.move != nullptr)
                {
                    meta_.move(dst, src);
                }
                else
                {
                    std::memcpy(dst, src, meta_.size);
                }

                if (meta_.destruct != nullptr)
                {
                    meta_.destruct(src);
                }

                ticks_[index] = ticks_[size_ - 1];
            }
            else
            {
                if (meta_.destruct != nullptr)
                {
                    meta_.destruct(GetRaw(index));
                }
            }

            --size_;
        }

        [[nodiscard]] void* GetRaw(size_t index) noexcept
        {
            hive::Assert(index < capacity_, "Index out of bounds");
            return static_cast<std::byte*>(data_) + (index * meta_.size);
        }

        [[nodiscard]] const void* GetRaw(size_t index) const noexcept
        {
            hive::Assert(index < capacity_, "Index out of bounds");
            return static_cast<const std::byte*>(data_) + (index * meta_.size);
        }

        template<typename T>
        [[nodiscard]] T* Get(size_t index) noexcept
        {
            hive::Assert(TypeIdOf<T>() == meta_.type_id, "Type mismatch");
            hive::Assert(index < size_, "Index out of bounds");
            return static_cast<T*>(GetRaw(index));
        }

        template<typename T>
        [[nodiscard]] const T* Get(size_t index) const noexcept
        {
            hive::Assert(TypeIdOf<T>() == meta_.type_id, "Type mismatch");
            hive::Assert(index < size_, "Index out of bounds");
            return static_cast<const T*>(GetRaw(index));
        }

        template<typename T>
        [[nodiscard]] T* Data() noexcept
        {
            hive::Assert(TypeIdOf<T>() == meta_.type_id, "Type mismatch");
            return static_cast<T*>(data_);
        }

        template<typename T>
        [[nodiscard]] const T* Data() const noexcept
        {
            hive::Assert(TypeIdOf<T>() == meta_.type_id, "Type mismatch");
            return static_cast<const T*>(data_);
        }

        void Clear()
        {
            if (meta_.destruct != nullptr)
            {
                for (size_t i = 0; i < size_; ++i)
                {
                    meta_.destruct(GetRaw(i));
                }
            }
            size_ = 0;
        }

        void Reserve(size_t new_capacity)
        {
            if (new_capacity <= capacity_)
            {
                return;
            }

            void* new_data = allocator_->Allocate(new_capacity * meta_.size, meta_.alignment);
            hive::Assert(new_data != nullptr, "Column data allocation failed");

            ComponentTicks* new_ticks = static_cast<ComponentTicks*>(
                allocator_->Allocate(new_capacity * sizeof(ComponentTicks), alignof(ComponentTicks)));
            hive::Assert(new_ticks != nullptr, "Column ticks allocation failed");

            if (data_ != nullptr)
            {
                for (size_t i = 0; i < size_; ++i)
                {
                    void* dst = static_cast<std::byte*>(new_data) + (i * meta_.size);
                    void* src = GetRaw(i);

                    if (meta_.move != nullptr)
                    {
                        meta_.move(dst, src);
                    }
                    else
                    {
                        std::memcpy(dst, src, meta_.size);
                    }

                    if (meta_.destruct != nullptr)
                    {
                        meta_.destruct(src);
                    }

                    // Copy ticks
                    new_ticks[i] = ticks_[i];
                }

                allocator_->Deallocate(data_);
            }

            if (ticks_ != nullptr)
            {
                allocator_->Deallocate(ticks_);
            }

            data_ = new_data;
            ticks_ = new_ticks;
            capacity_ = new_capacity;
        }

        [[nodiscard]] size_t Size() const noexcept { return size_; }
        [[nodiscard]] size_t Capacity() const noexcept { return capacity_; }
        [[nodiscard]] bool IsEmpty() const noexcept { return size_ == 0; }
        [[nodiscard]] TypeId GetTypeId() const noexcept { return meta_.type_id; }
        [[nodiscard]] const ComponentMeta& GetMeta() const noexcept { return meta_; }

        /**
         * Get ticks for a component at the given index
         */
        [[nodiscard]] ComponentTicks& GetTicks(size_t index) noexcept
        {
            hive::Assert(index < size_, "Index out of bounds");
            return ticks_[index];
        }

        [[nodiscard]] const ComponentTicks& GetTicks(size_t index) const noexcept
        {
            hive::Assert(index < size_, "Index out of bounds");
            return ticks_[index];
        }

        /**
         * Get raw ticks array
         */
        [[nodiscard]] ComponentTicks* TicksData() noexcept { return ticks_; }
        [[nodiscard]] const ComponentTicks* TicksData() const noexcept { return ticks_; }

        /**
         * Mark component as changed at the given tick
         */
        void MarkChanged(size_t index, Tick current_tick) noexcept
        {
            hive::Assert(index < size_, "Index out of bounds");
            ticks_[index].MarkChanged(current_tick);
        }

    private:
        void EnsureCapacity(size_t required)
        {
            if (required > capacity_)
            {
                size_t new_capacity = capacity_ == 0 ? 8 : capacity_ * 2;
                while (new_capacity < required)
                {
                    new_capacity *= 2;
                }
                Reserve(new_capacity);
            }
        }

        Allocator* allocator_;
        ComponentMeta meta_;
        void* data_;
        ComponentTicks* ticks_;
        size_t size_;
        size_t capacity_;
    };
}
