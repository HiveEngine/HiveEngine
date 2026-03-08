#pragma once

#include <hive/core/assert.h>

#include <wax/serialization/byte_span.h>

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <type_traits>

namespace wax
{
    /**
     * Binary deserialization reader with little-endian decoding
     *
     * BinaryReader provides methods for reading primitive types and raw bytes
     * from a ByteSpan. All multi-byte values are read in little-endian format.
     * The reader maintains a position cursor that advances with each read.
     *
     * Performance characteristics:
     * - Storage: 24 bytes (ByteSpan + position)
     * - Read: O(1) - memcpy
     * - Seek/Skip: O(1)
     * - VarInt: O(1) - max 10 iterations
     *
     * Limitations:
     * - Non-owning (source buffer must stay alive)
     * - Little-endian only
     * - Not thread-safe
     * - No automatic bounds checking in release (use TryRead for safety)
     *
     * Use cases:
     * - Asset deserialization
     * - Network packet parsing
     * - Binary file format reading
     * - Configuration loading
     *
     * Example:
     * @code
     *   uint8_t data[] = {0x78, 0x56, 0x34, 0x12, 0x01, 0x00};
     *   wax::BinaryReader reader{data, sizeof(data)};
     *
     *   uint32_t magic = reader.Read<uint32_t>();  // 0x12345678
     *   uint16_t version = reader.Read<uint16_t>(); // 1
     *   bool eof = reader.IsEof();  // true
     * @endcode
     */
    class BinaryReader
    {
    public:
        constexpr BinaryReader() noexcept
            : m_view{}
            , m_position{0}
        {
        }

        constexpr BinaryReader(const void* data, size_t size) noexcept
            : m_view{static_cast<const uint8_t*>(data), size}
            , m_position{0}
        {
        }

        constexpr BinaryReader(ByteSpan view) noexcept
            : m_view{view}
            , m_position{0}
        {
        }

        /**
         * Read a primitive type in little-endian format
         */
        template <typename T> [[nodiscard]] T Read() noexcept
        {
            static_assert(std::is_trivially_copyable_v<T>, "T must be trivially copyable");
            hive::Assert(m_position + sizeof(T) <= m_view.Size(), "BinaryReader read out of bounds");

            T value;
            std::memcpy(&value, m_view.Data() + m_position, sizeof(T));
            m_position += sizeof(T);
            return value;
        }

        template <typename T> void Read(T& out) noexcept
        {
            out = Read<T>();
        }

        template <typename T> [[nodiscard]] bool TryRead(T& out) noexcept
        {
            static_assert(std::is_trivially_copyable_v<T>, "T must be trivially copyable");

            if (m_position + sizeof(T) > m_view.Size())
            {
                return false;
            }

            std::memcpy(&out, m_view.Data() + m_position, sizeof(T));
            m_position += sizeof(T);
            return true;
        }

        void ReadBytes(void* dest, size_t count) noexcept
        {
            hive::Assert(m_position + count <= m_view.Size(), "BinaryReader read out of bounds");
            std::memcpy(dest, m_view.Data() + m_position, count);
            m_position += count;
        }

        /** Zero-copy */
        [[nodiscard]] ByteSpan ReadBytes(size_t count) noexcept
        {
            hive::Assert(m_position + count <= m_view.Size(), "BinaryReader read out of bounds");
            ByteSpan result{m_view.Data() + m_position, count};
            m_position += count;
            return result;
        }

        [[nodiscard]] bool TryReadBytes(size_t count, ByteSpan& out) noexcept
        {
            if (m_position + count > m_view.Size())
            {
                out = ByteSpan{};
                return false;
            }

            out = ByteSpan{m_view.Data() + m_position, count};
            m_position += count;
            return true;
        }

        /**
         * Read a length-prefixed string (uint32 length + chars)
         *
         * Returns a view into the original buffer (zero-copy).
         */
        [[nodiscard]] ByteSpan ReadString() noexcept
        {
            uint32_t length = Read<uint32_t>();
            return ReadBytes(length);
        }

        /**
         * Read a null-terminated string
         *
         * Returns a view including the null terminator.
         */
        [[nodiscard]] ByteSpan ReadStringZ() noexcept
        {
            size_t start = m_position;
            while (m_position < m_view.Size() && m_view[m_position] != 0)
            {
                ++m_position;
            }

            size_t length = m_position - start;
            if (m_position < m_view.Size())
            {
                ++m_position; // Skip null terminator
                ++length;     // Include null in returned view
            }

            return ByteSpan{m_view.Data() + start, length};
        }

        /**
         * Read variable-length integer (LEB128 unsigned)
         */
        [[nodiscard]] uint64_t ReadVarInt() noexcept
        {
            uint64_t result = 0;
            int shift = 0;

            while (m_position < m_view.Size())
            {
                uint8_t byte = m_view[m_position++];
                result |= static_cast<uint64_t>(byte & 0x7F) << shift;

                if ((byte & 0x80) == 0)
                {
                    break;
                }

                shift += 7;
                hive::Assert(shift < 64, "VarInt overflow");
            }

            return result;
        }

        [[nodiscard]] bool TryReadVarInt(uint64_t& out) noexcept
        {
            size_t startPos = m_position;
            uint64_t result = 0;
            int shift = 0;

            while (m_position < m_view.Size())
            {
                uint8_t byte = m_view[m_position++];
                result |= static_cast<uint64_t>(byte & 0x7F) << shift;

                if ((byte & 0x80) == 0)
                {
                    out = result;
                    return true;
                }

                shift += 7;
                if (shift >= 64)
                {
                    m_position = startPos; // Rollback
                    return false;
                }
            }

            m_position = startPos; // Rollback
            return false;
        }

        /**
         * Read signed variable-length integer (ZigZag + LEB128)
         */
        [[nodiscard]] int64_t ReadVarIntSigned() noexcept
        {
            uint64_t encoded = ReadVarInt();
            // ZigZag decoding: (n >> 1) ^ -(n & 1)
            return static_cast<int64_t>((encoded >> 1) ^ -(encoded & 1));
        }

        void Skip(size_t count) noexcept
        {
            hive::Assert(m_position + count <= m_view.Size(), "BinaryReader skip out of bounds");
            m_position += count;
        }

        [[nodiscard]] bool TrySkip(size_t count) noexcept
        {
            if (m_position + count > m_view.Size())
            {
                return false;
            }
            m_position += count;
            return true;
        }

        void Seek(size_t position) noexcept
        {
            hive::Assert(position <= m_view.Size(), "BinaryReader seek out of bounds");
            m_position = position;
        }

        [[nodiscard]] constexpr size_t Position() const noexcept
        {
            return m_position;
        }

        [[nodiscard]] constexpr size_t Remaining() const noexcept
        {
            return m_view.Size() - m_position;
        }

        [[nodiscard]] constexpr size_t Size() const noexcept
        {
            return m_view.Size();
        }

        [[nodiscard]] constexpr bool IsEof() const noexcept
        {
            return m_position >= m_view.Size();
        }

        [[nodiscard]] constexpr ByteSpan View() const noexcept
        {
            return m_view;
        }

        [[nodiscard]] constexpr ByteSpan RemainingView() const noexcept
        {
            return m_view.Subspan(m_position);
        }

        /** Does not advance position */
        [[nodiscard]] uint8_t Peek() const noexcept
        {
            hive::Assert(!IsEof(), "BinaryReader peek at EOF");
            return m_view[m_position];
        }

        [[nodiscard]] bool TryPeek(uint8_t& out) const noexcept
        {
            if (IsEof())
            {
                return false;
            }
            out = m_view[m_position];
            return true;
        }

    private:
        ByteSpan m_view;
        size_t m_position;
    };

} // namespace wax
