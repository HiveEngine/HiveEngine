#pragma once

#include <cstddef>
#include <cstring>
#include <utility>
#include <type_traits>
#include <initializer_list>
#include <hive/core/assert.h>
#include <comb/default_allocator.h>

namespace wax
{
    /**
     * Dynamic array with custom allocator support
     *
     * Vector provides a resizable array that manages its own memory using
     * a pluggable allocator system.
     * control over memory allocation strategies.
     *
     * Performance characteristics:
     * - Storage: Heap-allocated via custom allocator
     * - Access: O(1) - direct pointer arithmetic
     * - Push/Pop: O(1) amortized - may trigger reallocation
     * - Insert/Erase: O(n) - requires shifting elements
     * - Reallocation: O(n) - copies/moves all elements
     * - Growth factor: 2x capacity when full
     *
     * Memory layout:
     * - 24 bytes (64-bit): pointer + size + capacity
     * - Elements stored contiguously in allocator memory
     *
     * Limitations:
     * - No exception safety (aborts on allocation failure)
     * - Allocator must outlive the vector
     * - Cannot shrink capacity without manual intervention
     *
     * Use cases:
     * - Dynamic collections with known allocator lifetime
     * - Hot-path data structures with custom memory strategies
     * - Replacing std::vector in game/engine code
     *
     * Example:
     * @code
     *   comb::LinearAllocator alloc{1024 * 1024};
     *   wax::Vector<int, comb::LinearAllocator> vec{alloc};
     *
     *   vec.PushBack(1);
     *   vec.PushBack(2);
     *   vec.PushBack(3);
     *
     *   for (int val : vec) {
     *       // ...
     *   }
     * @endcode
     */
    template<typename T, typename Allocator = comb::DefaultAllocator>
    class Vector
    {
    public:
        using ValueType = T;
        using SizeType = size_t;
        using Iterator = T*;
        using ConstIterator = const T*;

        // Default constructor - uses global default allocator
        Vector() noexcept
            requires std::is_same_v<Allocator, comb::DefaultAllocator>
            : data_{nullptr}
            , size_{0}
            , capacity_{0}
            , allocator_{&comb::GetDefaultAllocator()}
        {}

        // Default constructor with initial capacity - uses global default allocator
        explicit Vector(size_t initial_capacity)
            requires std::is_same_v<Allocator, comb::DefaultAllocator>
            : data_{nullptr}
            , size_{0}
            , capacity_{0}
            , allocator_{&comb::GetDefaultAllocator()}
        {
            Reserve(initial_capacity);
        }

        // Constructor with allocator
        explicit Vector(Allocator& allocator) noexcept
            : data_{nullptr}
            , size_{0}
            , capacity_{0}
            , allocator_{&allocator}
        {}

        // Constructor with allocator and initial capacity
        Vector(Allocator& allocator, size_t initial_capacity)
            : data_{nullptr}
            , size_{0}
            , capacity_{0}
            , allocator_{&allocator}
        {
            Reserve(initial_capacity);
        }

        // Initializer list constructor - uses global default allocator
        Vector(std::initializer_list<T> init)
            requires std::is_same_v<Allocator, comb::DefaultAllocator>
            : data_{nullptr}
            , size_{0}
            , capacity_{0}
            , allocator_{&comb::GetDefaultAllocator()}
        {
            Reserve(init.size());
            for (const auto& val : init)
            {
                new (&data_[size_]) T(val);
                ++size_;
            }
        }

        // Initializer list constructor with allocator
        Vector(Allocator& allocator, std::initializer_list<T> init)
            : data_{nullptr}
            , size_{0}
            , capacity_{0}
            , allocator_{&allocator}
        {
            Reserve(init.size());
            for (const auto& val : init)
            {
                new (&data_[size_]) T(val);
                ++size_;
            }
        }

        // Destructor
        ~Vector() noexcept
        {
            Clear();
            if (data_)
            {
                allocator_->Deallocate(data_);
                data_ = nullptr;
                capacity_ = 0;
            }
        }

        // Copy constructor
        Vector(const Vector& other)
            : data_{nullptr}
            , size_{0}
            , capacity_{0}
            , allocator_{other.allocator_}
        {
            if (other.size_ > 0)
            {
                Reserve(other.size_);
                if constexpr (std::is_trivially_copyable_v<T>)
                {
                    std::memcpy(data_, other.data_, other.size_ * sizeof(T));
                }
                else
                {
                    for (size_t i = 0; i < other.size_; ++i)
                    {
                        new (&data_[i]) T(other.data_[i]);
                    }
                }
                size_ = other.size_;
            }
        }

        // Copy assignment
        Vector& operator=(const Vector& other)
        {
            if (this != &other)
            {
                Clear();
                if (data_)
                {
                    allocator_->Deallocate(data_);
                    data_ = nullptr;
                    capacity_ = 0;
                }

                allocator_ = other.allocator_;
                if (other.size_ > 0)
                {
                    Reserve(other.size_);
                    if constexpr (std::is_trivially_copyable_v<T>)
                    {
                        std::memcpy(data_, other.data_, other.size_ * sizeof(T));
                    }
                    else
                    {
                        for (size_t i = 0; i < other.size_; ++i)
                        {
                            new (&data_[i]) T(other.data_[i]);
                        }
                    }
                    size_ = other.size_;
                }
            }
            return *this;
        }

        // Move constructor
        Vector(Vector&& other) noexcept
            : data_{other.data_}
            , size_{other.size_}
            , capacity_{other.capacity_}
            , allocator_{other.allocator_}
        {
            other.data_ = nullptr;
            other.size_ = 0;
            other.capacity_ = 0;
        }

        // Move assignment
        Vector& operator=(Vector&& other) noexcept
        {
            if (this != &other)
            {
                Clear();
                if (data_)
                {
                    allocator_->Deallocate(data_);
                }

                data_ = other.data_;
                size_ = other.size_;
                capacity_ = other.capacity_;
                allocator_ = other.allocator_;

                other.data_ = nullptr;
                other.size_ = 0;
                other.capacity_ = 0;
            }
            return *this;
        }

        // Element access (bounds-checked in debug)
        [[nodiscard]] T& operator[](size_t index) noexcept
        {
            hive::Assert(index < size_, "Vector index out of bounds");
            return data_[index];
        }

        [[nodiscard]] const T& operator[](size_t index) const noexcept
        {
            hive::Assert(index < size_, "Vector index out of bounds");
            return data_[index];
        }

        // Element access (always bounds-checked)
        [[nodiscard]] T& At(size_t index)
        {
            hive::Check(index < size_, "Vector index out of bounds");
            return data_[index];
        }

        [[nodiscard]] const T& At(size_t index) const
        {
            hive::Check(index < size_, "Vector index out of bounds");
            return data_[index];
        }

        // First and last element access
        [[nodiscard]] T& Front() noexcept
        {
            hive::Assert(size_ > 0, "Vector is empty");
            return data_[0];
        }

        [[nodiscard]] const T& Front() const noexcept
        {
            hive::Assert(size_ > 0, "Vector is empty");
            return data_[0];
        }

        [[nodiscard]] T& Back() noexcept
        {
            hive::Assert(size_ > 0, "Vector is empty");
            return data_[size_ - 1];
        }

        [[nodiscard]] const T& Back() const noexcept
        {
            hive::Assert(size_ > 0, "Vector is empty");
            return data_[size_ - 1];
        }

        // Raw data access
        [[nodiscard]] T* Data() noexcept
        {
            return data_;
        }

        [[nodiscard]] const T* Data() const noexcept
        {
            return data_;
        }

        // Size information
        [[nodiscard]] size_t Size() const noexcept
        {
            return size_;
        }

        [[nodiscard]] size_t Capacity() const noexcept
        {
            return capacity_;
        }

        [[nodiscard]] bool IsEmpty() const noexcept
        {
            return size_ == 0;
        }

        // Iterator support
        [[nodiscard]] Iterator begin() noexcept
        {
            return data_;
        }

        [[nodiscard]] ConstIterator begin() const noexcept
        {
            return data_;
        }

        [[nodiscard]] Iterator end() noexcept
        {
            return data_ + size_;
        }

        [[nodiscard]] ConstIterator end() const noexcept
        {
            return data_ + size_;
        }

        // Capacity management
        void Reserve(size_t new_capacity)
        {
            if (new_capacity <= capacity_)
            {
                return;
            }

            T* new_data = static_cast<T*>(allocator_->Allocate(new_capacity * sizeof(T), alignof(T)));
            hive::Check(new_data != nullptr, "Vector allocation failed");

            // Move or copy existing elements
            if (data_)
            {
                if constexpr (std::is_trivially_copyable_v<T>)
                {
                    std::memcpy(new_data, data_, size_ * sizeof(T));
                }
                else
                {
                    for (size_t i = 0; i < size_; ++i)
                    {
                        new (&new_data[i]) T(std::move(data_[i]));
                        data_[i].~T();
                    }
                }

                allocator_->Deallocate(data_);
            }

            data_ = new_data;
            capacity_ = new_capacity;
        }

        void ShrinkToFit()
        {
            if (size_ == capacity_)
            {
                return;
            }

            if (size_ == 0)
            {
                if (data_)
                {
                    allocator_->Deallocate(data_);
                    data_ = nullptr;
                }
                capacity_ = 0;
                return;
            }

            T* new_data = static_cast<T*>(allocator_->Allocate(size_ * sizeof(T), alignof(T)));
            hive::Check(new_data != nullptr, "Vector allocation failed");

            if constexpr (std::is_trivially_copyable_v<T>)
            {
                std::memcpy(new_data, data_, size_ * sizeof(T));
            }
            else
            {
                for (size_t i = 0; i < size_; ++i)
                {
                    new (&new_data[i]) T(std::move(data_[i]));
                    data_[i].~T();
                }
            }

            allocator_->Deallocate(data_);
            data_ = new_data;
            capacity_ = size_;
        }

        // Modifiers
        void Clear() noexcept
        {
            if constexpr (!std::is_trivially_destructible_v<T>)
            {
                for (size_t i = 0; i < size_; ++i)
                {
                    data_[i].~T();
                }
            }
            size_ = 0;
        }

        void PushBack(const T& value)
        {
            if (size_ == capacity_)
            {
                size_t new_capacity = capacity_ == 0 ? 8 : capacity_ * 2;
                Reserve(new_capacity);
            }

            new (&data_[size_]) T(value);
            ++size_;
        }

        void PushBack(T&& value)
        {
            if (size_ == capacity_)
            {
                size_t new_capacity = capacity_ == 0 ? 8 : capacity_ * 2;
                Reserve(new_capacity);
            }

            new (&data_[size_]) T(std::move(value));
            ++size_;
        }

        template<typename... Args>
        T& EmplaceBack(Args&&... args)
        {
            if (size_ == capacity_)
            {
                size_t new_capacity = capacity_ == 0 ? 8 : capacity_ * 2;
                Reserve(new_capacity);
            }

            new (&data_[size_]) T(std::forward<Args>(args)...);
            ++size_;
            return data_[size_ - 1];
        }

        void PopBack() noexcept
        {
            hive::Assert(size_ > 0, "Vector is empty");
            --size_;
            if constexpr (!std::is_trivially_destructible_v<T>)
            {
                data_[size_].~T();
            }
        }

        void Resize(size_t new_size)
        {
            if (new_size > capacity_)
            {
                Reserve(new_size);
            }

            if (new_size > size_)
            {
                // Default construct new elements
                for (size_t i = size_; i < new_size; ++i)
                {
                    new (&data_[i]) T();
                }
            }
            else if (new_size < size_)
            {
                // Destroy excess elements
                if constexpr (!std::is_trivially_destructible_v<T>)
                {
                    for (size_t i = new_size; i < size_; ++i)
                    {
                        data_[i].~T();
                    }
                }
            }

            size_ = new_size;
        }

        void Resize(size_t new_size, const T& value)
        {
            if (new_size > capacity_)
            {
                Reserve(new_size);
            }

            if (new_size > size_)
            {
                // Copy construct new elements
                for (size_t i = size_; i < new_size; ++i)
                {
                    new (&data_[i]) T(value);
                }
            }
            else if (new_size < size_)
            {
                // Destroy excess elements
                if constexpr (!std::is_trivially_destructible_v<T>)
                {
                    for (size_t i = new_size; i < size_; ++i)
                    {
                        data_[i].~T();
                    }
                }
            }

            size_ = new_size;
        }

    private:
        T* data_;
        size_t size_;
        size_t capacity_;
        Allocator* allocator_;
    };
}
