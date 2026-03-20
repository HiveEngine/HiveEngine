#pragma once

#include <hive/core/assert.h>

#include <wax/containers/string_view.h>

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <type_traits>
#include <utility>

namespace wax
{
    // Stack-only fixed-capacity string (22 chars max, 24 bytes total).
    // No heap allocation, no allocator required. Truncates silently on overflow.
    class FixedString
    {
    public:
        using SizeType = size_t;
        using Iterator = char*;
        using ConstIterator = const char*;

        static constexpr size_t npos = static_cast<size_t>(-1);
        static constexpr size_t MaxCapacity = 22;

        constexpr FixedString() noexcept
            : buffer_{}
            , m_size{0}
        {
            buffer_[0] = '\0';
        }

        constexpr FixedString(const char* str) noexcept
            : buffer_{}
            , m_size{0}
        {
            if (str)
            {
                size_t len = StrLen(str);
                len = len <= MaxCapacity ? len : MaxCapacity;

                for (size_t i = 0; i < len; ++i)
                {
                    buffer_[i] = str[i];
                }
                buffer_[len] = '\0';
                m_size = static_cast<uint8_t>(len);
            }
            else
            {
                buffer_[0] = '\0';
            }
        }

        constexpr FixedString(StringView sv) noexcept
            : buffer_{}
            , m_size{0}
        {
            size_t len = sv.Size();
            len = len <= MaxCapacity ? len : MaxCapacity;

            for (size_t i = 0; i < len; ++i)
            {
                buffer_[i] = sv[i];
            }
            buffer_[len] = '\0';
            m_size = static_cast<uint8_t>(len);
        }

        constexpr FixedString(const char* data, size_t size) noexcept
            : buffer_{}
            , m_size{0}
        {
            size = size <= MaxCapacity ? size : MaxCapacity;

            for (size_t i = 0; i < size; ++i)
            {
                buffer_[i] = data[i];
            }
            buffer_[size] = '\0';
            m_size = static_cast<uint8_t>(size);
        }

        constexpr FixedString(const FixedString&) noexcept = default;
        constexpr FixedString& operator=(const FixedString&) noexcept = default;

        constexpr FixedString(FixedString&&) noexcept = default;
        constexpr FixedString& operator=(FixedString&&) noexcept = default;

        [[nodiscard]] constexpr char& operator[](size_t index) noexcept
        {
            hive::Assert(index < m_size, "FixedString index out of bounds");
            return buffer_[index];
        }

        [[nodiscard]] constexpr const char& operator[](size_t index) const noexcept
        {
            hive::Assert(index < m_size, "FixedString index out of bounds");
            return buffer_[index];
        }

        [[nodiscard]] constexpr char& At(size_t index)
        {
            hive::Check(index < m_size, "FixedString index out of bounds");
            return buffer_[index];
        }

        [[nodiscard]] constexpr const char& At(size_t index) const
        {
            hive::Check(index < m_size, "FixedString index out of bounds");
            return buffer_[index];
        }

        [[nodiscard]] constexpr char& Front() noexcept
        {
            hive::Assert(m_size > 0, "FixedString is empty");
            return buffer_[0];
        }

        [[nodiscard]] constexpr const char& Front() const noexcept
        {
            hive::Assert(m_size > 0, "FixedString is empty");
            return buffer_[0];
        }

        [[nodiscard]] constexpr char& Back() noexcept
        {
            hive::Assert(m_size > 0, "FixedString is empty");
            return buffer_[m_size - 1];
        }

        [[nodiscard]] constexpr const char& Back() const noexcept
        {
            hive::Assert(m_size > 0, "FixedString is empty");
            return buffer_[m_size - 1];
        }

        [[nodiscard]] constexpr char* Data() noexcept
        {
            return buffer_;
        }

        [[nodiscard]] constexpr const char* Data() const noexcept
        {
            return buffer_;
        }

        [[nodiscard]] constexpr const char* CStr() const noexcept
        {
            return buffer_;
        }

        [[nodiscard]] constexpr size_t Size() const noexcept
        {
            return m_size;
        }

        [[nodiscard]] constexpr size_t Length() const noexcept
        {
            return m_size;
        }

        [[nodiscard]] constexpr size_t Capacity() const noexcept
        {
            return MaxCapacity;
        }

        [[nodiscard]] constexpr bool IsEmpty() const noexcept
        {
            return m_size == 0;
        }

        [[nodiscard]] constexpr bool IsFull() const noexcept
        {
            return m_size == MaxCapacity;
        }

        [[nodiscard]] constexpr Iterator begin() noexcept
        {
            return buffer_;
        }

        [[nodiscard]] constexpr ConstIterator begin() const noexcept
        {
            return buffer_;
        }

        [[nodiscard]] constexpr Iterator end() noexcept
        {
            return buffer_ + m_size;
        }

        [[nodiscard]] constexpr ConstIterator end() const noexcept
        {
            return buffer_ + m_size;
        }

        [[nodiscard]] constexpr StringView View() const noexcept
        {
            return StringView{buffer_, m_size};
        }

        [[nodiscard]] constexpr operator StringView() const noexcept
        {
            return View();
        }

        constexpr void Clear() noexcept
        {
            m_size = 0;
            buffer_[0] = '\0';
        }

        constexpr void Append(char ch) noexcept
        {
            if (m_size < MaxCapacity)
            {
                buffer_[m_size] = ch;
                ++m_size;
                buffer_[m_size] = '\0';
            }
        }

        constexpr void Append(const char* str) noexcept
        {
            if (str == nullptr)
            {
                return;
            }

            size_t str_len = StrLen(str);
            Append(str, str_len);
        }

        constexpr void Append(const char* str, size_t count) noexcept
        {
            if (str == nullptr || count == 0)
            {
                return;
            }
            size_t actual_count = (m_size + count <= MaxCapacity) ? count : (MaxCapacity - m_size);

            for (size_t i = 0; i < actual_count; ++i)
            {
                buffer_[m_size + i] = str[i];
            }

            m_size += actual_count;
            buffer_[m_size] = '\0';
        }

        constexpr void Append(StringView sv) noexcept
        {
            Append(sv.Data(), sv.Size());
        }

        constexpr void PopBack() noexcept
        {
            hive::Assert(m_size > 0, "FixedString is empty");
            if (m_size > 0)
            {
                --m_size;
                buffer_[m_size] = '\0';
            }
        }

        constexpr void Resize(size_t new_size, char ch = '\0') noexcept
        {
            new_size = (new_size <= MaxCapacity) ? new_size : MaxCapacity;

            if (new_size > m_size)
            {
                for (size_t i = m_size; i < new_size; ++i)
                {
                    buffer_[i] = ch;
                }
            }

            m_size = static_cast<uint8_t>(new_size);
            buffer_[m_size] = '\0';
        }

        [[nodiscard]] constexpr size_t Find(char ch, size_t pos = 0) const noexcept
        {
            return View().Find(ch, pos);
        }

        [[nodiscard]] constexpr size_t Find(StringView sv, size_t pos = 0) const noexcept
        {
            return View().Find(sv, pos);
        }

        [[nodiscard]] constexpr size_t RFind(char ch, size_t pos = npos) const noexcept
        {
            return View().RFind(ch, pos);
        }

        [[nodiscard]] constexpr bool Contains(char ch) const noexcept
        {
            return View().Contains(ch);
        }

        [[nodiscard]] constexpr bool Contains(StringView sv) const noexcept
        {
            return View().Contains(sv);
        }

        [[nodiscard]] constexpr bool StartsWith(char ch) const noexcept
        {
            return View().StartsWith(ch);
        }

        [[nodiscard]] constexpr bool StartsWith(StringView sv) const noexcept
        {
            return View().StartsWith(sv);
        }

        [[nodiscard]] constexpr bool EndsWith(char ch) const noexcept
        {
            return View().EndsWith(ch);
        }

        [[nodiscard]] constexpr bool EndsWith(StringView sv) const noexcept
        {
            return View().EndsWith(sv);
        }

        [[nodiscard]] constexpr int Compare(const FixedString& other) const noexcept
        {
            return View().Compare(other.View());
        }

        [[nodiscard]] constexpr int Compare(StringView sv) const noexcept
        {
            return View().Compare(sv);
        }

        [[nodiscard]] constexpr bool Equals(const FixedString& other) const noexcept
        {
            return View().Equals(other.View());
        }

        [[nodiscard]] constexpr bool Equals(StringView sv) const noexcept
        {
            return View().Equals(sv);
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

        char buffer_[MaxCapacity + 1]; // +1 for null terminator
        uint8_t m_size;
    };

    [[nodiscard]] constexpr bool operator==(const FixedString& lhs, const FixedString& rhs) noexcept
    {
        return lhs.Equals(rhs);
    }

    [[nodiscard]] constexpr bool operator==(const FixedString& lhs, StringView rhs) noexcept
    {
        return lhs.Equals(rhs);
    }

    [[nodiscard]] constexpr bool operator==(StringView lhs, const FixedString& rhs) noexcept
    {
        return rhs.Equals(lhs);
    }

    [[nodiscard]] constexpr bool operator==(const FixedString& lhs, const char* rhs) noexcept
    {
        return lhs.Equals(StringView{rhs});
    }

    [[nodiscard]] constexpr bool operator==(const char* lhs, const FixedString& rhs) noexcept
    {
        return rhs.Equals(StringView{lhs});
    }

    [[nodiscard]] constexpr bool operator!=(const FixedString& lhs, const FixedString& rhs) noexcept
    {
        return !lhs.Equals(rhs);
    }

    [[nodiscard]] constexpr bool operator!=(const FixedString& lhs, StringView rhs) noexcept
    {
        return !lhs.Equals(rhs);
    }

    [[nodiscard]] constexpr bool operator!=(StringView lhs, const FixedString& rhs) noexcept
    {
        return !rhs.Equals(lhs);
    }

    [[nodiscard]] constexpr bool operator<(const FixedString& lhs, const FixedString& rhs) noexcept
    {
        return lhs.Compare(rhs) < 0;
    }

    [[nodiscard]] constexpr bool operator<(const FixedString& lhs, StringView rhs) noexcept
    {
        return lhs.Compare(rhs) < 0;
    }

    [[nodiscard]] constexpr bool operator<(StringView lhs, const FixedString& rhs) noexcept
    {
        return rhs.Compare(lhs) > 0;
    }

    [[nodiscard]] constexpr bool operator<=(const FixedString& lhs, const FixedString& rhs) noexcept
    {
        return lhs.Compare(rhs) <= 0;
    }

    [[nodiscard]] constexpr bool operator>(const FixedString& lhs, const FixedString& rhs) noexcept
    {
        return lhs.Compare(rhs) > 0;
    }

    [[nodiscard]] constexpr bool operator>=(const FixedString& lhs, const FixedString& rhs) noexcept
    {
        return lhs.Compare(rhs) >= 0;
    }
} // namespace wax
