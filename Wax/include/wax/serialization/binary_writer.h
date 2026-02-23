#pragma once

#include <wax/serialization/byte_buffer.h>
#include <wax/serialization/byte_span.h>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <type_traits>

namespace wax
{
    /**
     * Binary serialization writer with little-endian encoding
     *
     * BinaryWriter provides methods for writing primitive types and raw bytes
     * to an internal buffer. All multi-byte values are written in little-endian
     * format for cross-platform compatibility.
     *
     * Performance characteristics:
     * - Storage: Buffer overhead + written data
     * - Write: Amortized O(1) with buffer growth
     * - GetBuffer: O(1) - returns view
     * - VarInt: O(1) - max 10 bytes
     *
     * Limitations:
     * - Requires allocator for internal buffer
     * - Little-endian only (no big-endian support)
     * - Not thread-safe
     *
     * Use cases:
     * - Asset serialization
     * - Network packet building
     * - Binary file format writing
     * - Configuration serialization
     *
     * Example:
     * @code
     *   comb::LinearAllocator alloc{4096};
     *   wax::BinaryWriter<comb::LinearAllocator> writer{alloc};
     *
     *   writer.Write<uint32_t>(0x12345678);  // Magic
     *   writer.Write<uint16_t>(1);           // Version
     *   writer.WriteString("Hello");
     *   writer.WriteVarInt(12345);
     *
     *   wax::ByteSpan data = writer.View();
     * @endcode
     *
     * @tparam Allocator Comb allocator type
     */
    template<typename Allocator>
    class BinaryWriter
    {
    public:
        explicit BinaryWriter(Allocator& allocator) noexcept
            : buffer_{allocator}
        {}

        BinaryWriter(Allocator& allocator, size_t initial_capacity) noexcept
            : buffer_{allocator, initial_capacity}
        {}

        BinaryWriter(const BinaryWriter&) = delete;
        BinaryWriter& operator=(const BinaryWriter&) = delete;

        BinaryWriter(BinaryWriter&&) noexcept = default;
        BinaryWriter& operator=(BinaryWriter&&) noexcept = default;

        /**
         * Write a primitive type in little-endian format
         */
        template<typename T>
        void Write(T value) noexcept
        {
            static_assert(std::is_trivially_copyable_v<T>, "T must be trivially copyable");
            static_assert(std::is_arithmetic_v<T> || std::is_enum_v<T>,
                          "Use WriteBytes for non-primitive types");

            // Little-endian write
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

        /**
         * Write a length-prefixed string (uint32 length + chars)
         */
        void WriteString(const char* str) noexcept
        {
            size_t len = str ? std::strlen(str) : 0;
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
                size_t len = std::strlen(str);
                buffer_.Append(str, len + 1);
            }
            else
            {
                buffer_.Append(uint8_t{0});
            }
        }

        /**
         * Write variable-length integer (LEB128 unsigned)
         *
         * Encodes integers in 1-10 bytes depending on value.
         * Smaller values use fewer bytes.
         */
        void WriteVarInt(uint64_t value) noexcept
        {
            do
            {
                uint8_t byte = value & 0x7F;
                value >>= 7;
                if (value != 0)
                {
                    byte |= 0x80;  // More bytes follow
                }
                buffer_.Append(byte);
            } while (value != 0);
        }

        /**
         * Write signed variable-length integer (ZigZag + LEB128)
         */
        void WriteVarIntSigned(int64_t value) noexcept
        {
            // ZigZag encoding: (n << 1) ^ (n >> 63)
            uint64_t encoded = static_cast<uint64_t>((value << 1) ^ (value >> 63));
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
            size_t pos = buffer_.Size();
            size_t padding = (alignment - (pos % alignment)) % alignment;
            WritePadding(padding);
        }

        void Reserve(size_t capacity) noexcept
        {
            buffer_.Reserve(capacity);
        }

        void Clear() noexcept
        {
            buffer_.Clear();
        }

        [[nodiscard]] size_t Size() const noexcept
        {
            return buffer_.Size();
        }

        [[nodiscard]] bool IsEmpty() const noexcept
        {
            return buffer_.IsEmpty();
        }

        [[nodiscard]] ByteSpan View() const noexcept
        {
            return buffer_.View();
        }

        [[nodiscard]] const uint8_t* Data() const noexcept
        {
            return buffer_.Data();
        }

        [[nodiscard]] ByteBuffer<Allocator>& GetBuffer() noexcept
        {
            return buffer_;
        }

        [[nodiscard]] const ByteBuffer<Allocator>& GetBuffer() const noexcept
        {
            return buffer_;
        }

    private:
        ByteBuffer<Allocator> buffer_;
    };

}
