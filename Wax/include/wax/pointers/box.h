#pragma once

#include <hive/core/assert.h>

#include <comb/default_allocator.h>
#include <comb/new.h>

#include <type_traits>
#include <utility>

namespace wax
{
    // Unique-ownership smart pointer with explicit allocator control.
    // Allocator must outlive the Box. Not copyable, move-only.
    template <typename T, comb::Allocator Allocator = comb::DefaultAllocator> class Box
    {
    public:
        using ValueType = T;
        using AllocatorType = Allocator;

        constexpr Box() noexcept
            : m_ptr{nullptr}
            , m_allocator{nullptr}
        {
        }

        constexpr Box(Allocator& allocator, T* ptr) noexcept
            : m_ptr{ptr}
            , m_allocator{&allocator}
        {
        }

        Box(const Box&) = delete;
        Box& operator=(const Box&) = delete;

        constexpr Box(Box&& other) noexcept
            : m_ptr{other.m_ptr}
            , m_allocator{other.m_allocator}
        {
            other.m_ptr = nullptr;
            other.m_allocator = nullptr;
        }

        constexpr Box& operator=(Box&& other) noexcept
        {
            if (this != &other)
            {
                Reset();

                m_ptr = other.m_ptr;
                m_allocator = other.m_allocator;

                other.m_ptr = nullptr;
                other.m_allocator = nullptr;
            }
            return *this;
        }

        ~Box() noexcept
        {
            Reset();
        }

        [[nodiscard]] constexpr T& operator*() const noexcept
        {
            hive::Assert(m_ptr != nullptr, "Dereferencing null Box");
            return *m_ptr;
        }

        [[nodiscard]] constexpr T* operator->() const noexcept
        {
            hive::Assert(m_ptr != nullptr, "Dereferencing null Box");
            return m_ptr;
        }

        [[nodiscard]] constexpr T* Get() const noexcept
        {
            return m_ptr;
        }

        [[nodiscard]] constexpr Allocator* GetAllocator() const noexcept
        {
            return m_allocator;
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

        [[nodiscard]] constexpr T* Release() noexcept
        {
            T* temp = m_ptr;
            m_ptr = nullptr;
            m_allocator = nullptr;
            return temp;
        }

        constexpr void Reset() noexcept
        {
            if (m_ptr && m_allocator)
            {
                comb::Delete(*m_allocator, m_ptr);
            }
            m_ptr = nullptr;
            m_allocator = nullptr;
        }

        constexpr void Reset(Allocator& allocator, T* ptr) noexcept
        {
            if (m_ptr && m_allocator)
            {
                comb::Delete(*m_allocator, m_ptr);
            }
            m_ptr = ptr;
            m_allocator = &allocator;
        }

        [[nodiscard]] constexpr bool operator==(const Box& other) const noexcept
        {
            return m_ptr == other.m_ptr;
        }

        [[nodiscard]] constexpr bool operator==(std::nullptr_t) const noexcept
        {
            return m_ptr == nullptr;
        }

    private:
        T* m_ptr;
        Allocator* m_allocator;
    };

    template <typename T, comb::Allocator Allocator, typename... Args>
    [[nodiscard]] Box<T, Allocator> MakeBox(Allocator& allocator, Args&&... args)
    {
        T* ptr = comb::New<T>(allocator, std::forward<Args>(args)...);
        return Box<T, Allocator>{allocator, ptr};
    }

    // Default allocator overload
    template <typename T, typename... Args> [[nodiscard]] Box<T, comb::DefaultAllocator> MakeBox(Args&&... args)
    {
        auto& allocator = comb::GetDefaultAllocator();
        T* ptr = comb::New<T>(allocator, std::forward<Args>(args)...);
        return Box<T, comb::DefaultAllocator>{allocator, ptr};
    }
} // namespace wax
