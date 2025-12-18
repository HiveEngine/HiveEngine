#pragma once

#include <wax/serialization/byte_span.h>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <type_traits>
#include <hive/core/assert.h>

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
            : view_{}
            , position_{0}
        {}

        constexpr BinaryReader(const void* data, size_t size) noexcept
            : view_{static_cast<const uint8_t*>(data), size}
            , position_{0}
        {}

        constexpr BinaryReader(ByteSpan view) noexcept
            : view_{view}
            , position_{0}
        {}

        /**
         * Read a primitive type in little-endian format
         */
        template<typename T>
        [[nodiscard]] T Read() noexcept
        {
            static_assert(std::is_trivially_copyable_v<T>, "T must be trivially copyable");
            hive::Assert(position_ + sizeof(T) <= view_.Size(), "BinaryReader read out of bounds");

            T value;
            std::memcpy(&value, view_.Data() + position_, sizeof(T));
            position_ += sizeof(T);
            return value;
        }

        /**
         * Read a primitive type into an output parameter
         */
        template<typename T>
        void Read(T& out) noexcept
        {
            out = Read<T>();
        }

        /**
         * Try to read a primitive type, returns false if not enough data
         */
        template<typename T>
        [[nodiscard]] bool TryRead(T& out) noexcept
        {
            static_assert(std::is_trivially_copyable_v<T>, "T must be trivially copyable");

            if (position_ + sizeof(T) > view_.Size())
            {
                return false;
            }

            std::memcpy(&out, view_.Data() + position_, sizeof(T));
            position_ += sizeof(T);
            return true;
        }

        /**
         * Read raw bytes into a buffer
         */
        void ReadBytes(void* dest, size_t count) noexcept
        {
            hive::Assert(position_ + count <= view_.Size(), "BinaryReader read out of bounds");
            std::memcpy(dest, view_.Data() + position_, count);
            position_ += count;
        }

        /**
         * Read raw bytes as a ByteSpan (zero-copy)
         */
        [[nodiscard]] ByteSpan ReadBytes(size_t count) noexcept
        {
            hive::Assert(position_ + count <= view_.Size(), "BinaryReader read out of bounds");
            ByteSpan result{view_.Data() + position_, count};
            position_ += count;
            return result;
        }

        /**
         * Try to read raw bytes, returns empty span if not enough data
         */
        [[nodiscard]] bool TryReadBytes(size_t count, ByteSpan& out) noexcept
        {
            if (position_ + count > view_.Size())
            {
                out = ByteSpan{};
                return false;
            }

            out = ByteSpan{view_.Data() + position_, count};
            position_ += count;
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
            size_t start = position_;
            while (position_ < view_.Size() && view_[position_] != 0)
            {
                ++position_;
            }

            size_t length = position_ - start;
            if (position_ < view_.Size())
            {
                ++position_;  // Skip null terminator
                ++length;     // Include null in returned view
            }

            return ByteSpan{view_.Data() + start, length};
        }

        /**
         * Read variable-length integer (LEB128 unsigned)
         */
        [[nodiscard]] uint64_t ReadVarInt() noexcept
        {
            uint64_t result = 0;
            int shift = 0;

            while (position_ < view_.Size())
            {
                uint8_t byte = view_[position_++];
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

        /**
         * Try to read variable-length integer
         */
        [[nodiscard]] bool TryReadVarInt(uint64_t& out) noexcept
        {
            size_t start_pos = position_;
            uint64_t result = 0;
            int shift = 0;

            while (position_ < view_.Size())
            {
                uint8_t byte = view_[position_++];
                result |= static_cast<uint64_t>(byte & 0x7F) << shift;

                if ((byte & 0x80) == 0)
                {
                    out = result;
                    return true;
                }

                shift += 7;
                if (shift >= 64)
                {
                    position_ = start_pos;  // Rollback
                    return false;
                }
            }

            position_ = start_pos;  // Rollback
            return false;
        }

        /**
         * Read signed variable-length integer (ZigZag + LEB128)
         */
        [[nodiscard]] int64_t ReadVarIntSigned() noexcept
        {
            uint64_t encoded = ReadVarInt();
            // ZigZag decoding: (n >> 1) ^ -(n & 1)
            return static_cast<int64_t>((encoded >> 1) ^ (~(encoded & 1) + 1));
        }

        /**
         * Skip bytes without reading
         */
        void Skip(size_t count) noexcept
        {
            hive::Assert(position_ + count <= view_.Size(), "BinaryReader skip out of bounds");
            position_ += count;
        }

        /**
         * Try to skip bytes
         */
        [[nodiscard]] bool TrySkip(size_t count) noexcept
        {
            if (position_ + count > view_.Size())
            {
                return false;
            }
            position_ += count;
            return true;
        }

        /**
         * Seek to an absolute position
         */
        void Seek(size_t position) noexcept
        {
            hive::Assert(position <= view_.Size(), "BinaryReader seek out of bounds");
            position_ = position;
        }

        /**
         * Get current position
         */
        [[nodiscard]] constexpr size_t Position() const noexcept
        {
            return position_;
        }

        /**
         * Get remaining bytes
         */
        [[nodiscard]] constexpr size_t Remaining() const noexcept
        {
            return view_.Size() - position_;
        }

        /**
         * Get total size
         */
        [[nodiscard]] constexpr size_t Size() const noexcept
        {
            return view_.Size();
        }

        /**
         * Check if at end of data
         */
        [[nodiscard]] constexpr bool IsEof() const noexcept
        {
            return position_ >= view_.Size();
        }

        /**
         * Get underlying buffer view
         */
        [[nodiscard]] constexpr ByteSpan View() const noexcept
        {
            return view_;
        }

        /**
         * Get remaining data as ByteSpan
         */
        [[nodiscard]] constexpr ByteSpan RemainingView() const noexcept
        {
            return view_.Subspan(position_);
        }

        /**
         * Peek at next byte without advancing
         */
        [[nodiscard]] uint8_t Peek() const noexcept
        {
            hive::Assert(!IsEof(), "BinaryReader peek at EOF");
            return view_[position_];
        }

        /**
         * Try to peek at next byte
         */
        [[nodiscard]] bool TryPeek(uint8_t& out) const noexcept
        {
            if (IsEof())
            {
                return false;
            }
            out = view_[position_];
            return true;
        }

    private:
        ByteSpan view_;
        size_t position_;
    };

}
