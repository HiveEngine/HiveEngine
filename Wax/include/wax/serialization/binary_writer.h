#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <type_traits>
#include <comb/memory_resource.h>
#include <wax/serialization/byte_buffer.h>
#include <wax/serialization/byte_span.h>

namespace wax
{
    class BinaryWriter
    {
    public:
        BinaryWriter() noexcept
            : buffer_{}
        {}

        explicit BinaryWriter(comb::MemoryResource allocator) noexcept
            : buffer_{allocator}
        {}

        template<comb::Allocator Allocator>
        explicit BinaryWriter(Allocator& allocator) noexcept
            : buffer_{allocator}
        {}

        BinaryWriter(comb::MemoryResource allocator, size_t initial_capacity) noexcept
            : buffer_{allocator, initial_capacity}
        {}

        template<comb::Allocator Allocator>
        BinaryWriter(Allocator& allocator, size_t initial_capacity) noexcept
            : buffer_{allocator, initial_capacity}
        {}

        BinaryWriter(const BinaryWriter&) = delete;
        BinaryWriter& operator=(const BinaryWriter&) = delete;
        BinaryWriter(BinaryWriter&&) noexcept = default;
        BinaryWriter& operator=(BinaryWriter&&) noexcept = default;

        template<typename T>
        void Write(T value) noexcept
        {
            static_assert(std::is_trivially_copyable_v<T>, "T must be trivially copyable");
            static_assert(std::is_arithmetic_v<T> || std::is_enum_v<T>, "Use WriteBytes for non-primitive types");

            uint8_t bytes[sizeof(T)];
            std::memcpy(bytes, &value, sizeof(T));
            buffer_.Append(bytes, sizeof(T));
        }

        void WriteBytes(const void* data, size_t size) noexcept
        {
            buffer_.Append(data, size);
        }

        void WriteBytes(ByteSpan view) noexcept
        {
            buffer_.Append(view);
        }

        void WriteString(const char* str) noexcept
        {
            const size_t len = str ? std::strlen(str) : 0;
            Write<uint32_t>(static_cast<uint32_t>(len));
            if (len > 0)
            {
                buffer_.Append(str, len);
            }
        }

        void WriteString(const char* str, size_t length) noexcept
        {
            Write<uint32_t>(static_cast<uint32_t>(length));
            if (length > 0 && str != nullptr)
            {
                buffer_.Append(str, length);
            }
        }

        void WriteStringZ(const char* str) noexcept
        {
            if (str)
            {
                const size_t len = std::strlen(str);
                buffer_.Append(str, len + 1);
            }
            else
            {
                buffer_.Append(uint8_t{0});
            }
        }

        void WriteVarInt(uint64_t value) noexcept
        {
            do
            {
                uint8_t byte = value & 0x7F;
                value >>= 7;
                if (value != 0)
                {
                    byte |= 0x80;
                }
                buffer_.Append(byte);
            } while (value != 0);
        }

        void WriteVarIntSigned(int64_t value) noexcept
        {
            const uint64_t encoded = static_cast<uint64_t>((value << 1) ^ (value >> 63));
            WriteVarInt(encoded);
        }

        void WritePadding(size_t count) noexcept
        {
            for (size_t i = 0; i < count; ++i)
            {
                buffer_.Append(uint8_t{0});
            }
        }

        void WriteAlignment(size_t alignment) noexcept
        {
            const size_t pos = buffer_.Size();
            const size_t padding = (alignment - (pos % alignment)) % alignment;
            WritePadding(padding);
        }

        void Reserve(size_t capacity) noexcept { buffer_.Reserve(capacity); }
        void Clear() noexcept { buffer_.Clear(); }
        [[nodiscard]] size_t Size() const noexcept { return buffer_.Size(); }
        [[nodiscard]] bool IsEmpty() const noexcept { return buffer_.IsEmpty(); }
        [[nodiscard]] ByteSpan View() const noexcept { return buffer_.View(); }
        [[nodiscard]] const uint8_t* Data() const noexcept { return buffer_.Data(); }
        [[nodiscard]] comb::MemoryResource GetAllocator() const noexcept { return buffer_.GetAllocator(); }
        [[nodiscard]] ByteBuffer& GetBuffer() noexcept { return buffer_; }
        [[nodiscard]] const ByteBuffer& GetBuffer() const noexcept { return buffer_; }

    private:
        ByteBuffer buffer_;
    };
}