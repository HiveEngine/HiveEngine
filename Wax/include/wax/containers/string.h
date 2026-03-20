#pragma once

#include <hive/core/assert.h>

#include <comb/memory_resource.h>

#include <wax/containers/string_view.h>

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <functional>
#include <type_traits>
#include <utility>

namespace wax
{
    class String
    {
    public:
        using SizeType = size_t;
        using Iterator = char*;
        using ConstIterator = const char*;

        static constexpr size_t npos = static_cast<size_t>(-1);
        static constexpr size_t ssoCapacity = 22;

        String() noexcept
            : m_allocator{comb::GetDefaultMemoryResource()}
        {
            InitSso();
        }

        String(const char* str)
            : m_allocator{comb::GetDefaultMemoryResource()}
        {
            AssignFromPointer(str, str ? std::strlen(str) : 0);
        }

        String(StringView sv)
            : m_allocator{comb::GetDefaultMemoryResource()}
        {
            AssignFromPointer(sv.Data(), sv.Size());
        }

        explicit String(comb::MemoryResource allocator) noexcept
            : m_allocator{allocator}
        {
            InitSso();
        }

        template <comb::Allocator Allocator>
        explicit String(Allocator& allocator) noexcept
            : String{comb::MemoryResource{allocator}}
        {
        }

        String(comb::MemoryResource allocator, const char* str)
            : m_allocator{allocator}
        {
            AssignFromPointer(str, str ? std::strlen(str) : 0);
        }

        template <comb::Allocator Allocator>
        String(Allocator& allocator, const char* str)
            : String{comb::MemoryResource{allocator}, str}
        {
        }

        String(comb::MemoryResource allocator, StringView sv)
            : m_allocator{allocator}
        {
            AssignFromPointer(sv.Data(), sv.Size());
        }

        template <comb::Allocator Allocator>
        String(Allocator& allocator, StringView sv)
            : String{comb::MemoryResource{allocator}, sv}
        {
        }

        String(comb::MemoryResource allocator, const char* data, size_t size)
            : m_allocator{allocator}
        {
            AssignFromPointer(data, size);
        }

        template <comb::Allocator Allocator>
        String(Allocator& allocator, const char* data, size_t size)
            : String{comb::MemoryResource{allocator}, data, size}
        {
        }

        ~String() noexcept
        {
            if (IsHeap())
            {
                m_allocator.Deallocate(m_heap.m_data);
            }
        }

        String(const String& other)
            : m_allocator{other.m_allocator}
        {
            if (other.IsHeap())
            {
                const size_t size = other.m_heap.m_size;
                InitHeap(size);
                std::memcpy(m_heap.m_data, other.m_heap.m_data, size);
                m_heap.m_data[size] = '\0';
            }
            else
            {
                m_sso = other.m_sso;
            }
        }

        String& operator=(const String& other)
        {
            if (this != &other)
            {
                if (IsHeap())
                {
                    m_allocator.Deallocate(m_heap.m_data);
                }

                m_allocator = other.m_allocator;

                if (other.IsHeap())
                {
                    const size_t size = other.m_heap.m_size;
                    InitHeap(size);
                    std::memcpy(m_heap.m_data, other.m_heap.m_data, size);
                    m_heap.m_data[size] = '\0';
                }
                else
                {
                    m_sso = other.m_sso;
                }
            }
            return *this;
        }

        String(String&& other) noexcept
            : m_allocator{other.m_allocator}
        {
            if (other.IsHeap())
            {
                m_heap = other.m_heap;
                other.InitSso();
            }
            else
            {
                m_sso = other.m_sso;
            }
        }

        String& operator=(String&& other) noexcept
        {
            if (this != &other)
            {
                if (IsHeap())
                {
                    m_allocator.Deallocate(m_heap.m_data);
                }

                m_allocator = other.m_allocator;

                if (other.IsHeap())
                {
                    m_heap = other.m_heap;
                    other.InitSso();
                }
                else
                {
                    m_sso = other.m_sso;
                }
            }
            return *this;
        }

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

        [[nodiscard]] char* Data() noexcept
        {
            return IsHeap() ? m_heap.m_data : m_sso.m_buffer;
        }

        [[nodiscard]] const char* Data() const noexcept
        {
            return IsHeap() ? m_heap.m_data : m_sso.m_buffer;
        }

        [[nodiscard]] const char* CStr() const noexcept
        {
            return Data();
        }

        [[nodiscard]] size_t Size() const noexcept
        {
            return IsHeap() ? m_heap.m_size : GetSsoSize();
        }

        [[nodiscard]] size_t Length() const noexcept
        {
            return Size();
        }

        [[nodiscard]] size_t Capacity() const noexcept
        {
            return IsHeap() ? GetHeapCapacity() : ssoCapacity;
        }

        [[nodiscard]] bool IsEmpty() const noexcept
        {
            return Size() == 0;
        }

        [[nodiscard]] comb::MemoryResource GetAllocator() const noexcept
        {
            return m_allocator;
        }

        [[nodiscard]] Iterator Begin() noexcept
        {
            return Data();
        }

        [[nodiscard]] ConstIterator Begin() const noexcept
        {
            return Data();
        }

        [[nodiscard]] Iterator End() noexcept
        {
            return Data() + Size();
        }

        [[nodiscard]] ConstIterator End() const noexcept
        {
            return Data() + Size();
        }

        [[nodiscard]] StringView View() const noexcept
        {
            return StringView{Data(), Size()};
        }

        [[nodiscard]] operator StringView() const noexcept
        {
            return View();
        }

        void Reserve(size_t newCapacity)
        {
            if (newCapacity <= Capacity())
            {
                return;
            }

            const size_t currentSize = Size();
            char* newData = static_cast<char*>(m_allocator.Allocate(newCapacity + 1, 1));
            hive::Check(newData != nullptr, "String allocation failed");

            if (currentSize > 0)
            {
                std::memcpy(newData, Data(), currentSize);
            }
            newData[currentSize] = '\0';

            if (IsHeap())
            {
                m_allocator.Deallocate(m_heap.m_data);
            }

            m_heap.m_data = newData;
            m_heap.m_size = currentSize;
            SetHeapCapacity(newCapacity);
        }

        void ShrinkToFit()
        {
            const size_t currentSize = Size();

            if (currentSize <= ssoCapacity && IsHeap())
            {
                char tempBuffer[ssoBufferSize]{};
                std::memcpy(tempBuffer, m_heap.m_data, currentSize);

                m_allocator.Deallocate(m_heap.m_data);

                InitSso();
                std::memcpy(m_sso.m_buffer, tempBuffer, currentSize);
                SetSsoSize(currentSize);
            }
            else if (IsHeap() && currentSize < GetHeapCapacity())
            {
                char* newData = static_cast<char*>(m_allocator.Allocate(currentSize + 1, 1));
                hive::Check(newData != nullptr, "String allocation failed");

                std::memcpy(newData, m_heap.m_data, currentSize);
                newData[currentSize] = '\0';

                m_allocator.Deallocate(m_heap.m_data);
                m_heap.m_data = newData;
                m_heap.m_size = currentSize;
                SetHeapCapacity(currentSize);
            }
        }

        void Clear() noexcept
        {
            if (IsHeap())
            {
                m_heap.m_size = 0;
                m_heap.m_data[0] = '\0';
            }
            else
            {
                SetSsoSize(0);
            }
        }

        void Append(char ch)
        {
            const size_t currentSize = Size();
            const size_t newSize = currentSize + 1;

            if (newSize > Capacity())
            {
                Reserve(newSize * 2);
            }

            Data()[currentSize] = ch;
            Data()[newSize] = '\0';

            if (IsHeap())
            {
                m_heap.m_size = newSize;
            }
            else
            {
                SetSsoSize(newSize);
            }
        }

        void Append(const char* str)
        {
            if (str == nullptr)
            {
                return;
            }

            Append(str, std::strlen(str));
        }

        void Append(const char* str, size_t count)
        {
            if (str == nullptr || count == 0)
            {
                return;
            }

            const size_t currentSize = Size();
            const size_t newSize = currentSize + count;

            if (newSize > Capacity())
            {
                // Detect self-referencing: str points inside our buffer
                const char* bufStart = Data();
                const char* bufEnd = bufStart + currentSize;
                const bool isSelfRef = (str >= bufStart && str < bufEnd);
                size_t selfOffset = static_cast<size_t>(str - bufStart);

                Reserve(newSize * 2);

                if (isSelfRef)
                {
                    str = Data() + selfOffset;
                }
            }

            std::memcpy(Data() + currentSize, str, count);
            Data()[newSize] = '\0';

            if (IsHeap())
            {
                m_heap.m_size = newSize;
            }
            else
            {
                SetSsoSize(newSize);
            }
        }

        void Append(StringView sv)
        {
            Append(sv.Data(), sv.Size());
        }

        void PopBack() noexcept
        {
            const size_t currentSize = Size();
            hive::Assert(currentSize > 0, "String is empty");

            const size_t newSize = currentSize - 1;
            Data()[newSize] = '\0';

            if (IsHeap())
            {
                m_heap.m_size = newSize;
            }
            else
            {
                SetSsoSize(newSize);
            }
        }

        void Resize(size_t newSize, char ch = '\0')
        {
            const size_t currentSize = Size();

            if (newSize > Capacity())
            {
                Reserve(newSize);
            }

            if (newSize > currentSize)
            {
                std::memset(Data() + currentSize, ch, newSize - currentSize);
            }

            Data()[newSize] = '\0';

            if (IsHeap())
            {
                m_heap.m_size = newSize;
            }
            else
            {
                SetSsoSize(newSize);
            }
        }

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
        static constexpr size_t ssoBufferSize = 23;

        struct SsoLayout
        {
            char m_buffer[ssoBufferSize];
            uint8_t m_sizeAndFlag;
        };

        struct HeapLayout
        {
            char* m_data;
            size_t m_size;
            size_t m_capacity;
        };

        union
        {
            SsoLayout m_sso;
            HeapLayout m_heap;
        };

        static_assert(sizeof(SsoLayout) == sizeof(HeapLayout),
                      "SSO/Heap layout size mismatch — String SSO requires 64-bit platform");
        static_assert(sizeof(size_t) == 8,
                      "String SSO flag trick requires 64-bit size_t");
        static_assert(offsetof(SsoLayout, m_sizeAndFlag) == sizeof(HeapLayout) - 1,
                      "SSO flag byte must overlap high byte of HeapLayout::m_capacity (requires little-endian)");

        comb::MemoryResource m_allocator;

        [[nodiscard]] bool IsHeap() const noexcept
        {
            return (m_sso.m_sizeAndFlag & 0x80) != 0;
        }

        void InitSso() noexcept
        {
            m_sso.m_buffer[0] = '\0';
            m_sso.m_sizeAndFlag = 0;
        }

        void InitHeap(size_t size)
        {
            const size_t capacity = size;
            m_heap.m_data = static_cast<char*>(m_allocator.Allocate(capacity + 1, 1));
            hive::Check(m_heap.m_data != nullptr, "String allocation failed");
            m_heap.m_size = size;
            SetHeapCapacity(capacity);
        }

        void AssignFromPointer(const char* data, size_t size)
        {
            if (size <= ssoCapacity)
            {
                InitSso();
                if (size > 0 && data != nullptr)
                {
                    std::memcpy(m_sso.m_buffer, data, size);
                }
                SetSsoSize(size);
            }
            else
            {
                InitHeap(size);
                if (data != nullptr)
                {
                    std::memcpy(m_heap.m_data, data, size);
                }
                m_heap.m_data[size] = '\0';
            }
        }

        [[nodiscard]] size_t GetSsoSize() const noexcept
        {
            return m_sso.m_sizeAndFlag & 0x7F;
        }

        void SetSsoSize(size_t size) noexcept
        {
            hive::Assert(size <= ssoCapacity, "SSO size exceeds capacity");
            m_sso.m_sizeAndFlag = static_cast<uint8_t>(size);
            m_sso.m_buffer[size] = '\0';
        }

        [[nodiscard]] size_t GetHeapCapacity() const noexcept
        {
            return m_heap.m_capacity & ~(1ULL << 63);
        }

        void SetHeapCapacity(size_t capacity) noexcept
        {
            m_heap.m_capacity = capacity | (1ULL << 63);
        }
    };

    [[nodiscard]] inline bool operator==(const String& lhs, const String& rhs) noexcept
    {
        return lhs.Equals(rhs.View());
    }

    [[nodiscard]] inline bool operator==(const String& lhs, StringView rhs) noexcept
    {
        return lhs.Equals(rhs);
    }

    [[nodiscard]] inline bool operator==(StringView lhs, const String& rhs) noexcept
    {
        return rhs.Equals(lhs);
    }

    [[nodiscard]] inline bool operator==(const String& lhs, const char* rhs) noexcept
    {
        return lhs.Equals(StringView{rhs ? rhs : "", rhs ? std::strlen(rhs) : 0});
    }

    [[nodiscard]] inline bool operator==(const char* lhs, const String& rhs) noexcept
    {
        return rhs == lhs;
    }

    [[nodiscard]] inline bool operator<(const String& lhs, const String& rhs) noexcept
    {
        return lhs.Compare(rhs.View()) < 0;
    }

    [[nodiscard]] inline bool operator<(const String& lhs, StringView rhs) noexcept
    {
        return lhs.Compare(rhs) < 0;
    }

    [[nodiscard]] inline bool operator<(StringView lhs, const String& rhs) noexcept
    {
        return rhs.Compare(lhs) > 0;
    }

    [[nodiscard]] inline bool operator<=(const String& lhs, const String& rhs) noexcept
    {
        return lhs.Compare(rhs.View()) <= 0;
    }

    [[nodiscard]] inline bool operator>(const String& lhs, const String& rhs) noexcept
    {
        return lhs.Compare(rhs.View()) > 0;
    }

    [[nodiscard]] inline bool operator>=(const String& lhs, const String& rhs) noexcept
    {
        return lhs.Compare(rhs.View()) >= 0;
    }

    [[nodiscard]] inline String operator+(const String& lhs, const String& rhs)
    {
        String result = lhs;
        result.Append(rhs.View());
        return result;
    }

    [[nodiscard]] inline String operator+(const String& lhs, StringView rhs)
    {
        String result = lhs;
        result.Append(rhs);
        return result;
    }

    [[nodiscard]] inline String operator+(const String& lhs, const char* rhs)
    {
        String result = lhs;
        result.Append(rhs);
        return result;
    }
} // namespace wax

template <> struct std::hash<wax::String>
{
    size_t operator()(const wax::String& s) const noexcept
    {
        size_t h = 14695981039346656037ull;
        for (size_t i = 0; i < s.Size(); ++i)
        {
            h ^= static_cast<size_t>(static_cast<unsigned char>(s.Data()[i]));
            h *= 1099511628211ull;
        }
        return h;
    }
};