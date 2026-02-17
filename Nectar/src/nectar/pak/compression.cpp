#include <nectar/pak/compression.h>
#include <lz4.h>
#include <zstd.h>
#include <cstring>

namespace nectar
{
    wax::ByteBuffer<> Compress(wax::ByteSpan input,
                                CompressionMethod method,
                                comb::DefaultAllocator& alloc)
    {
        wax::ByteBuffer<> result{alloc};

        if (input.Size() == 0)
            return result;

        if (method == CompressionMethod::None)
        {
            result.Resize(input.Size());
            std::memcpy(result.Data(), input.Data(), input.Size());
            return result;
        }

        if (method == CompressionMethod::LZ4)
        {
            int src_size = static_cast<int>(input.Size());
            int bound = LZ4_compressBound(src_size);
            if (bound <= 0) return result;

            result.Resize(static_cast<size_t>(bound));
            int compressed = LZ4_compress_default(
                reinterpret_cast<const char*>(input.Data()),
                reinterpret_cast<char*>(result.Data()),
                src_size,
                bound);

            if (compressed <= 0 || static_cast<size_t>(compressed) >= input.Size())
            {
                result.Clear();
                return result;
            }
            result.Resize(static_cast<size_t>(compressed));
            return result;
        }

        if (method == CompressionMethod::Zstd)
        {
            size_t bound = ZSTD_compressBound(input.Size());
            if (ZSTD_isError(bound))
                return result;

            result.Resize(bound);
            size_t compressed = ZSTD_compress(
                result.Data(), bound,
                input.Data(), input.Size(),
                3); // level 3

            if (ZSTD_isError(compressed) || compressed >= input.Size())
            {
                result.Clear();
                return result;
            }
            result.Resize(compressed);
            return result;
        }

        return result;
    }

    wax::ByteBuffer<> Decompress(wax::ByteSpan compressed,
                                  size_t uncompressed_size,
                                  CompressionMethod method,
                                  comb::DefaultAllocator& alloc)
    {
        wax::ByteBuffer<> result{alloc};

        if (compressed.Size() == 0 || uncompressed_size == 0)
            return result;

        if (method == CompressionMethod::None)
        {
            result.Resize(compressed.Size());
            std::memcpy(result.Data(), compressed.Data(), compressed.Size());
            return result;
        }

        result.Resize(uncompressed_size);

        if (method == CompressionMethod::LZ4)
        {
            int decompressed = LZ4_decompress_safe(
                reinterpret_cast<const char*>(compressed.Data()),
                reinterpret_cast<char*>(result.Data()),
                static_cast<int>(compressed.Size()),
                static_cast<int>(uncompressed_size));

            if (decompressed < 0 || static_cast<size_t>(decompressed) != uncompressed_size)
            {
                result.Clear();
                return result;
            }
            return result;
        }

        if (method == CompressionMethod::Zstd)
        {
            size_t decompressed = ZSTD_decompress(
                result.Data(), uncompressed_size,
                compressed.Data(), compressed.Size());

            if (ZSTD_isError(decompressed) || decompressed != uncompressed_size)
            {
                result.Clear();
                return result;
            }
            return result;
        }

        result.Clear();
        return result;
    }
}
