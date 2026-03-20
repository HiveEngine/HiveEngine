#pragma once

#include <hive/core/assert.h>

#include <type_traits>

namespace wax
{
    // Non-owning reference wrapper that is never null.
    // Rebindable (unlike T&), stores in containers. Asserts non-null in debug.
    template <typename T> class Ref
    {
    public:
        using ValueType = T;

        constexpr Ref(T& value) noexcept
            : m_ptr{&value}
        {
            hive::Assert(m_ptr != nullptr, "Ref cannot be null");
        }

        constexpr explicit Ref(T* ptr) noexcept
            : m_ptr{ptr}
        {
            hive::Assert(m_ptr != nullptr, "Ref cannot be constructed from nullptr");
        }

        constexpr Ref(const Ref&) noexcept = default;
        constexpr Ref& operator=(const Ref&) noexcept = default;

        [[nodiscard]] constexpr T& operator*() const noexcept
        {
            return *m_ptr;
        }

        [[nodiscard]] constexpr T* operator->() const noexcept
        {
            return m_ptr;
        }

        [[nodiscard]] constexpr T* Get() const noexcept
        {
            return m_ptr;
        }

        [[nodiscard]] constexpr operator T&() const noexcept
        {
            return *m_ptr;
        }

        constexpr void Rebind(T& value) noexcept
        {
            m_ptr = &value;
            hive::Assert(m_ptr != nullptr, "Ref cannot be rebound to null");
        }

        constexpr void Rebind(T* ptr) noexcept
        {
            hive::Assert(ptr != nullptr, "Ref cannot be rebound to nullptr");
            m_ptr = ptr;
        }

        [[nodiscard]] constexpr bool operator==(const Ref& other) const noexcept
        {
            return m_ptr == other.m_ptr;
        }

        [[nodiscard]] constexpr bool operator!=(const Ref& other) const noexcept
        {
            return m_ptr != other.m_ptr;
        }

        [[nodiscard]] constexpr bool operator<(const Ref& other) const noexcept
        {
            return m_ptr < other.m_ptr;
        }

        [[nodiscard]] constexpr bool operator<=(const Ref& other) const noexcept
        {
            return m_ptr <= other.m_ptr;
        }

        [[nodiscard]] constexpr bool operator>(const Ref& other) const noexcept
        {
            return m_ptr > other.m_ptr;
        }

        [[nodiscard]] constexpr bool operator>=(const Ref& other) const noexcept
        {
            return m_ptr >= other.m_ptr;
        }

    private:
        T* m_ptr;
    };

    template <typename T> Ref(T&) -> Ref<T>;
} // namespace wax
