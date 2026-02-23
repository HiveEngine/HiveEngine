#pragma once

#include <wax/serialization/byte_span.h>
#include <comb/default_allocator.h>

namespace nectar
{
    /// Per-type loading/unloading trait.
    /// Implementations must be thread-safe (for future async loading).
    template<typename T>
    class AssetLoader
    {
    public:
        virtual ~AssetLoader() = default;

        /// Load asset from raw bytes. Returns nullptr on failure.
        [[nodiscard]] virtual T* Load(wax::ByteSpan data, comb::DefaultAllocator& alloc) = 0;

        /// Free a previously loaded asset.
        virtual void Unload(T* asset, comb::DefaultAllocator& alloc) = 0;

        /// Approximate memory footprint of a loaded asset. Default: 0 (untracked).
        virtual size_t SizeOf([[maybe_unused]] const T* asset) const { return 0; }
    };
}
