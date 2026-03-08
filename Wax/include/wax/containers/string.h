#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <functional>
#include <type_traits>
#include <utility>
#include <comb/memory_resource.h>
#include <hive/core/assert.h>
#include <wax/containers/string_view.h>

namespace wax
{
    class String
    {
    public:
        using SizeType = size_t;
        using Iterator = char*;
        using ConstIterator = const char*;

        static constexpr size_t npos = static_cast<size_t>(-1);
        static constexpr size_t SsoCapacity = 22;

        String() noexcept
            : allocator_{comb::GetDefaultMemoryResource()}
        {
            InitSso();
        }

        String(const char* str)
            : allocator_{comb::GetDefaultMemoryResource()}
        {
            AssignFromPointer(str, str ? std::strlen(str) : 0);
        }

        String(StringView sv)
            : allocator_{comb::GetDefaultMemoryResource()}
        {
            AssignFromPointer(sv.Data(), sv.Size());
        }

        explicit String(comb::MemoryResource allocator) noexcept
            : allocator_{allocator}
        {
            InitSso();
        }

        template<comb::Allocator Allocator>
        explicit String(Allocator& allocator) noexcept
            : String{comb::MemoryResource{allocator}}
        {}

        String(comb::MemoryResource allocator, const char* str)
            : allocator_{allocator}
        {
            AssignFromPointer(str, str ? std::strlen(str) : 0);
        }

        template<comb::Allocator Allocator>
        String(Allocator& allocator, const char* str)
            : String{comb::MemoryResource{allocator}, str}
        {}

        String(comb::MemoryResource allocator, StringView sv)
            : allocator_{allocator}
        {
            AssignFromPointer(sv.Data(), sv.Size());
        }

        template<comb::Allocator Allocator>
        String(Allocator& allocator, StringView sv)
            : String{comb::MemoryResource{allocator}, sv}
        {}

        String(comb::MemoryResource allocator, const char* data, size_t size)
            : allocator_{allocator}
        {
            AssignFromPointer(data, size);
        }

        template<comb::Allocator Allocator>
        String(Allocator& allocator, const char* data, size_t size)
            : String{comb::MemoryResource{allocator}, data, size}
        {}

        ~String() noexcept
        {
            if (IsHeap())
            {
                allocator_.Deallocate(heap_.data);
            }
        }

        String(const String& other)
            : allocator_{other.allocator_}
        {
            if (other.IsHeap())
            {
                const size_t size = other.heap_.size;
                InitHeap(size);
                std::memcpy(heap_.data, other.heap_.data, size);
                heap_.data[size] = '\0';
            }
            else
            {
                sso_ = other.sso_;
            }
        }

        String& operator=(const String& other)
        {
            if (this != &other)
            {
                if (IsHeap())
                {
                    allocator_.Deallocate(heap_.data);
                }

                allocator_ = other.allocator_;

                if (other.IsHeap())
                {
                    const size_t size = other.heap_.size;
                    InitHeap(size);
                    std::memcpy(heap_.data, other.heap_.data, size);
                    heap_.data[size] = '\0';
                }
                else
                {
                    sso_ = other.sso_;
                }
            }
            return *this;
        }

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

        String& operator=(String&& other) noexcept
        {
            if (this != &other)
            {
                if (IsHeap())
                {
                    allocator_.Deallocate(heap_.data);
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
            return IsHeap() ? heap_.data : sso_.buffer;
        }

        [[nodiscard]] const char* Data() const noexcept
        {
            return IsHeap() ? heap_.data : sso_.buffer;
        }

        [[nodiscard]] const char* CStr() const noexcept
        {
            return Data();
        }

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

        [[nodiscard]] comb::MemoryResource GetAllocator() const noexcept
        {
            return allocator_;
        }

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

        [[nodiscard]] StringView View() const noexcept
        {
            return StringView{Data(), Size()};
        }

        [[nodiscard]] operator StringView() const noexcept
        {
            return View();
        }

        void Reserve(size_t new_capacity)
        {
            if (new_capacity <= Capacity())
            {
                return;
            }

            const size_t current_size = Size();
            char* new_data = static_cast<char*>(allocator_.Allocate(new_capacity + 1, 1));
            hive::Check(new_data != nullptr, "String allocation failed");

            if (current_size > 0)
            {
                std::memcpy(new_data, Data(), current_size);
            }
            new_data[current_size] = '\0';

            if (IsHeap())
            {
                allocator_.Deallocate(heap_.data);
            }

            heap_.data = new_data;
            heap_.size = current_size;
            SetHeapCapacity(new_capacity);
        }

        void ShrinkToFit()
        {
            const size_t current_size = Size();

            if (current_size <= SsoCapacity && IsHeap())
            {
                char temp_buffer[SsoBufferSize]{};
                std::memcpy(temp_buffer, heap_.data, current_size);

                allocator_.Deallocate(heap_.data);

                InitSso();
                std::memcpy(sso_.buffer, temp_buffer, current_size);
                SetSsoSize(current_size);
            }
            else if (IsHeap() && current_size < GetHeapCapacity())
            {
                char* new_data = static_cast<char*>(allocator_.Allocate(current_size + 1, 1));
                hive::Check(new_data != nullptr, "String allocation failed");

                std::memcpy(new_data, heap_.data, current_size);
                new_data[current_size] = '\0';

                allocator_.Deallocate(heap_.data);
                heap_.data = new_data;
                heap_.size = current_size;
                SetHeapCapacity(current_size);
            }
        }

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
            const size_t current_size = Size();
            const size_t new_size = current_size + 1;

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

            Append(str, std::strlen(str));
        }

        void Append(const char* str, size_t count)
        {
            if (str == nullptr || count == 0)
            {
                return;
            }

            const size_t current_size = Size();
            const size_t new_size = current_size + count;

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
            const size_t current_size = Size();
            hive::Assert(current_size > 0, "String is empty");

            const size_t new_size = current_size - 1;
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
            const size_t current_size = Size();

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
        static constexpr size_t SsoBufferSize = 23;

        struct SsoLayout
        {
            char buffer[SsoBufferSize];
            uint8_t size_and_flag;
        };

        struct HeapLayout
        {
            char* data;
            size_t size;
            size_t capacity;
        };

        union
        {
            SsoLayout sso_;
            HeapLayout heap_;
        };

        comb::MemoryResource allocator_;

        [[nodiscard]] bool IsHeap() const noexcept
        {
            return (sso_.size_and_flag & 0x80) != 0;
        }

        void InitSso() noexcept
        {
            sso_.buffer[0] = '\0';
            sso_.size_and_flag = 0;
        }

        void InitHeap(size_t size)
        {
            const size_t capacity = size;
            heap_.data = static_cast<char*>(allocator_.Allocate(capacity + 1, 1));
            hive::Check(heap_.data != nullptr, "String allocation failed");
            heap_.size = size;
            SetHeapCapacity(capacity);
        }

        void AssignFromPointer(const char* data, size_t size)
        {
            if (size <= SsoCapacity)
            {
                InitSso();
                if (size > 0 && data != nullptr)
                {
                    std::memcpy(sso_.buffer, data, size);
                }
                SetSsoSize(size);
            }
            else
            {
                InitHeap(size);
                if (data != nullptr)
                {
                    std::memcpy(heap_.data, data, size);
                }
                heap_.data[size] = '\0';
            }
        }

        [[nodiscard]] size_t GetSsoSize() const noexcept
        {
            return sso_.size_and_flag & 0x7F;
        }

        void SetSsoSize(size_t size) noexcept
        {
            hive::Assert(size <= SsoCapacity, "SSO size exceeds capacity");
            sso_.size_and_flag = static_cast<uint8_t>(size);
            sso_.buffer[size] = '\0';
        }

        [[nodiscard]] size_t GetHeapCapacity() const noexcept
        {
            return heap_.capacity & ~(1ULL << 63);
        }

        void SetHeapCapacity(size_t capacity) noexcept
        {
            heap_.capacity = capacity | (1ULL << 63);
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
}

template<>
struct std::hash<wax::String>
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