#pragma once

#include <comb/memory_resource.h>

#include <wax/serialization/byte_buffer.h>
#include <wax/serialization/byte_span.h>

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <type_traits>

namespace wax
{
    class BinaryWriter
    {
    public:
        BinaryWriter() noexcept
            : m_buffer{} {}

        explicit BinaryWriter(comb::MemoryResource allocator) noexcept
            : m_buffer{allocator} {}

        template <comb::Allocator Allocator>
        explicit BinaryWriter(Allocator& allocator) noexcept
            : m_buffer{allocator} {}

        BinaryWriter(comb::MemoryResource allocator, size_t initialCapacity) noexcept
            : m_buffer{allocator, initialCapacity} {}

        template <comb::Allocator Allocator>
        BinaryWriter(Allocator& allocator, size_t initialCapacity) noexcept
            : m_buffer{allocator, initialCapacity} {}

        BinaryWriter(const BinaryWriter&) = delete;
        BinaryWriter& operator=(const BinaryWriter&) = delete;
        BinaryWriter(BinaryWriter&&) noexcept = default;
        BinaryWriter& operator=(BinaryWriter&&) noexcept = default;

        template <typename T> void Write(T value) noexcept {
            static_assert(std::is_trivially_copyable_v<T>, "T must be trivially copyable");
            static_assert(std::is_arithmetic_v<T> || std::is_enum_v<T>, "Use WriteBytes for non-primitive types");

            uint8_t bytes[sizeof(T)];
            std::memcpy(bytes, &value, sizeof(T));
            m_buffer.Append(bytes, sizeof(T));
        }

        void WriteBytes(const void* data, size_t size) noexcept { m_buffer.Append(data, size); }

        void WriteBytes(ByteSpan view) noexcept { m_buffer.Append(view); }

        void WriteString(const char* str) noexcept {
            const size_t len = str ? std::strlen(str) : 0;
            Write<uint32_t>(static_cast<uint32_t>(len));
            if (len > 0)
            {
                m_buffer.Append(str, len);
            }
        }

        void WriteString(const char* str, size_t length) noexcept {
            Write<uint32_t>(static_cast<uint32_t>(length));
            if (length > 0 && str != nullptr)
            {
                m_buffer.Append(str, length);
            }
        }

        void WriteStringZ(const char* str) noexcept {
            if (str)
            {
                const size_t len = std::strlen(str);
                m_buffer.Append(str, len + 1);
            }
            else
            {
                m_buffer.Append(uint8_t{0});
            }
        }

        void WriteVarInt(uint64_t value) noexcept {
            do
            {
                uint8_t byte = value & 0x7F;
                value >>= 7;
                if (value != 0)
                {
                    byte |= 0x80;
                }
                m_buffer.Append(byte);
            } while (value != 0);
        }

        void WriteVarIntSigned(int64_t value) noexcept {
            const uint64_t encoded = static_cast<uint64_t>((value << 1) ^ (value >> 63));
            WriteVarInt(encoded);
        }

        void WritePadding(size_t count) noexcept {
            for (size_t i = 0; i < count; ++i)
            {
                m_buffer.Append(uint8_t{0});
            }
        }

        void WriteAlignment(size_t alignment) noexcept {
            const size_t pos = m_buffer.Size();
            const size_t padding = (alignment - (pos % alignment)) % alignment;
            WritePadding(padding);
        }

        void Reserve(size_t capacity) noexcept { m_buffer.Reserve(capacity); }
        void Clear() noexcept { m_buffer.Clear(); }
        [[nodiscard]] size_t Size() const noexcept { return m_buffer.Size(); }
        [[nodiscard]] bool IsEmpty() const noexcept { return m_buffer.IsEmpty(); }
        [[nodiscard]] ByteSpan View() const noexcept { return m_buffer.View(); }
        [[nodiscard]] const uint8_t* Data() const noexcept { return m_buffer.Data(); }
        [[nodiscard]] comb::MemoryResource GetAllocator() const noexcept { return m_buffer.GetAllocator(); }
        [[nodiscard]] ByteBuffer& GetBuffer() noexcept { return m_buffer; }
        [[nodiscard]] const ByteBuffer& GetBuffer() const noexcept { return m_buffer; }

    private:
        ByteBuffer m_buffer;
    };
} // namespace wax