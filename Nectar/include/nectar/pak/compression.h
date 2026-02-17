#pragma once

#include <nectar/pak/npak_format.h>
#include <wax/serialization/byte_buffer.h>
#include <wax/serialization/byte_span.h>
#include <comb/default_allocator.h>

namespace nectar
{
    /// Compress data with the given method.
    /// Returns empty buffer if compression fails or produces larger output than input.
    [[nodiscard]] wax::ByteBuffer<> Compress(
        wax::ByteSpan input,
        CompressionMethod method,
        comb::DefaultAllocator& alloc);

    /// Decompress data. uncompressed_size must match the original data size.
    /// Returns empty buffer on failure.
    [[nodiscard]] wax::ByteBuffer<> Decompress(
        wax::ByteSpan compressed,
        size_t uncompressed_size,
        CompressionMethod method,
        comb::DefaultAllocator& alloc);
}
