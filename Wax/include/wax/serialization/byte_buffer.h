#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <hive/core/assert.h>
#include <comb/default_allocator.h>
#include <wax/serialization/byte_span.h>
#include <wax/containers/vector.h>

namespace wax
{
    /**
     * Owning binary data buffer
     *
     * ByteBuffer provides a growable container for binary data using a Comb allocator.
     * It wraps wax::Vector<uint8_t> and adds convenience methods for binary I/O.
     *
     * Performance characteristics:
     * - Storage: Vector overhead + data
     * - Append: Amortized O(1) with growth
     * - Reserve: O(n) copy on reallocation
     * - Access: O(1) - direct pointer
     *
     * Limitations:
     * - Requires allocator (not default constructible)
     * - Growth may cause reallocation and copy
     * - Not thread-safe
     *
     * Use cases:
     * - Loading file contents
     * - Building binary data for serialization
     * - Asset data storage
     * - Network packet buffers
     *
     * Example:
     * @code
     *   comb::LinearAllocator alloc{1024};
     *   wax::ByteBuffer buffer{alloc};
     *
     *   buffer.Reserve(256);
     *   buffer.Append("MAGIC", 5);
     *   buffer.Append<uint32_t>(42);
     *
     *   wax::ByteSpan view = buffer.View();
     * @endcode
     *
     * @tparam Allocator Comb allocator type
     */
    template<typename Allocator = comb::DefaultAllocator>
    class ByteBuffer
    {
    public:
        // Default constructor - uses global default allocator
        ByteBuffer() noexcept
            requires std::is_same_v<Allocator, comb::DefaultAllocator>
            : data_{comb::GetDefaultAllocator()}
        {}

        // Default constructor with capacity - uses global default allocator
        explicit ByteBuffer(size_t initial_capacity) noexcept
            requires std::is_same_v<Allocator, comb::DefaultAllocator>
            : data_{comb::GetDefaultAllocator(), initial_capacity}
        {}

        // Constructor with allocator
        explicit ByteBuffer(Allocator& allocator) noexcept
            : data_{allocator}
        {}

        ByteBuffer(Allocator& allocator, size_t initial_capacity) noexcept
            : data_{allocator, initial_capacity}
        {}

        ByteBuffer(const ByteBuffer&) = delete;
        ByteBuffer& operator=(const ByteBuffer&) = delete;

        ByteBuffer(ByteBuffer&& other) noexcept = default;
        ByteBuffer& operator=(ByteBuffer&& other) noexcept = default;

        [[nodiscard]] const uint8_t* Data() const noexcept
        {
            return data_.Data();
        }

        [[nodiscard]] uint8_t* Data() noexcept
        {
            return data_.Data();
        }

        [[nodiscard]] size_t Size() const noexcept
        {
            return data_.Size();
        }

        [[nodiscard]] size_t Capacity() const noexcept
        {
            return data_.Capacity();
        }

        [[nodiscard]] bool IsEmpty() const noexcept
        {
            return data_.IsEmpty();
        }

        [[nodiscard]] ByteSpan View() const noexcept
        {
            return ByteSpan{data_.Data(), data_.Size()};
        }

        [[nodiscard]] ByteSpan View(size_t offset, size_t count) const noexcept
        {
            hive::Assert(offset + count <= data_.Size(), "ByteBuffer view out of bounds");
            return ByteSpan{data_.Data() + offset, count};
        }

        void Reserve(size_t capacity) noexcept
        {
            data_.Reserve(capacity);
        }

        void Resize(size_t new_size) noexcept
        {
            data_.Resize(new_size);
        }

        void Clear() noexcept
        {
            data_.Clear();
        }

        void Append(const void* src, size_t size) noexcept
        {
            if (size == 0 || src == nullptr)
            {
                return;
            }

            size_t old_size = data_.Size();
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

        [[nodiscard]] const uint8_t* begin() const noexcept
        {
            return data_.begin();
        }

        [[nodiscard]] const uint8_t* end() const noexcept
        {
            return data_.end();
        }

        [[nodiscard]] uint8_t* begin() noexcept
        {
            return data_.begin();
        }

        [[nodiscard]] uint8_t* end() noexcept
        {
            return data_.end();
        }

    private:
        wax::Vector<uint8_t, Allocator> data_;
    };

}
