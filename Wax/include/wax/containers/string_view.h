#pragma once

#include <cstddef>
#include <cstring>
#include <hive/core/assert.h>

namespace wax
{
    /**
     * Non-owning view over a string
     *
     * StringView provides a lightweight reference to an existing string without
     * owning the data. It's essentially a const char* + size with bounds checking.
     * Immutable, zero-allocation, and perfect for passing strings to functions.
     *
     * Performance characteristics:
     * - Storage: 16 bytes (pointer + size) on 64-bit systems
     * - Access: O(1) - direct pointer arithmetic
     * - Construction: O(1) for pointer+size, O(n) for null-terminated strings
     * - Copy: O(1) - trivially copyable
     * - Comparison: O(n) worst case
     * - Find: O(n*m) naive search
     * - Bounds check: O(1) in debug, zero overhead in release
     *
     * Limitations:
     * - Non-owning (caller must ensure data stays alive)
     * - Immutable (read-only view)
     * - No null termination guarantee (unless constructed from C string)
     * - Dangling pointer risk (if source data is destroyed)
     *
     * Use cases:
     * - Function parameters (avoid copying strings)
     * - View into String, string literals, or C strings
     * - Substrings of existing data
     * - Parsing and tokenization
     *
     * Example:
     * @code
     *   void ProcessName(wax::StringView name) {
     *       if (name.StartsWith("Player")) {
     *           // ...
     *       }
     *   }
     *
     *   ProcessName("Player1");  // String literal
     *   wax::String str{alloc, "Enemy"};
     *   ProcessName(str);  // Implicit conversion from String
     *
     *   wax::StringView sub = name.Substr(0, 6);  // "Player"
     * @endcode
     */
    class StringView
    {
    public:
        using SizeType = size_t;
        using Iterator = const char*;
        using ConstIterator = const char*;

        static constexpr size_t npos = static_cast<size_t>(-1);

        constexpr StringView() noexcept
            : data_{nullptr}
            , size_{0}
        {}

        constexpr StringView(const char* data, size_t size) noexcept
            : data_{data}
            , size_{size}
        {}

        constexpr StringView(const char* str) noexcept
            : data_{str}
            , size_{str ? StrLen(str) : 0}
        {}

        template<size_t N>
        constexpr StringView(const char (&str)[N]) noexcept
            : data_{str}
            , size_{N - 1}
        {}

        constexpr StringView(const StringView&) noexcept = default;
        constexpr StringView& operator=(const StringView&) noexcept = default;

        [[nodiscard]] constexpr char operator[](size_t index) const noexcept
        {
            hive::Assert(index < size_, "StringView index out of bounds");
            return data_[index];
        }

        [[nodiscard]] constexpr char At(size_t index) const
        {
            hive::Check(index < size_, "StringView index out of bounds");
            return data_[index];
        }

        [[nodiscard]] constexpr char Front() const noexcept
        {
            hive::Assert(size_ > 0, "StringView is empty");
            return data_[0];
        }

        [[nodiscard]] constexpr char Back() const noexcept
        {
            hive::Assert(size_ > 0, "StringView is empty");
            return data_[size_ - 1];
        }

        [[nodiscard]] constexpr const char* Data() const noexcept
        {
            return data_;
        }

        [[nodiscard]] constexpr size_t Size() const noexcept
        {
            return size_;
        }

        [[nodiscard]] constexpr size_t Length() const noexcept
        {
            return size_;
        }

        [[nodiscard]] constexpr bool IsEmpty() const noexcept
        {
            return size_ == 0;
        }

        [[nodiscard]] constexpr ConstIterator begin() const noexcept
        {
            return data_;
        }

        [[nodiscard]] constexpr ConstIterator end() const noexcept
        {
            return data_ + size_;
        }

        [[nodiscard]] constexpr StringView Substr(size_t pos = 0, size_t count = npos) const noexcept
        {
            hive::Assert(pos <= size_, "Substr position out of bounds");
            size_t actual_count = (count == npos || pos + count > size_) ? size_ - pos : count;
            return StringView{data_ + pos, actual_count};
        }

        [[nodiscard]] constexpr StringView RemovePrefix(size_t n) const noexcept
        {
            hive::Assert(n <= size_, "RemovePrefix count exceeds size");
            return StringView{data_ + n, size_ - n};
        }

        [[nodiscard]] constexpr StringView RemoveSuffix(size_t n) const noexcept
        {
            hive::Assert(n <= size_, "RemoveSuffix count exceeds size");
            return StringView{data_, size_ - n};
        }

        [[nodiscard]] constexpr size_t Find(char ch, size_t pos = 0) const noexcept
        {
            if (pos >= size_)
            {
                return npos;
            }

            if (!std::is_constant_evaluated())
            {
                const void* result = std::memchr(data_ + pos, ch, size_ - pos);
                if (result)
                {
                    return static_cast<size_t>(static_cast<const char*>(result) - data_);
                }
                return npos;
            }

            for (size_t i = pos; i < size_; ++i)
            {
                if (data_[i] == ch)
                {
                    return i;
                }
            }

            return npos;
        }

        [[nodiscard]] constexpr size_t Find(StringView sv, size_t pos = 0) const noexcept
        {
            if (sv.size_ == 0)
            {
                return pos <= size_ ? pos : npos;
            }

            if (pos + sv.size_ > size_)
            {
                return npos;
            }

            for (size_t i = pos; i <= size_ - sv.size_; ++i)
            {
                bool match = true;
                for (size_t j = 0; j < sv.size_; ++j)
                {
                    if (data_[i + j] != sv.data_[j])
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
            if (size_ == 0)
            {
                return npos;
            }

            size_t start = (pos == npos || pos >= size_) ? size_ - 1 : pos;

            for (size_t i = start + 1; i > 0; --i)
            {
                if (data_[i - 1] == ch)
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
            return size_ > 0 && data_[0] == ch;
        }

        [[nodiscard]] constexpr bool StartsWith(StringView sv) const noexcept
        {
            if (sv.size_ > size_)
            {
                return false;
            }

            for (size_t i = 0; i < sv.size_; ++i)
            {
                if (data_[i] != sv.data_[i])
                {
                    return false;
                }
            }

            return true;
        }

        [[nodiscard]] constexpr bool EndsWith(char ch) const noexcept
        {
            return size_ > 0 && data_[size_ - 1] == ch;
        }

        [[nodiscard]] constexpr bool EndsWith(StringView sv) const noexcept
        {
            if (sv.size_ > size_)
            {
                return false;
            }

            size_t offset = size_ - sv.size_;
            for (size_t i = 0; i < sv.size_; ++i)
            {
                if (data_[offset + i] != sv.data_[i])
                {
                    return false;
                }
            }

            return true;
        }

        [[nodiscard]] constexpr int Compare(StringView other) const noexcept
        {
            size_t min_size = size_ < other.size_ ? size_ : other.size_;

            for (size_t i = 0; i < min_size; ++i)
            {
                if (data_[i] < other.data_[i])
                {
                    return -1;
                }
                if (data_[i] > other.data_[i])
                {
                    return 1;
                }
            }

            if (size_ < other.size_)
            {
                return -1;
            }
            if (size_ > other.size_)
            {
                return 1;
            }

            return 0;
        }

        [[nodiscard]] constexpr bool Equals(StringView other) const noexcept
        {
            if (size_ != other.size_)
            {
                return false;
            }

            for (size_t i = 0; i < size_; ++i)
            {
                if (data_[i] != other.data_[i])
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

        const char* data_;
        size_t size_;
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
}
