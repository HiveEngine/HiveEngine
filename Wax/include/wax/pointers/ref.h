#pragma once

#include <type_traits>
#include <hive/core/assert.h>

namespace wax
{
    /**
     * Non-owning reference wrapper that is never null
     *
     * Ref<T> is a rebindable, non-owning reference to an object that is guaranteed
     * to never be null. Unlike C++ references (T&), Ref can be reassigned and stored
     * in containers. Unlike raw pointers (T*), Ref enforces non-null invariant.
     *
     * Performance characteristics:
     * - Storage: 8 bytes (same as raw pointer on 64-bit)
     * - Access: O(1) - direct dereference (zero overhead)
     * - Copy: O(1) - trivially copyable
     * - Comparison: O(1) - pointer comparison
     * - Zero runtime overhead (no vtable, no checks in release)
     *
     * Limitations:
     * - Non-owning (does not manage lifetime)
     * - Caller must ensure referenced object stays alive
     * - Cannot be null (assertion in debug, undefined in release)
     * - Not thread-safe (no synchronization)
     *
     * Use cases:
     * - Function parameters (avoid null checks)
     * - Class members (rebindable reference)
     * - Containers of references
     * - Return non-null references from functions
     *
     * Example:
     * @code
     *   void ProcessEntity(wax::Ref<Entity> entity) {
     *       entity->Update();  // No null check needed!
     *   }
     *
     *   Entity e;
     *   wax::Ref<Entity> ref{e};
     *   ref->Health = 100;
     *
     *   // Rebindable (unlike T&)
     *   Entity e2;
     *   ref = e2;
     *
     *   // Storable in containers
     *   wax::Vector<wax::Ref<Entity>> entities{alloc};
     *   entities.PushBack(ref);
     * @endcode
     */
    template<typename T>
    class Ref
    {
    public:
        using ValueType = T;

        // Constructor from reference (never null)
        constexpr Ref(T& value) noexcept
            : ptr_{&value}
        {
            hive::Assert(ptr_ != nullptr, "Ref cannot be null");
        }

        // Constructor from pointer (checked in debug)
        constexpr explicit Ref(T* ptr) noexcept
            : ptr_{ptr}
        {
            hive::Assert(ptr_ != nullptr, "Ref cannot be constructed from nullptr");
        }

        // Copy constructor (trivial)
        constexpr Ref(const Ref&) noexcept = default;
        constexpr Ref& operator=(const Ref&) noexcept = default;

        // Dereference operators
        [[nodiscard]] constexpr T& operator*() const noexcept
        {
            return *ptr_;
        }

        [[nodiscard]] constexpr T* operator->() const noexcept
        {
            return ptr_;
        }

        // Get underlying pointer
        [[nodiscard]] constexpr T* Get() const noexcept
        {
            return ptr_;
        }

        // Conversion to reference
        [[nodiscard]] constexpr operator T&() const noexcept
        {
            return *ptr_;
        }

        // Rebind to a different object
        constexpr void Rebind(T& value) noexcept
        {
            ptr_ = &value;
            hive::Assert(ptr_ != nullptr, "Ref cannot be rebound to null");
        }

        constexpr void Rebind(T* ptr) noexcept
        {
            hive::Assert(ptr != nullptr, "Ref cannot be rebound to nullptr");
            ptr_ = ptr;
        }

        // Comparison operators (compare pointers)
        [[nodiscard]] constexpr bool operator==(const Ref& other) const noexcept
        {
            return ptr_ == other.ptr_;
        }

        [[nodiscard]] constexpr bool operator!=(const Ref& other) const noexcept
        {
            return ptr_ != other.ptr_;
        }

        [[nodiscard]] constexpr bool operator<(const Ref& other) const noexcept
        {
            return ptr_ < other.ptr_;
        }

        [[nodiscard]] constexpr bool operator<=(const Ref& other) const noexcept
        {
            return ptr_ <= other.ptr_;
        }

        [[nodiscard]] constexpr bool operator>(const Ref& other) const noexcept
        {
            return ptr_ > other.ptr_;
        }

        [[nodiscard]] constexpr bool operator>=(const Ref& other) const noexcept
        {
            return ptr_ >= other.ptr_;
        }

    private:
        T* ptr_;
    };

    // Deduction guide
    template<typename T>
    Ref(T&) -> Ref<T>;
}
