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
     * Dynamic string with Small String Optimization (SSO) and custom allocator support
     *
     * String provides a mutable, null-terminated string that uses SSO to avoid
     * heap allocations for short strings (≤22 characters). Longer strings use
     * a pluggable allocator system for explicit memory control.
     *
     * Performance characteristics:
     * - Storage: 24 bytes (SSO buffer or heap pointer + size + capacity)
     * - SSO threshold: 22 characters (zero heap allocations!)
     * - Access: O(1) - direct pointer arithmetic
     * - Append: O(1) amortized if SSO, O(n) if reallocation needed
     * - Insert/Erase: O(n) - requires shifting characters
     * - Find: O(n*m) naive search
     * - Growth factor: 2x capacity when full
     *
     * Memory layout (SSO mode - strings ≤22 chars):
     * ┌───────────────────────────────┐
     * │ char buffer[23]               │  Inline storage (22 chars + '\0')
     * │ uint8_t size (bit 7 = 0)      │  Size with SSO flag
     * └───────────────────────────────┘
     *   24 bytes total
     *
     * Memory layout (Heap mode - strings >22 chars):
     * ┌───────────────────────────────┐
     * │ char* data                    │  Pointer to heap
     * │ size_t size                   │  String length
     * │ size_t capacity (bit 63 = 1)  │  Capacity with heap flag
     * └───────────────────────────────┘
     *   24 bytes total
     *
     * Limitations:
     * - No exception safety (aborts on allocation failure)
     * - Allocator must outlive the string
     * - UTF-8 data treated as raw bytes (no encoding awareness)
     * - Not thread-safe (requires external synchronization)
     *
     * Use cases:
     * - Short-lived strings (names, paths, logs) - SSO eliminates allocations
     * - Dynamic text manipulation in game/engine code
     * - Replacing std::string with explicit allocator control
     *
     * Example:
     * @code
     *   comb::LinearAllocator alloc{1024};
     *
     *   // SSO - no heap allocation!
     *   wax::String<comb::LinearAllocator> name{alloc, "Player"};
     *   name.Append("1");  // Still SSO (7 chars)
     *
     *   // Heap allocation (>22 chars)
     *   wax::String<comb::LinearAllocator> path{alloc, "Assets/Textures/hero.png"};
     *
     *   // String operations
     *   if (path.StartsWith("Assets/")) {
     *       wax::StringView filename = path.View().Substr(7);
     *   }
     *
     *   // Null-terminated C string
     *   const char* c_str = path.CStr();
     * @endcode
     */
    template<typename Allocator>
    class String
    {
    public:
        using SizeType = size_t;
        using Iterator = char*;
        using ConstIterator = const char*;

        static constexpr size_t npos = static_cast<size_t>(-1);
        static constexpr size_t SsoCapacity = 22;  // 23 bytes buffer - 1 for null terminator

        // Default constructor (empty string, SSO mode)
        explicit String(Allocator& allocator) noexcept
            : allocator_{&allocator}
        {
            InitSso();
        }

        // Constructor from C string
        String(Allocator& allocator, const char* str)
            : allocator_{&allocator}
        {
            size_t len = str ? std::strlen(str) : 0;

            if (len <= SsoCapacity)
            {
                InitSso();
                std::memcpy(sso_.buffer, str, len);
                SetSsoSize(len);
            }
            else
            {
                InitHeap(len);
                std::memcpy(heap_.data, str, len);
                heap_.data[len] = '\0';
            }
        }

        // Constructor from StringView
        String(Allocator& allocator, StringView sv)
            : allocator_{&allocator}
        {
            if (sv.Size() <= SsoCapacity)
            {
                InitSso();
                std::memcpy(sso_.buffer, sv.Data(), sv.Size());
                SetSsoSize(sv.Size());
            }
            else
            {
                InitHeap(sv.Size());
                std::memcpy(heap_.data, sv.Data(), sv.Size());
                heap_.data[sv.Size()] = '\0';
            }
        }

        // Constructor from pointer and size
        String(Allocator& allocator, const char* data, size_t size)
            : allocator_{&allocator}
        {
            if (size <= SsoCapacity)
            {
                InitSso();
                std::memcpy(sso_.buffer, data, size);
                SetSsoSize(size);
            }
            else
            {
                InitHeap(size);
                std::memcpy(heap_.data, data, size);
                heap_.data[size] = '\0';
            }
        }

        // Destructor
        ~String() noexcept
        {
            if (IsHeap())
            {
                allocator_->Deallocate(heap_.data);
            }
        }

        // Copy constructor
        String(const String& other)
            : allocator_{other.allocator_}
        {
            if (other.IsHeap())
            {
                // Heap mode: allocate and copy
                size_t size = other.heap_.size;
                InitHeap(size);
                std::memcpy(heap_.data, other.heap_.data, size);
                heap_.data[size] = '\0';
            }
            else
            {
                // SSO mode: direct structure copy (fast path)
                sso_ = other.sso_;
            }
        }

        // Copy assignment
        String& operator=(const String& other)
        {
            if (this != &other)
            {
                if (IsHeap())
                {
                    allocator_->Deallocate(heap_.data);
                }

                allocator_ = other.allocator_;

                if (other.IsHeap())
                {
                    // Heap mode: allocate and copy
                    size_t size = other.heap_.size;
                    InitHeap(size);
                    std::memcpy(heap_.data, other.heap_.data, size);
                    heap_.data[size] = '\0';
                }
                else
                {
                    // SSO mode: direct structure copy (fast path)
                    sso_ = other.sso_;
                }
            }
            return *this;
        }

        // Move constructor
        String(String&& other) noexcept
            : allocator_{other.allocator_}
        {
            if (other.IsHeap())
            {
                heap_ = other.heap_;
                other.InitSso();
            }
            else
            {
                sso_ = other.sso_;
            }
        }

        // Move assignment
        String& operator=(String&& other) noexcept
        {
            if (this != &other)
            {
                if (IsHeap())
                {
                    allocator_->Deallocate(heap_.data);
                }

                allocator_ = other.allocator_;

                if (other.IsHeap())
                {
                    heap_ = other.heap_;
                    other.InitSso();
                }
                else
                {
                    sso_ = other.sso_;
                }
            }
            return *this;
        }

        // Element access (bounds-checked in debug)
        [[nodiscard]] char& operator[](size_t index) noexcept
        {
            hive::Assert(index < Size(), "String index out of bounds");
            return Data()[index];
        }

        [[nodiscard]] const char& operator[](size_t index) const noexcept
        {
            hive::Assert(index < Size(), "String index out of bounds");
            return Data()[index];
        }

        // Element access (always bounds-checked)
        [[nodiscard]] char& At(size_t index)
        {
            hive::Check(index < Size(), "String index out of bounds");
            return Data()[index];
        }

        [[nodiscard]] const char& At(size_t index) const
        {
            hive::Check(index < Size(), "String index out of bounds");
            return Data()[index];
        }

        // First and last character access
        [[nodiscard]] char& Front() noexcept
        {
            hive::Assert(Size() > 0, "String is empty");
            return Data()[0];
        }

        [[nodiscard]] const char& Front() const noexcept
        {
            hive::Assert(Size() > 0, "String is empty");
            return Data()[0];
        }

        [[nodiscard]] char& Back() noexcept
        {
            hive::Assert(Size() > 0, "String is empty");
            return Data()[Size() - 1];
        }

        [[nodiscard]] const char& Back() const noexcept
        {
            hive::Assert(Size() > 0, "String is empty");
            return Data()[Size() - 1];
        }

        // Raw data access (mutable)
        [[nodiscard]] char* Data() noexcept
        {
            return IsHeap() ? heap_.data : sso_.buffer;
        }

        [[nodiscard]] const char* Data() const noexcept
        {
            return IsHeap() ? heap_.data : sso_.buffer;
        }

        // Null-terminated C string
        [[nodiscard]] const char* CStr() const noexcept
        {
            return Data();
        }

        // Size information
        [[nodiscard]] size_t Size() const noexcept
        {
            return IsHeap() ? heap_.size : GetSsoSize();
        }

        [[nodiscard]] size_t Length() const noexcept
        {
            return Size();
        }

        [[nodiscard]] size_t Capacity() const noexcept
        {
            return IsHeap() ? GetHeapCapacity() : SsoCapacity;
        }

        [[nodiscard]] bool IsEmpty() const noexcept
        {
            return Size() == 0;
        }

        // Iterator support
        [[nodiscard]] Iterator begin() noexcept
        {
            return Data();
        }

        [[nodiscard]] ConstIterator begin() const noexcept
        {
            return Data();
        }

        [[nodiscard]] Iterator end() noexcept
        {
            return Data() + Size();
        }

        [[nodiscard]] ConstIterator end() const noexcept
        {
            return Data() + Size();
        }

        // View conversion
        [[nodiscard]] StringView View() const noexcept
        {
            return StringView{Data(), Size()};
        }

        [[nodiscard]] operator StringView() const noexcept
        {
            return View();
        }

        // Capacity management
        void Reserve(size_t new_capacity)
        {
            if (new_capacity <= Capacity())
            {
                return;
            }

            size_t current_size = Size();

            if (IsHeap())
            {
                char* new_data = static_cast<char*>(allocator_->Allocate(new_capacity + 1, 1));
                hive::Check(new_data != nullptr, "String allocation failed");

                std::memcpy(new_data, heap_.data, current_size);
                new_data[current_size] = '\0';

                allocator_->Deallocate(heap_.data);
                heap_.data = new_data;
                heap_.size = current_size;
                SetHeapCapacity(new_capacity);
            }
            else
            {
                char* new_data = static_cast<char*>(allocator_->Allocate(new_capacity + 1, 1));
                hive::Check(new_data != nullptr, "String allocation failed");

                std::memcpy(new_data, sso_.buffer, current_size);
                new_data[current_size] = '\0';

                heap_.data = new_data;
                heap_.size = current_size;
                SetHeapCapacity(new_capacity);
            }
        }

        void ShrinkToFit()
        {
            size_t current_size = Size();

            if (current_size <= SsoCapacity && IsHeap())
            {
                char temp_buffer[SsoCapacity];
                std::memcpy(temp_buffer, heap_.data, current_size);

                allocator_->Deallocate(heap_.data);

                InitSso();
                std::memcpy(sso_.buffer, temp_buffer, current_size);
                SetSsoSize(current_size);
            }
            else if (IsHeap() && current_size < GetHeapCapacity())
            {
                char* new_data = static_cast<char*>(allocator_->Allocate(current_size + 1, 1));
                hive::Check(new_data != nullptr, "String allocation failed");

                std::memcpy(new_data, heap_.data, current_size);
                new_data[current_size] = '\0';

                allocator_->Deallocate(heap_.data);
                heap_.data = new_data;
                heap_.size = current_size;
                SetHeapCapacity(current_size);
            }
        }

        // Modifiers
        void Clear() noexcept
        {
            if (IsHeap())
            {
                heap_.size = 0;
                heap_.data[0] = '\0';
            }
            else
            {
                SetSsoSize(0);
            }
        }

        void Append(char ch)
        {
            size_t current_size = Size();
            size_t new_size = current_size + 1;

            if (new_size > Capacity())
            {
                Reserve(new_size * 2);
            }

            Data()[current_size] = ch;
            Data()[new_size] = '\0';

            if (IsHeap())
            {
                heap_.size = new_size;
            }
            else
            {
                SetSsoSize(new_size);
            }
        }

        void Append(const char* str)
        {
            if (str == nullptr)
            {
                return;
            }

            size_t str_len = std::strlen(str);
            Append(str, str_len);
        }

        void Append(const char* str, size_t count)
        {
            if (str == nullptr || count == 0)
            {
                return;
            }

            size_t current_size = Size();
            size_t new_size = current_size + count;

            if (new_size > Capacity())
            {
                Reserve(new_size * 2);
            }

            std::memcpy(Data() + current_size, str, count);
            Data()[new_size] = '\0';

            if (IsHeap())
            {
                heap_.size = new_size;
            }
            else
            {
                SetSsoSize(new_size);
            }
        }

        void Append(StringView sv)
        {
            Append(sv.Data(), sv.Size());
        }

        void PopBack() noexcept
        {
            size_t current_size = Size();
            hive::Assert(current_size > 0, "String is empty");

            size_t new_size = current_size - 1;
            Data()[new_size] = '\0';

            if (IsHeap())
            {
                heap_.size = new_size;
            }
            else
            {
                SetSsoSize(new_size);
            }
        }

        void Resize(size_t new_size, char ch = '\0')
        {
            size_t current_size = Size();

            if (new_size > Capacity())
            {
                Reserve(new_size);
            }

            if (new_size > current_size)
            {
                std::memset(Data() + current_size, ch, new_size - current_size);
            }

            Data()[new_size] = '\0';

            if (IsHeap())
            {
                heap_.size = new_size;
            }
            else
            {
                SetSsoSize(new_size);
            }
        }

        // Search operations (delegate to StringView)
        [[nodiscard]] size_t Find(char ch, size_t pos = 0) const noexcept
        {
            return View().Find(ch, pos);
        }

        [[nodiscard]] size_t Find(StringView sv, size_t pos = 0) const noexcept
        {
            return View().Find(sv, pos);
        }

        [[nodiscard]] size_t RFind(char ch, size_t pos = npos) const noexcept
        {
            return View().RFind(ch, pos);
        }

        [[nodiscard]] bool Contains(char ch) const noexcept
        {
            return View().Contains(ch);
        }

        [[nodiscard]] bool Contains(StringView sv) const noexcept
        {
            return View().Contains(sv);
        }

        [[nodiscard]] bool StartsWith(char ch) const noexcept
        {
            return View().StartsWith(ch);
        }

        [[nodiscard]] bool StartsWith(StringView sv) const noexcept
        {
            return View().StartsWith(sv);
        }

        [[nodiscard]] bool EndsWith(char ch) const noexcept
        {
            return View().EndsWith(ch);
        }

        [[nodiscard]] bool EndsWith(StringView sv) const noexcept
        {
            return View().EndsWith(sv);
        }

        // Comparison operations
        [[nodiscard]] int Compare(const String& other) const noexcept
        {
            return View().Compare(other.View());
        }

        [[nodiscard]] int Compare(StringView sv) const noexcept
        {
            return View().Compare(sv);
        }

        [[nodiscard]] bool Equals(const String& other) const noexcept
        {
            return View().Equals(other.View());
        }

        [[nodiscard]] bool Equals(StringView sv) const noexcept
        {
            return View().Equals(sv);
        }

    private:
        static constexpr size_t SsoBufferSize = 23;  // Physical buffer size in bytes

        // SSO (Small String Optimization) layout
        struct SsoLayout
        {
            char buffer[SsoBufferSize];  // 23 bytes physical buffer
            uint8_t size_and_flag;       // bit 7 = 0 (SSO mode), bits 0-6 = size
        };

        // Heap layout
        struct HeapLayout
        {
            char* data;
            size_t size;
            size_t capacity;  // bit 63 = 1 (heap mode flag), bits 0-62 = capacity
        };

        union
        {
            SsoLayout sso_;
            HeapLayout heap_;
        };

        Allocator* allocator_;

        // Helper functions
        [[nodiscard]] bool IsHeap() const noexcept
        {
            // Test bit 7 of last byte (offset 23)
            // SSO: bit 7 of size_and_flag = 0
            // Heap: bit 7 of last byte of capacity (bit 63 overall) = 1
            return (sso_.size_and_flag & 0x80) != 0;
        }

        void InitSso() noexcept
        {
            sso_.buffer[0] = '\0';
            sso_.size_and_flag = 0;  // SSO mode (bit 7 = 0)
        }

        void InitHeap(size_t size)
        {
            size_t capacity = size;
            heap_.data = static_cast<char*>(allocator_->Allocate(capacity + 1, 1));
            hive::Check(heap_.data != nullptr, "String allocation failed");
            heap_.size = size;
            SetHeapCapacity(capacity);
        }

        [[nodiscard]] size_t GetSsoSize() const noexcept
        {
            return sso_.size_and_flag & 0x7F;  // Mask out bit 7, get bits 0-6
        }

        void SetSsoSize(size_t size) noexcept
        {
            hive::Assert(size <= SsoCapacity, "SSO size exceeds capacity");
            sso_.size_and_flag = static_cast<uint8_t>(size);  // bit 7 = 0 (SSO mode)
            sso_.buffer[size] = '\0';
        }

        [[nodiscard]] size_t GetHeapCapacity() const noexcept
        {
            return heap_.capacity & ~(1ULL << 63);  // Mask out bit 63
        }

        void SetHeapCapacity(size_t capacity) noexcept
        {
            heap_.capacity = capacity | (1ULL << 63);  // Set bit 63 (heap mode flag)
        }
    };

    // Comparison operators
    template<typename Allocator>
    [[nodiscard]] bool operator==(const String<Allocator>& lhs, const String<Allocator>& rhs) noexcept
    {
        return lhs.Equals(rhs);
    }

    template<typename Allocator>
    [[nodiscard]] bool operator==(const String<Allocator>& lhs, StringView rhs) noexcept
    {
        return lhs.Equals(rhs);
    }

    template<typename Allocator>
    [[nodiscard]] bool operator==(StringView lhs, const String<Allocator>& rhs) noexcept
    {
        return rhs.Equals(lhs);
    }

    template<typename Allocator>
    [[nodiscard]] bool operator!=(const String<Allocator>& lhs, const String<Allocator>& rhs) noexcept
    {
        return !lhs.Equals(rhs);
    }

    template<typename Allocator>
    [[nodiscard]] bool operator!=(const String<Allocator>& lhs, StringView rhs) noexcept
    {
        return !lhs.Equals(rhs);
    }

    template<typename Allocator>
    [[nodiscard]] bool operator!=(StringView lhs, const String<Allocator>& rhs) noexcept
    {
        return !rhs.Equals(lhs);
    }

    template<typename Allocator>
    [[nodiscard]] bool operator<(const String<Allocator>& lhs, const String<Allocator>& rhs) noexcept
    {
        return lhs.Compare(rhs) < 0;
    }

    template<typename Allocator>
    [[nodiscard]] bool operator<(const String<Allocator>& lhs, StringView rhs) noexcept
    {
        return lhs.Compare(rhs) < 0;
    }

    template<typename Allocator>
    [[nodiscard]] bool operator<(StringView lhs, const String<Allocator>& rhs) noexcept
    {
        return rhs.Compare(lhs) > 0;
    }

    template<typename Allocator>
    [[nodiscard]] bool operator<=(const String<Allocator>& lhs, const String<Allocator>& rhs) noexcept
    {
        return lhs.Compare(rhs) <= 0;
    }

    template<typename Allocator>
    [[nodiscard]] bool operator>(const String<Allocator>& lhs, const String<Allocator>& rhs) noexcept
    {
        return lhs.Compare(rhs) > 0;
    }

    template<typename Allocator>
    [[nodiscard]] bool operator>=(const String<Allocator>& lhs, const String<Allocator>& rhs) noexcept
    {
        return lhs.Compare(rhs) >= 0;
    }

    // Concatenation operator
    template<typename Allocator>
    [[nodiscard]] String<Allocator> operator+(const String<Allocator>& lhs, const String<Allocator>& rhs)
    {
        String<Allocator> result = lhs;
        result.Append(rhs.View());
        return result;
    }

    template<typename Allocator>
    [[nodiscard]] String<Allocator> operator+(const String<Allocator>& lhs, StringView rhs)
    {
        String<Allocator> result = lhs;
        result.Append(rhs);
        return result;
    }

    template<typename Allocator>
    [[nodiscard]] String<Allocator> operator+(const String<Allocator>& lhs, const char* rhs)
    {
        String<Allocator> result = lhs;
        result.Append(rhs);
        return result;
    }
}
