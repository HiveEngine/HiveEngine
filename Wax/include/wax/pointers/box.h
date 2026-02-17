#pragma once

#include <utility>
#include <type_traits>
#include <hive/core/assert.h>
#include <comb/new.h>
#include <comb/default_allocator.h>

namespace wax
{
    /**
     * Unique ownership smart pointer with custom allocator support
     *
     * Box<T, Allocator> is a unique-ownership smart pointer that manages the lifetime
     * of a dynamically allocated object using a custom allocator. Similar to std::unique_ptr
     * but with explicit allocator control for game engines.
     *
     * Performance characteristics:
     * - Storage: 16 bytes (pointer + allocator reference on 64-bit)
     * - Access: O(1) - direct dereference
     * - Construction: O(1) - allocation + construction
     * - Destruction: O(1) - destruction + deallocation
     * - Move: O(1) - pointer swap
     * - Copy: Deleted (unique ownership)
     *
     * Memory layout:
     * ┌─────────────────────┐
     * │ T* ptr (8 bytes)    │
     * │ Allocator* alloc    │
     * └─────────────────────┘
     *   16 bytes total
     *
     * Limitations:
     * - Not copyable (unique ownership)
     * - Allocator must outlive the Box
     * - No array support (use Vector instead)
     * - Not thread-safe (no synchronization)
     *
     * Use cases:
     * - Unique ownership of dynamically allocated objects
     * - RAII for complex objects
     * - Replacing std::unique_ptr with explicit allocator
     * - Factory functions returning owned objects
     *
     * Example:
     * @code
     *   comb::LinearAllocator alloc{1024};
     *
     *   // Create owned object
     *   auto camera = wax::MakeBox<Camera>(alloc, position, rotation);
     *   camera->Update();
     *
     *   // Automatically destroyed when out of scope
     *   {
     *       auto temp = wax::MakeBox<Entity>(alloc);
     *   }  // temp destroyed here
     *
     *   // Move ownership
     *   auto moved = std::move(camera);
     * @endcode
     */
    template<typename T, typename Allocator = comb::DefaultAllocator>
    class Box
    {
    public:
        using ValueType = T;
        using AllocatorType = Allocator;

        constexpr Box() noexcept
            : ptr_{nullptr}
            , allocator_{nullptr}
        {}

        constexpr Box(Allocator& allocator, T* ptr) noexcept
            : ptr_{ptr}
            , allocator_{&allocator}
        {}

        Box(const Box&) = delete;
        Box& operator=(const Box&) = delete;

        constexpr Box(Box&& other) noexcept
            : ptr_{other.ptr_}
            , allocator_{other.allocator_}
        {
            other.ptr_ = nullptr;
            other.allocator_ = nullptr;
        }

        constexpr Box& operator=(Box&& other) noexcept
        {
            if (this != &other)
            {
                Reset();

                ptr_ = other.ptr_;
                allocator_ = other.allocator_;

                other.ptr_ = nullptr;
                other.allocator_ = nullptr;
            }
            return *this;
        }

        ~Box() noexcept
        {
            Reset();
        }

        [[nodiscard]] constexpr T& operator*() const noexcept
        {
            hive::Assert(ptr_ != nullptr, "Dereferencing null Box");
            return *ptr_;
        }

        [[nodiscard]] constexpr T* operator->() const noexcept
        {
            hive::Assert(ptr_ != nullptr, "Dereferencing null Box");
            return ptr_;
        }

        [[nodiscard]] constexpr T* Get() const noexcept
        {
            return ptr_;
        }

        [[nodiscard]] constexpr Allocator* GetAllocator() const noexcept
        {
            return allocator_;
        }

        [[nodiscard]] constexpr explicit operator bool() const noexcept
        {
            return ptr_ != nullptr;
        }

        [[nodiscard]] constexpr bool IsNull() const noexcept
        {
            return ptr_ == nullptr;
        }

        [[nodiscard]] constexpr bool IsValid() const noexcept
        {
            return ptr_ != nullptr;
        }

        [[nodiscard]] constexpr T* Release() noexcept
        {
            T* temp = ptr_;
            ptr_ = nullptr;
            allocator_ = nullptr;
            return temp;
        }

        constexpr void Reset() noexcept
        {
            if (ptr_ && allocator_)
            {
                comb::Delete(*allocator_, ptr_);
            }
            ptr_ = nullptr;
            allocator_ = nullptr;
        }

        constexpr void Reset(T* ptr) noexcept
        {
            if (ptr_ && allocator_)
            {
                comb::Delete(*allocator_, ptr_);
            }
            ptr_ = ptr;
        }

        [[nodiscard]] constexpr bool operator==(const Box& other) const noexcept
        {
            return ptr_ == other.ptr_;
        }

        [[nodiscard]] constexpr bool operator!=(const Box& other) const noexcept
        {
            return ptr_ != other.ptr_;
        }

        [[nodiscard]] constexpr bool operator==(std::nullptr_t) const noexcept
        {
            return ptr_ == nullptr;
        }

        [[nodiscard]] constexpr bool operator!=(std::nullptr_t) const noexcept
        {
            return ptr_ != nullptr;
        }

    private:
        T* ptr_;
        Allocator* allocator_;
    };

    template<typename T, typename Allocator, typename... Args>
    [[nodiscard]] Box<T, Allocator> MakeBox(Allocator& allocator, Args&&... args)
    {
        T* ptr = comb::New<T>(allocator, std::forward<Args>(args)...);
        return Box<T, Allocator>{allocator, ptr};
    }

    // Default allocator overload
    template<typename T, typename... Args>
    [[nodiscard]] Box<T, comb::DefaultAllocator> MakeBox(Args&&... args)
    {
        auto& allocator = comb::GetDefaultAllocator();
        T* ptr = comb::New<T>(allocator, std::forward<Args>(args)...);
        return Box<T, comb::DefaultAllocator>{allocator, ptr};
    }
}
