#include <nectar/pak/crc32.h>

namespace nectar
{
    namespace
    {
        constexpr uint32_t ComputeCrc32Entry(uint32_t index) noexcept
        {
            uint32_t crc = index;
            for (int j = 0; j < 8; ++j)
            {
                if (crc & 1)
                    crc = 0xEDB88320 ^ (crc >> 1);
                else
                    crc >>= 1;
            }
            return crc;
        }

        struct Crc32Table
        {
            uint32_t entries[256];

            constexpr Crc32Table() noexcept : entries{}
            {
                for (uint32_t i = 0; i < 256; ++i)
                    entries[i] = ComputeCrc32Entry(i);
            }
        };

        constexpr Crc32Table kTable{};
    }

    uint32_t Crc32Update(uint32_t crc, const void* data, size_t size) noexcept
    {
        auto* bytes = static_cast<const uint8_t*>(data);
        for (size_t i = 0; i < size; ++i)
            crc = kTable.entries[(crc ^ bytes[i]) & 0xFF] ^ (crc >> 8);
        return crc;
    }

    uint32_t Crc32(const void* data, size_t size) noexcept
    {
        return Crc32Update(0xFFFFFFFF, data, size) ^ 0xFFFFFFFF;
    }
}
