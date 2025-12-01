#pragma once

#include <cstddef>
#include <cstring>
#include <cstdint>
#include <utility>
#include <type_traits>
#include <hive/core/assert.h>
#include <wax/containers/string_view.h>

namespace wax
{
    /**
     * Fixed-size string with Small String Optimization (SSO) only - no heap allocations
     *
     * FixedString is a stack-only string with a fixed maximum capacity (22 characters).
     * Unlike String<Allocator>, it NEVER allocates on the heap and requires no allocator.
     * Perfect for small, known-bounded strings (entity names, tags, short paths, etc.).
     *
     * Performance characteristics:
     * - Storage: 24 bytes (inline buffer + size + padding)
     * - Max capacity: 22 characters (fixed, cannot grow)
     * - Access: O(1) - direct pointer arithmetic
     * - Append: O(1) if within capacity, truncates silently otherwise
     * - Find: O(n*m) naive search
     * - Zero heap allocations (100% stack-based)
     *
     * Memory layout:
     * ┌───────────────────────────────┐
     * │ char buffer[23]               │  Inline storage (22 chars + null terminator)
     * │ uint8_t size                  │  Current size (0-22)
     * └───────────────────────────────┘
     *   24 bytes total
     *
     * Limitations:
     * - Fixed capacity (22 chars max, cannot grow)
     * - Append/Insert beyond capacity truncates silently (no error)
     * - No allocator required (pro and con)
     * - UTF-8 data treated as raw bytes (no encoding awareness)
     *
     * Use cases:
     * - Entity names, tags, IDs (short identifiers)
     * - Debug labels, log prefixes
     * - Short file extensions, enum strings
     * - Embedded systems (no dynamic allocation)
     * - Performance-critical code (zero allocations)
     *
     * Example:
     * @code
     *   // No allocator needed!
     *   wax::FixedString name{"Player"};
     *   name.Append("1");  // "Player1" - still fits
     *
     *   wax::FixedString tag{"Enemy"};
     *   tag.Append("_Tank");  // "Enemy_Tank"
     *
     *   // Comparison
     *   if (name == "Player1") {
     *       // ...
     *   }
     *
     *   // Convert to StringView
     *   wax::StringView view = name.View();
     *
     *   // Null-terminated C string
     *   const char* c_str = name.CStr();
     * @endcode
     */
    class FixedString
    {
    public:
        using SizeType = size_t;
        using Iterator = char*;
        using ConstIterator = const char*;

        static constexpr size_t npos = static_cast<size_t>(-1);
        static constexpr size_t MaxCapacity = 22;  // Max characters (buffer is MaxCapacity + 1 for null terminator)

        // Default constructor (empty string)
        constexpr FixedString() noexcept
            : buffer_{}
            , size_{0}
        {
            buffer_[0] = '\0';
        }

        // Constructor from C string (truncates if exceeds MaxCapacity)
        constexpr FixedString(const char* str) noexcept
            : buffer_{}
            , size_{0}
        {
            if (str)
            {
                size_t len = StrLen(str);
                // Silently truncate to MaxCapacity (consistent with Append behavior)
                len = len <= MaxCapacity ? len : MaxCapacity;

                for (size_t i = 0; i < len; ++i)
                {
                    buffer_[i] = str[i];
                }
                buffer_[len] = '\0';
                size_ = static_cast<uint8_t>(len);
            }
            else
            {
                buffer_[0] = '\0';
            }
        }

        // Constructor from StringView (truncates if exceeds MaxCapacity)
        constexpr FixedString(StringView sv) noexcept
            : buffer_{}
            , size_{0}
        {
            size_t len = sv.Size();
            // Silently truncate to MaxCapacity (consistent with Append behavior)
            len = len <= MaxCapacity ? len : MaxCapacity;

            for (size_t i = 0; i < len; ++i)
            {
                buffer_[i] = sv[i];
            }
            buffer_[len] = '\0';
            size_ = static_cast<uint8_t>(len);
        }

        // Constructor from pointer and size (truncates if exceeds MaxCapacity)
        constexpr FixedString(const char* data, size_t size) noexcept
            : buffer_{}
            , size_{0}
        {
            // Silently truncate to MaxCapacity (consistent with Append behavior)
            size = size <= MaxCapacity ? size : MaxCapacity;

            for (size_t i = 0; i < size; ++i)
            {
                buffer_[i] = data[i];
            }
            buffer_[size] = '\0';
            size_ = static_cast<uint8_t>(size);
        }

        // Copy constructor (trivial)
        constexpr FixedString(const FixedString&) noexcept = default;
        constexpr FixedString& operator=(const FixedString&) noexcept = default;

        // Move constructor (trivial)
        constexpr FixedString(FixedString&&) noexcept = default;
        constexpr FixedString& operator=(FixedString&&) noexcept = default;

        // Element access (bounds-checked in debug)
        [[nodiscard]] constexpr char& operator[](size_t index) noexcept
        {
            hive::Assert(index < size_, "FixedString index out of bounds");
            return buffer_[index];
        }

        [[nodiscard]] constexpr const char& operator[](size_t index) const noexcept
        {
            hive::Assert(index < size_, "FixedString index out of bounds");
            return buffer_[index];
        }

        // Element access (always bounds-checked)
        [[nodiscard]] constexpr char& At(size_t index)
        {
            hive::Check(index < size_, "FixedString index out of bounds");
            return buffer_[index];
        }

        [[nodiscard]] constexpr const char& At(size_t index) const
        {
            hive::Check(index < size_, "FixedString index out of bounds");
            return buffer_[index];
        }

        // First and last character access
        [[nodiscard]] constexpr char& Front() noexcept
        {
            hive::Assert(size_ > 0, "FixedString is empty");
            return buffer_[0];
        }

        [[nodiscard]] constexpr const char& Front() const noexcept
        {
            hive::Assert(size_ > 0, "FixedString is empty");
            return buffer_[0];
        }

        [[nodiscard]] constexpr char& Back() noexcept
        {
            hive::Assert(size_ > 0, "FixedString is empty");
            return buffer_[size_ - 1];
        }

        [[nodiscard]] constexpr const char& Back() const noexcept
        {
            hive::Assert(size_ > 0, "FixedString is empty");
            return buffer_[size_ - 1];
        }

        // Raw data access (mutable)
        [[nodiscard]] constexpr char* Data() noexcept
        {
            return buffer_;
        }

        [[nodiscard]] constexpr const char* Data() const noexcept
        {
            return buffer_;
        }

        // Null-terminated C string
        [[nodiscard]] constexpr const char* CStr() const noexcept
        {
            return buffer_;
        }

        // Size information
        [[nodiscard]] constexpr size_t Size() const noexcept
        {
            return size_;
        }

        [[nodiscard]] constexpr size_t Length() const noexcept
        {
            return size_;
        }

        [[nodiscard]] constexpr size_t Capacity() const noexcept
        {
            return MaxCapacity;
        }

        [[nodiscard]] constexpr bool IsEmpty() const noexcept
        {
            return size_ == 0;
        }

        [[nodiscard]] constexpr bool IsFull() const noexcept
        {
            return size_ == MaxCapacity;
        }

        // Iterator support
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
            return buffer_ + size_;
        }

        [[nodiscard]] constexpr ConstIterator end() const noexcept
        {
            return buffer_ + size_;
        }

        // View conversion
        [[nodiscard]] constexpr StringView View() const noexcept
        {
            return StringView{buffer_, size_};
        }

        [[nodiscard]] constexpr operator StringView() const noexcept
        {
            return View();
        }

        // Modifiers
        constexpr void Clear() noexcept
        {
            size_ = 0;
            buffer_[0] = '\0';
        }

        constexpr void Append(char ch) noexcept
        {
            // Silently ignore if at capacity (consistent with truncation behavior)
            if (size_ < MaxCapacity)
            {
                buffer_[size_] = ch;
                ++size_;
                buffer_[size_] = '\0';
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

            // Silently truncate to MaxCapacity (consistent with truncation behavior)
            size_t actual_count = (size_ + count <= MaxCapacity) ? count : (MaxCapacity - size_);

            for (size_t i = 0; i < actual_count; ++i)
            {
                buffer_[size_ + i] = str[i];
            }

            size_ += actual_count;
            buffer_[size_] = '\0';
        }

        constexpr void Append(StringView sv) noexcept
        {
            Append(sv.Data(), sv.Size());
        }

        constexpr void PopBack() noexcept
        {
            hive::Assert(size_ > 0, "FixedString is empty");
            if (size_ > 0)
            {
                --size_;
                buffer_[size_] = '\0';
            }
        }

        constexpr void Resize(size_t new_size, char ch = '\0') noexcept
        {
            // Silently truncate to MaxCapacity (consistent with constructors and Append)
            new_size = (new_size <= MaxCapacity) ? new_size : MaxCapacity;

            if (new_size > size_)
            {
                for (size_t i = size_; i < new_size; ++i)
                {
                    buffer_[i] = ch;
                }
            }

            size_ = static_cast<uint8_t>(new_size);
            buffer_[size_] = '\0';
        }

        // Search operations (delegate to StringView)
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

        // Comparison operations
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
        // constexpr strlen implementation
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

        char buffer_[MaxCapacity + 1];  // +1 for null terminator
        uint8_t size_;
    };

    // Comparison operators
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
}
