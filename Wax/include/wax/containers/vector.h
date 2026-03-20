#pragma once

#include <hive/core/assert.h>

#include <comb/memory_resource.h>

#include <cstddef>
#include <cstring>
#include <initializer_list>
#include <type_traits>
#include <utility>

namespace wax
{
    template <typename T> class Vector
    {
    public:
        using ValueType = T;
        using SizeType = size_t;
        using Iterator = T*;
        using ConstIterator = const T*;

        Vector() noexcept
            : m_data{nullptr}
            , m_size{0}
            , m_capacity{0}
            , m_allocator{comb::GetDefaultMemoryResource()}
        {
        }

        explicit Vector(size_t initialCapacity)
            : m_data{nullptr}
            , m_size{0}
            , m_capacity{0}
            , m_allocator{comb::GetDefaultMemoryResource()}
        {
            Reserve(initialCapacity);
        }

        explicit Vector(comb::MemoryResource allocator) noexcept
            : m_data{nullptr}
            , m_size{0}
            , m_capacity{0}
            , m_allocator{allocator}
        {
        }

        template <comb::Allocator Allocator>
        explicit Vector(Allocator& allocator) noexcept
            : m_data{nullptr}
            , m_size{0}
            , m_capacity{0}
            , m_allocator{comb::MemoryResource{allocator}}
        {
        }

        Vector(comb::MemoryResource allocator, size_t initialCapacity)
            : m_data{nullptr}
            , m_size{0}
            , m_capacity{0}
            , m_allocator{allocator}
        {
            Reserve(initialCapacity);
        }

        template <comb::Allocator Allocator>
        Vector(Allocator& allocator, size_t initialCapacity)
            : m_data{nullptr}
            , m_size{0}
            , m_capacity{0}
            , m_allocator{comb::MemoryResource{allocator}}
        {
            Reserve(initialCapacity);
        }

        Vector(std::initializer_list<T> init)
            : m_data{nullptr}
            , m_size{0}
            , m_capacity{0}
            , m_allocator{comb::GetDefaultMemoryResource()}
        {
            Reserve(init.size());
            for (const auto& value : init)
            {
                new (&m_data[m_size]) T(value);
                ++m_size;
            }
        }

        Vector(comb::MemoryResource allocator, std::initializer_list<T> init)
            : m_data{nullptr}
            , m_size{0}
            , m_capacity{0}
            , m_allocator{allocator}
        {
            Reserve(init.size());
            for (const auto& value : init)
            {
                new (&m_data[m_size]) T(value);
                ++m_size;
            }
        }

        template <comb::Allocator Allocator>
        Vector(Allocator& allocator, std::initializer_list<T> init)
            : m_data{nullptr}
            , m_size{0}
            , m_capacity{0}
            , m_allocator{comb::MemoryResource{allocator}}
        {
            Reserve(init.size());
            for (const auto& value : init)
            {
                new (&m_data[m_size]) T(value);
                ++m_size;
            }
        }

        ~Vector() noexcept
        {
            Clear();
            if (m_data)
            {
                m_allocator.Deallocate(m_data);
                m_data = nullptr;
                m_capacity = 0;
            }
        }

        Vector(const Vector& other)
            : m_data{nullptr}
            , m_size{0}
            , m_capacity{0}
            , m_allocator{other.m_allocator}
        {
            if (other.m_size > 0)
            {
                Reserve(other.m_size);
                if constexpr (std::is_trivially_copyable_v<T>)
                {
                    std::memcpy(m_data, other.m_data, other.m_size * sizeof(T));
                }
                else
                {
                    for (size_t i = 0; i < other.m_size; ++i)
                    {
                        new (&m_data[i]) T(other.m_data[i]);
                    }
                }
                m_size = other.m_size;
            }
        }

        Vector& operator=(const Vector& other)
        {
            if (this != &other)
            {
                Clear();
                if (m_data)
                {
                    m_allocator.Deallocate(m_data);
                    m_data = nullptr;
                    m_capacity = 0;
                }

                m_allocator = other.m_allocator;
                if (other.m_size > 0)
                {
                    Reserve(other.m_size);
                    if constexpr (std::is_trivially_copyable_v<T>)
                    {
                        std::memcpy(m_data, other.m_data, other.m_size * sizeof(T));
                    }
                    else
                    {
                        for (size_t i = 0; i < other.m_size; ++i)
                        {
                            new (&m_data[i]) T(other.m_data[i]);
                        }
                    }
                    m_size = other.m_size;
                }
            }
            return *this;
        }

        Vector(Vector&& other) noexcept
            : m_data{other.m_data}
            , m_size{other.m_size}
            , m_capacity{other.m_capacity}
            , m_allocator{other.m_allocator}
        {
            other.m_data = nullptr;
            other.m_size = 0;
            other.m_capacity = 0;
        }

        Vector& operator=(Vector&& other) noexcept
        {
            if (this != &other)
            {
                Clear();
                if (m_data)
                {
                    m_allocator.Deallocate(m_data);
                }

                m_data = other.m_data;
                m_size = other.m_size;
                m_capacity = other.m_capacity;
                m_allocator = other.m_allocator;

                other.m_data = nullptr;
                other.m_size = 0;
                other.m_capacity = 0;
            }
            return *this;
        }

        [[nodiscard]] T& operator[](size_t index) noexcept
        {
            hive::Assert(index < m_size, "Vector index out of bounds");
            return m_data[index];
        }

        [[nodiscard]] const T& operator[](size_t index) const noexcept
        {
            hive::Assert(index < m_size, "Vector index out of bounds");
            return m_data[index];
        }

        [[nodiscard]] T& At(size_t index)
        {
            hive::Check(index < m_size, "Vector index out of bounds");
            return m_data[index];
        }

        [[nodiscard]] const T& At(size_t index) const
        {
            hive::Check(index < m_size, "Vector index out of bounds");
            return m_data[index];
        }

        [[nodiscard]] T& Front() noexcept
        {
            hive::Assert(m_size > 0, "Vector is empty");
            return m_data[0];
        }

        [[nodiscard]] const T& Front() const noexcept
        {
            hive::Assert(m_size > 0, "Vector is empty");
            return m_data[0];
        }

        [[nodiscard]] T& Back() noexcept
        {
            hive::Assert(m_size > 0, "Vector is empty");
            return m_data[m_size - 1];
        }

        [[nodiscard]] const T& Back() const noexcept
        {
            hive::Assert(m_size > 0, "Vector is empty");
            return m_data[m_size - 1];
        }

        [[nodiscard]] T* Data() noexcept
        {
            return m_data;
        }

        [[nodiscard]] const T* Data() const noexcept
        {
            return m_data;
        }

        [[nodiscard]] size_t Size() const noexcept
        {
            return m_size;
        }

        [[nodiscard]] size_t Capacity() const noexcept
        {
            return m_capacity;
        }

        [[nodiscard]] bool IsEmpty() const noexcept
        {
            return m_size == 0;
        }

        [[nodiscard]] comb::MemoryResource GetAllocator() const noexcept
        {
            return m_allocator;
        }

        [[nodiscard]] Iterator Begin() noexcept
        {
            return m_data;
        }

        [[nodiscard]] ConstIterator Begin() const noexcept
        {
            return m_data;
        }

        [[nodiscard]] Iterator End() noexcept
        {
            return m_data + m_size;
        }

        [[nodiscard]] ConstIterator End() const noexcept
        {
            return m_data + m_size;
        }

        [[nodiscard]] Iterator begin() noexcept
        {
            return Begin();
        }

        [[nodiscard]] ConstIterator begin() const noexcept
        {
            return Begin();
        }

        [[nodiscard]] ConstIterator cbegin() const noexcept
        {
            return Begin();
        }

        [[nodiscard]] Iterator end() noexcept
        {
            return End();
        }

        [[nodiscard]] ConstIterator end() const noexcept
        {
            return End();
        }

        [[nodiscard]] ConstIterator cend() const noexcept
        {
            return End();
        }

        void Reserve(size_t newCapacity)
        {
            if (newCapacity <= m_capacity)
            {
                return;
            }

            hive::Check(newCapacity <= SIZE_MAX / sizeof(T), "Vector capacity overflow");

            T* newData = static_cast<T*>(m_allocator.Allocate(newCapacity * sizeof(T), alignof(T)));
            hive::Check(newData != nullptr, "Vector allocation failed");

            if (m_data)
            {
                if constexpr (std::is_trivially_copyable_v<T>)
                {
                    std::memcpy(newData, m_data, m_size * sizeof(T));
                }
                else
                {
                    for (size_t i = 0; i < m_size; ++i)
                    {
                        new (&newData[i]) T(std::move(m_data[i]));
                        m_data[i].~T();
                    }
                }

                m_allocator.Deallocate(m_data);
            }

            m_data = newData;
            m_capacity = newCapacity;
        }

        void ShrinkToFit()
        {
            if (m_size == m_capacity)
            {
                return;
            }

            if (m_size == 0)
            {
                if (m_data)
                {
                    m_allocator.Deallocate(m_data);
                    m_data = nullptr;
                }
                m_capacity = 0;
                return;
            }

            T* newData = static_cast<T*>(m_allocator.Allocate(m_size * sizeof(T), alignof(T)));
            hive::Check(newData != nullptr, "Vector allocation failed");

            if constexpr (std::is_trivially_copyable_v<T>)
            {
                std::memcpy(newData, m_data, m_size * sizeof(T));
            }
            else
            {
                for (size_t i = 0; i < m_size; ++i)
                {
                    new (&newData[i]) T(std::move(m_data[i]));
                    m_data[i].~T();
                }
            }

            m_allocator.Deallocate(m_data);
            m_data = newData;
            m_capacity = m_size;
        }

        void Clear() noexcept
        {
            if constexpr (!std::is_trivially_destructible_v<T>)
            {
                for (size_t i = 0; i < m_size; ++i)
                {
                    m_data[i].~T();
                }
            }
            m_size = 0;
        }

        void PushBack(const T& value)
        {
            if (m_size == m_capacity)
            {
                // value may reference an element in this vector; copy before realloc
                T valueCopy{value};
                const size_t newCapacity = m_capacity == 0 ? 8 : m_capacity * 2;
                Reserve(newCapacity);
                new (&m_data[m_size]) T(std::move(valueCopy));
                ++m_size;
                return;
            }

            new (&m_data[m_size]) T(value);
            ++m_size;
        }

        void PushBack(T&& value)
        {
            if (m_size == m_capacity)
            {
                T valueCopy{std::move(value)};
                const size_t newCapacity = m_capacity == 0 ? 8 : m_capacity * 2;
                Reserve(newCapacity);
                new (&m_data[m_size]) T(std::move(valueCopy));
                ++m_size;
                return;
            }

            new (&m_data[m_size]) T(std::move(value));
            ++m_size;
        }

        template <typename... Args> T& EmplaceBack(Args&&... args)
        {
            if (m_size == m_capacity)
            {
                const size_t newCapacity = m_capacity == 0 ? 8 : m_capacity * 2;
                Reserve(newCapacity);
            }

            new (&m_data[m_size]) T(std::forward<Args>(args)...);
            ++m_size;
            return m_data[m_size - 1];
        }

        void PopBack() noexcept
        {
            hive::Assert(m_size > 0, "Vector is empty");
            --m_size;
            if constexpr (!std::is_trivially_destructible_v<T>)
            {
                m_data[m_size].~T();
            }
        }

        Iterator Erase(Iterator pos)
        {
            hive::Assert(pos >= Begin() && pos < End(), "Iterator out of range");
            size_t index = static_cast<size_t>(pos - Begin());

            if constexpr (std::is_trivially_copyable_v<T>)
            {
                std::memmove(&m_data[index], &m_data[index + 1], (m_size - index - 1) * sizeof(T));
            }
            else
            {
                for (size_t i = index; i + 1 < m_size; ++i)
                {
                    m_data[i] = std::move(m_data[i + 1]);
                }
                m_data[m_size - 1].~T();
            }

            --m_size;
            return pos;
        }

        Iterator EraseSwapBack(Iterator pos)
        {
            hive::Assert(pos >= Begin() && pos < End(), "Iterator out of range");
            size_t index = static_cast<size_t>(pos - Begin());

            if (index != m_size - 1)
            {
                m_data[index] = std::move(m_data[m_size - 1]);
            }

            PopBack();
            return pos;
        }

        void Resize(size_t newSize)
        {
            if (newSize > m_capacity)
            {
                Reserve(newSize);
            }

            if (newSize > m_size)
            {
                for (size_t i = m_size; i < newSize; ++i)
                {
                    new (&m_data[i]) T();
                }
            }
            else if (newSize < m_size)
            {
                if constexpr (!std::is_trivially_destructible_v<T>)
                {
                    for (size_t i = newSize; i < m_size; ++i)
                    {
                        m_data[i].~T();
                    }
                }
            }

            m_size = newSize;
        }

        void Resize(size_t newSize, const T& value)
        {
            if (newSize > m_capacity)
            {
                Reserve(newSize);
            }

            if (newSize > m_size)
            {
                for (size_t i = m_size; i < newSize; ++i)
                {
                    new (&m_data[i]) T(value);
                }
            }
            else if (newSize < m_size)
            {
                if constexpr (!std::is_trivially_destructible_v<T>)
                {
                    for (size_t i = newSize; i < m_size; ++i)
                    {
                        m_data[i].~T();
                    }
                }
            }

            m_size = newSize;
        }

    private:
        T* m_data;
        size_t m_size;
        size_t m_capacity;
        comb::MemoryResource m_allocator;
    };
} // namespace wax
