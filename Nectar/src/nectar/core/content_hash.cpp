#include <nectar/core/content_hash.h>

namespace nectar
{
    // FNV-1a 64-bit constants
    static constexpr uint64_t kFnvBasis = 14695981039346656037ULL;
    static constexpr uint64_t kFnvPrime = 1099511628211ULL;

    static uint64_t Fnv1a64(const uint8_t* data, size_t size, uint64_t seed) noexcept
    {
        uint64_t hash = seed;
        for (size_t i = 0; i < size; ++i)
        {
            hash ^= static_cast<uint64_t>(data[i]);
            hash *= kFnvPrime;
        }
        return hash;
    }

    ContentHash ContentHash::FromData(const void* data, size_t size) noexcept
    {
        if (data == nullptr || size == 0)
        {
            // Deterministic hash for empty data (not Invalid — empty is valid content)
            return ContentHash{kFnvBasis, kFnvBasis ^ kFnvPrime};
        }

        auto* bytes = static_cast<const uint8_t*>(data);

        // Two FNV-1a passes with different seeds for 128-bit output
        uint64_t high = Fnv1a64(bytes, size, kFnvBasis);
        uint64_t low = Fnv1a64(bytes, size, kFnvBasis ^ 0xFF51AFD7ED558CCDULL);

        return ContentHash{high, low};
    }
}
