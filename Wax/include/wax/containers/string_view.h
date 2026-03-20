#pragma once

#include <hive/core/assert.h>

#include <cstddef>
#include <cstring>
#include <type_traits>

namespace wax
{
    // Non-owning, immutable view over a string (const char* + size).
    // No null-termination guarantee unless constructed from a C string.
    class StringView
    {
    public:
        using SizeType = size_t;
        using Iterator = const char*;
        using ConstIterator = const char*;

        static constexpr size_t npos = static_cast<size_t>(-1);

        constexpr StringView() noexcept
            : m_data{nullptr}
            , m_size{0}
        {
        }

        constexpr StringView(const char* data, size_t size) noexcept
            : m_data{data}
            , m_size{size}
        {
        }

        constexpr StringView(const char* str) noexcept
            : m_data{str}
            , m_size{str ? StrLen(str) : 0}
        {
        }

        template <size_t N>
        constexpr StringView(const char (&str)[N]) noexcept
            : m_data{str}
            , m_size{N - 1}
        {
        }

        constexpr StringView(const StringView&) noexcept = default;
        constexpr StringView& operator=(const StringView&) noexcept = default;

        [[nodiscard]] constexpr char operator[](size_t index) const noexcept
        {
            hive::Assert(index < m_size, "StringView index out of bounds");
            return m_data[index];
        }

        [[nodiscard]] constexpr char At(size_t index) const
        {
            hive::Check(index < m_size, "StringView index out of bounds");
            return m_data[index];
        }

        [[nodiscard]] constexpr char Front() const noexcept
        {
            hive::Assert(m_size > 0, "StringView is empty");
            return m_data[0];
        }

        [[nodiscard]] constexpr char Back() const noexcept
        {
            hive::Assert(m_size > 0, "StringView is empty");
            return m_data[m_size - 1];
        }

        [[nodiscard]] constexpr const char* Data() const noexcept
        {
            return m_data;
        }

        [[nodiscard]] constexpr size_t Size() const noexcept
        {
            return m_size;
        }

        [[nodiscard]] constexpr size_t Length() const noexcept
        {
            return m_size;
        }

        [[nodiscard]] constexpr bool IsEmpty() const noexcept
        {
            return m_size == 0;
        }

        [[nodiscard]] constexpr ConstIterator Begin() const noexcept
        {
            return m_data;
        }

        [[nodiscard]] constexpr ConstIterator End() const noexcept
        {
            return m_data + m_size;
        }

        [[nodiscard]] constexpr ConstIterator begin() const noexcept
        {
            return Begin();
        }

        [[nodiscard]] constexpr ConstIterator cbegin() const noexcept
        {
            return Begin();
        }

        [[nodiscard]] constexpr ConstIterator end() const noexcept
        {
            return End();
        }

        [[nodiscard]] constexpr ConstIterator cend() const noexcept
        {
            return End();
        }

        [[nodiscard]] constexpr StringView Substr(size_t pos = 0, size_t count = npos) const noexcept
        {
            hive::Assert(pos <= m_size, "Substr position out of bounds");
            size_t actualCount = (count == npos || pos + count > m_size) ? m_size - pos : count;
            return StringView{m_data + pos, actualCount};
        }

        [[nodiscard]] constexpr StringView RemovePrefix(size_t n) const noexcept
        {
            hive::Assert(n <= m_size, "RemovePrefix count exceeds size");
            return StringView{m_data + n, m_size - n};
        }

        [[nodiscard]] constexpr StringView RemoveSuffix(size_t n) const noexcept
        {
            hive::Assert(n <= m_size, "RemoveSuffix count exceeds size");
            return StringView{m_data, m_size - n};
        }

        [[nodiscard]] constexpr size_t Find(char ch, size_t pos = 0) const noexcept
        {
            if (pos >= m_size)
            {
                return npos;
            }

            if (!std::is_constant_evaluated())
            {
                const void* result = std::memchr(m_data + pos, ch, m_size - pos);
                if (result)
                {
                    return static_cast<size_t>(static_cast<const char*>(result) - m_data);
                }
                return npos;
            }

            for (size_t i = pos; i < m_size; ++i)
            {
                if (m_data[i] == ch)
                {
                    return i;
                }
            }

            return npos;
        }

        [[nodiscard]] constexpr size_t Find(StringView sv, size_t pos = 0) const noexcept
        {
            if (sv.m_size == 0)
            {
                return pos <= m_size ? pos : npos;
            }

            if (pos + sv.m_size > m_size)
            {
                return npos;
            }

            for (size_t i = pos; i <= m_size - sv.m_size; ++i)
            {
                bool match = true;
                for (size_t j = 0; j < sv.m_size; ++j)
                {
                    if (m_data[i + j] != sv.m_data[j])
                    {
                        match = false;
                        break;
                    }
                }
                if (match)
                {
                    return i;
                }
            }

            return npos;
        }

        [[nodiscard]] constexpr size_t RFind(char ch, size_t pos = npos) const noexcept
        {
            if (m_size == 0)
            {
                return npos;
            }

            size_t start = (pos == npos || pos >= m_size) ? m_size - 1 : pos;

            for (size_t i = start + 1; i > 0; --i)
            {
                if (m_data[i - 1] == ch)
                {
                    return i - 1;
                }
            }

            return npos;
        }

        [[nodiscard]] constexpr bool Contains(char ch) const noexcept
        {
            return Find(ch) != npos;
        }

        [[nodiscard]] constexpr bool Contains(StringView sv) const noexcept
        {
            return Find(sv) != npos;
        }

        [[nodiscard]] constexpr bool StartsWith(char ch) const noexcept
        {
            return m_size > 0 && m_data[0] == ch;
        }

        [[nodiscard]] constexpr bool StartsWith(StringView sv) const noexcept
        {
            if (sv.m_size > m_size)
            {
                return false;
            }

            for (size_t i = 0; i < sv.m_size; ++i)
            {
                if (m_data[i] != sv.m_data[i])
                {
                    return false;
                }
            }

            return true;
        }

        [[nodiscard]] constexpr bool EndsWith(char ch) const noexcept
        {
            return m_size > 0 && m_data[m_size - 1] == ch;
        }

        [[nodiscard]] constexpr bool EndsWith(StringView sv) const noexcept
        {
            if (sv.m_size > m_size)
            {
                return false;
            }

            size_t offset = m_size - sv.m_size;
            for (size_t i = 0; i < sv.m_size; ++i)
            {
                if (m_data[offset + i] != sv.m_data[i])
                {
                    return false;
                }
            }

            return true;
        }

        [[nodiscard]] constexpr int Compare(StringView other) const noexcept
        {
            size_t minSize = m_size < other.m_size ? m_size : other.m_size;

            for (size_t i = 0; i < minSize; ++i)
            {
                if (m_data[i] < other.m_data[i])
                {
                    return -1;
                }
                if (m_data[i] > other.m_data[i])
                {
                    return 1;
                }
            }

            if (m_size < other.m_size)
            {
                return -1;
            }
            if (m_size > other.m_size)
            {
                return 1;
            }

            return 0;
        }

        [[nodiscard]] constexpr bool Equals(StringView other) const noexcept
        {
            if (m_size != other.m_size)
            {
                return false;
            }

            for (size_t i = 0; i < m_size; ++i)
            {
                if (m_data[i] != other.m_data[i])
                {
                    return false;
                }
            }

            return true;
        }

    private:
        static constexpr size_t StrLen(const char* str) noexcept
        {
            if (str == nullptr)
            {
                return 0;
            }

            size_t len = 0;
            while (str[len] != '\0')
            {
                ++len;
            }
            return len;
        }

        const char* m_data;
        size_t m_size;
    };

    [[nodiscard]] constexpr bool operator==(StringView lhs, StringView rhs) noexcept
    {
        return lhs.Equals(rhs);
    }

    [[nodiscard]] constexpr bool operator!=(StringView lhs, StringView rhs) noexcept
    {
        return !lhs.Equals(rhs);
    }

    [[nodiscard]] constexpr bool operator<(StringView lhs, StringView rhs) noexcept
    {
        return lhs.Compare(rhs) < 0;
    }

    [[nodiscard]] constexpr bool operator<=(StringView lhs, StringView rhs) noexcept
    {
        return lhs.Compare(rhs) <= 0;
    }

    [[nodiscard]] constexpr bool operator>(StringView lhs, StringView rhs) noexcept
    {
        return lhs.Compare(rhs) > 0;
    }

    [[nodiscard]] constexpr bool operator>=(StringView lhs, StringView rhs) noexcept
    {
        return lhs.Compare(rhs) >= 0;
    }
} // namespace wax
