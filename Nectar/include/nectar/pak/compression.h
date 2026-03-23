#pragma once

#include <hive/hive_config.h>

#include <comb/default_allocator.h>

#include <wax/serialization/byte_buffer.h>
#include <wax/serialization/byte_span.h>

#include <nectar/pak/npak_format.h>

namespace nectar
{
    /// Compress data with the given method.
    /// Returns empty buffer if compression fails or produces larger output than input.
    [[nodiscard]] HIVE_API wax::ByteBuffer Compress(wax::ByteSpan input, CompressionMethod method,
                                                    comb::DefaultAllocator& alloc);

    /// Decompress data. uncompressed_size must match the original data size.
    /// Returns empty buffer on failure.
    [[nodiscard]] HIVE_API wax::ByteBuffer Decompress(wax::ByteSpan compressed, size_t uncompressedSize,
                                                      CompressionMethod method, comb::DefaultAllocator& alloc);
} // namespace nectar
