#pragma once

#include <cstddef>
#include <cstdint>

namespace nectar
{
    /// CRC32 (IEEE 802.3 polynomial) for TOC integrity checking.
    [[nodiscard]] uint32_t Crc32(const void* data, size_t size) noexcept;

    /// Incremental CRC32. Start with crc = 0xFFFFFFFF, finalize with ^ 0xFFFFFFFF.
    [[nodiscard]] uint32_t Crc32Update(uint32_t crc, const void* data, size_t size) noexcept;
}
