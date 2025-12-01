#pragma once

#include <type_traits>
#include <hive/core/assert.h>

namespace wax
{
    /**
     * Non-owning nullable pointer wrapper
     *
     * Ptr<T> is a rebindable, non-owning pointer that can be null. It provides
     * type-safe pointer semantics with explicit null checks via bool conversion.
     * Unlike raw pointers, Ptr has clearer semantics and better IDE support.
     *
     * Performance characteristics:
     * - Storage: 8 bytes (same as raw pointer on 64-bit)
     * - Access: O(1) - direct dereference (zero overhead)
     * - Copy: O(1) - trivially copyable
     * - Comparison: O(1) - pointer comparison
     * - Null check: O(1) - single compare
     * - Zero runtime overhead (no vtable, no virtual calls)
     *
     * Limitations:
     * - Non-owning (does not manage lifetime)
     * - Caller must ensure referenced object stays alive
     * - Can be null (requires explicit checks)
     * - Not thread-safe (no synchronization)
     *
     * Use cases:
     * - Optional references (may or may not exist)
     * - Return nullable references from functions
     * - Iteration over nullable collections
     * - Observer pattern (non-owning observers)
     *
     * Example:
     * @code
     *   wax::Ptr<Entity> FindEntity(EntityID id) {
     *       if (entities.Contains(id)) {
     *           return &entities[id];
     *       }
     *       return nullptr;
     *   }
     *
     *   auto entity = FindEntity(id);
     *   if (entity) {  // Explicit null check
     *       entity->Update();
     *   }
     *
     *   // Can be stored in containers
     *   wax::Vector<wax::Ptr<Entity>> observers{alloc};
     *   observers.PushBack(entity);
     * @endcode
     */
    template<typename T>
    class Ptr
    {
    public:
        using ValueType = T;

        constexpr Ptr() noexcept
            : ptr_{nullptr}
        {}

        constexpr Ptr(std::nullptr_t) noexcept
            : ptr_{nullptr}
        {}

        constexpr Ptr(T* ptr) noexcept
            : ptr_{ptr}
        {}

        constexpr Ptr(T& value) noexcept
            : ptr_{&value}
        {}

        constexpr Ptr(const Ptr&) noexcept = default;
        constexpr Ptr& operator=(const Ptr&) noexcept = default;

        constexpr Ptr& operator=(std::nullptr_t) noexcept
        {
            ptr_ = nullptr;
            return *this;
        }

        constexpr Ptr& operator=(T* ptr) noexcept
        {
            ptr_ = ptr;
            return *this;
        }

        [[nodiscard]] constexpr T& operator*() const noexcept
        {
            hive::Assert(ptr_ != nullptr, "Dereferencing null Ptr");
            return *ptr_;
        }

        [[nodiscard]] constexpr T* operator->() const noexcept
        {
            hive::Assert(ptr_ != nullptr, "Dereferencing null Ptr");
            return ptr_;
        }

        [[nodiscard]] constexpr T* Get() const noexcept
        {
            return ptr_;
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

        constexpr void Reset() noexcept
        {
            ptr_ = nullptr;
        }

        constexpr void Rebind(T* ptr) noexcept
        {
            ptr_ = ptr;
        }

        constexpr void Rebind(T& value) noexcept
        {
            ptr_ = &value;
        }

        [[nodiscard]] constexpr bool operator==(const Ptr& other) const noexcept
        {
            return ptr_ == other.ptr_;
        }

        [[nodiscard]] constexpr bool operator!=(const Ptr& other) const noexcept
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

        [[nodiscard]] constexpr bool operator<(const Ptr& other) const noexcept
        {
            return ptr_ < other.ptr_;
        }

        [[nodiscard]] constexpr bool operator<=(const Ptr& other) const noexcept
        {
            return ptr_ <= other.ptr_;
        }

        [[nodiscard]] constexpr bool operator>(const Ptr& other) const noexcept
        {
            return ptr_ > other.ptr_;
        }

        [[nodiscard]] constexpr bool operator>=(const Ptr& other) const noexcept
        {
            return ptr_ >= other.ptr_;
        }

    private:
        T* ptr_;
    };

    template<typename T>
    [[nodiscard]] constexpr bool operator==(std::nullptr_t, const Ptr<T>& ptr) noexcept
    {
        return ptr == nullptr;
    }

    template<typename T>
    [[nodiscard]] constexpr bool operator!=(std::nullptr_t, const Ptr<T>& ptr) noexcept
    {
        return ptr != nullptr;
    }

    template<typename T>
    Ptr(T*) -> Ptr<T>;

    template<typename T>
    Ptr(T&) -> Ptr<T>;
}
