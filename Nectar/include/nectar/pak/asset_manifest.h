#pragma once

#include <nectar/core/content_hash.h>
#include <wax/containers/hash_map.h>
#include <wax/containers/string.h>
#include <wax/containers/string_view.h>
#include <wax/serialization/byte_buffer.h>
#include <wax/serialization/byte_span.h>
#include <comb/default_allocator.h>

namespace nectar
{
    /// Maps VFS paths to ContentHash.
    /// Embedded inside .npak at the sentinel hash for runtime path resolution.
    class AssetManifest
    {
    public:
        explicit AssetManifest(comb::DefaultAllocator& alloc);

        void Add(wax::StringView vfs_path, ContentHash hash);

        [[nodiscard]] const ContentHash* Find(wax::StringView vfs_path) const;

        [[nodiscard]] size_t Count() const noexcept;

        /// Iterate all entries: callback(StringView path, ContentHash hash)
        template<typename F>
        void ForEach(F&& fn) const
        {
            for (auto it = entries_.begin(); it != entries_.end(); ++it)
                fn(wax::StringView{it.Key().CStr(), it.Key().Size()}, it.Value());
        }

        /// Serialize to binary for embedding in .npak.
        [[nodiscard]] wax::ByteBuffer<> Serialize(comb::DefaultAllocator& alloc) const;

        /// Deserialize from binary.
        [[nodiscard]] static AssetManifest Deserialize(
            wax::ByteSpan data, comb::DefaultAllocator& alloc);

    private:
        comb::DefaultAllocator* alloc_;
        wax::HashMap<wax::String<>, ContentHash> entries_;
    };
}
