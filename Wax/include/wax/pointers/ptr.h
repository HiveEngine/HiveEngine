#pragma once

#include <hive/core/assert.h>

#include <type_traits>

namespace wax
{
    // Non-owning nullable pointer wrapper. Rebindable, can be null.
    template <typename T> class Ptr
    {
    public:
        using ValueType = T;

        constexpr Ptr() noexcept
            : m_ptr{nullptr}
        {
        }

        constexpr Ptr(std::nullptr_t) noexcept
            : m_ptr{nullptr}
        {
        }

        constexpr Ptr(T* ptr) noexcept
            : m_ptr{ptr}
        {
        }

        constexpr Ptr(T& value) noexcept
            : m_ptr{&value}
        {
        }

        constexpr Ptr(const Ptr&) noexcept = default;
        constexpr Ptr& operator=(const Ptr&) noexcept = default;

        constexpr Ptr& operator=(std::nullptr_t) noexcept
        {
            m_ptr = nullptr;
            return *this;
        }

        constexpr Ptr& operator=(T* ptr) noexcept
        {
            m_ptr = ptr;
            return *this;
        }

        [[nodiscard]] constexpr T& operator*() const noexcept
        {
            hive::Assert(m_ptr != nullptr, "Dereferencing null Ptr");
            return *m_ptr;
        }

        [[nodiscard]] constexpr T* operator->() const noexcept
        {
            hive::Assert(m_ptr != nullptr, "Dereferencing null Ptr");
            return m_ptr;
        }

        [[nodiscard]] constexpr T* Get() const noexcept
        {
            return m_ptr;
        }

        [[nodiscard]] constexpr explicit operator bool() const noexcept
        {
            return m_ptr != nullptr;
        }

        [[nodiscard]] constexpr bool IsNull() const noexcept
        {
            return m_ptr == nullptr;
        }

        [[nodiscard]] constexpr bool IsValid() const noexcept
        {
            return m_ptr != nullptr;
        }

        constexpr void Reset() noexcept
        {
            m_ptr = nullptr;
        }

        constexpr void Rebind(T* ptr) noexcept
        {
            m_ptr = ptr;
        }

        constexpr void Rebind(T& value) noexcept
        {
            m_ptr = &value;
        }

        [[nodiscard]] constexpr bool operator==(const Ptr& other) const noexcept
        {
            return m_ptr == other.m_ptr;
        }

        [[nodiscard]] constexpr bool operator!=(const Ptr& other) const noexcept
        {
            return m_ptr != other.m_ptr;
        }

        [[nodiscard]] constexpr bool operator==(std::nullptr_t) const noexcept
        {
            return m_ptr == nullptr;
        }

        [[nodiscard]] constexpr bool operator!=(std::nullptr_t) const noexcept
        {
            return m_ptr != nullptr;
        }

        [[nodiscard]] constexpr bool operator<(const Ptr& other) const noexcept
        {
            return m_ptr < other.m_ptr;
        }

        [[nodiscard]] constexpr bool operator<=(const Ptr& other) const noexcept
        {
            return m_ptr <= other.m_ptr;
        }

        [[nodiscard]] constexpr bool operator>(const Ptr& other) const noexcept
        {
            return m_ptr > other.m_ptr;
        }

        [[nodiscard]] constexpr bool operator>=(const Ptr& other) const noexcept
        {
            return m_ptr >= other.m_ptr;
        }

    private:
        T* m_ptr;
    };

    template <typename T> [[nodiscard]] constexpr bool operator==(std::nullptr_t, const Ptr<T>& ptr) noexcept
    {
        return ptr == nullptr;
    }

    template <typename T> [[nodiscard]] constexpr bool operator!=(std::nullptr_t, const Ptr<T>& ptr) noexcept
    {
        return ptr != nullptr;
    }

    template <typename T> Ptr(T*) -> Ptr<T>;

    template <typename T> Ptr(T&) -> Ptr<T>;
} // namespace wax
