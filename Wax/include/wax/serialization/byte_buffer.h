#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <comb/memory_resource.h>
#include <hive/core/assert.h>
#include <wax/containers/vector.h>
#include <wax/serialization/byte_span.h>

namespace wax
{
    class ByteBuffer
    {
    public:
        ByteBuffer() noexcept
            : data_{}
        {}

        explicit ByteBuffer(size_t initial_capacity) noexcept
            : data_(initial_capacity)
        {}

        explicit ByteBuffer(comb::MemoryResource allocator) noexcept
            : data_{allocator}
        {}

        template<comb::Allocator Allocator>
        explicit ByteBuffer(Allocator& allocator) noexcept
            : data_{allocator}
        {}

        ByteBuffer(comb::MemoryResource allocator, size_t initial_capacity) noexcept
            : data_{allocator, initial_capacity}
        {}

        template<comb::Allocator Allocator>
        ByteBuffer(Allocator& allocator, size_t initial_capacity) noexcept
            : data_{allocator, initial_capacity}
        {}

        ByteBuffer(const ByteBuffer&) = delete;
        ByteBuffer& operator=(const ByteBuffer&) = delete;
        ByteBuffer(ByteBuffer&& other) noexcept = default;
        ByteBuffer& operator=(ByteBuffer&& other) noexcept = default;

        [[nodiscard]] const uint8_t* Data() const noexcept { return data_.Data(); }
        [[nodiscard]] uint8_t* Data() noexcept { return data_.Data(); }
        [[nodiscard]] size_t Size() const noexcept { return data_.Size(); }
        [[nodiscard]] size_t Capacity() const noexcept { return data_.Capacity(); }
        [[nodiscard]] bool IsEmpty() const noexcept { return data_.IsEmpty(); }
        [[nodiscard]] comb::MemoryResource GetAllocator() const noexcept { return data_.GetAllocator(); }

        [[nodiscard]] ByteSpan View() const noexcept
        {
            return ByteSpan{data_.Data(), data_.Size()};
        }

        [[nodiscard]] ByteSpan View(size_t offset, size_t count) const noexcept
        {
            hive::Assert(offset + count <= data_.Size(), "ByteBuffer view out of bounds");
            return ByteSpan{data_.Data() + offset, count};
        }

        void Reserve(size_t capacity) noexcept { data_.Reserve(capacity); }
        void Resize(size_t new_size) noexcept { data_.Resize(new_size); }
        void Clear() noexcept { data_.Clear(); }

        void Append(const void* src, size_t size) noexcept
        {
            if (size == 0 || src == nullptr)
            {
                return;
            }

            const size_t old_size = data_.Size();
            data_.Resize(old_size + size);
            std::memcpy(data_.Data() + old_size, src, size);
        }

        void Append(ByteSpan view) noexcept
        {
            Append(view.Data(), view.Size());
        }

        void Append(uint8_t byte) noexcept
        {
            data_.PushBack(byte);
        }

        template<typename T>
        void Append(const T& value) noexcept
        {
            static_assert(std::is_trivially_copyable_v<T>, "T must be trivially copyable");
            Append(&value, sizeof(T));
        }

        [[nodiscard]] uint8_t operator[](size_t index) const noexcept
        {
            hive::Assert(index < data_.Size(), "ByteBuffer index out of bounds");
            return data_[index];
        }

        [[nodiscard]] uint8_t& operator[](size_t index) noexcept
        {
            hive::Assert(index < data_.Size(), "ByteBuffer index out of bounds");
            return data_[index];
        }

        [[nodiscard]] const uint8_t* begin() const noexcept { return data_.begin(); }
        [[nodiscard]] const uint8_t* end() const noexcept { return data_.end(); }
        [[nodiscard]] uint8_t* begin() noexcept { return data_.begin(); }
        [[nodiscard]] uint8_t* end() noexcept { return data_.end(); }

    private:
        wax::Vector<uint8_t> data_;
    };
}