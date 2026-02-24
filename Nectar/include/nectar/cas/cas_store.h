#pragma once

#include <nectar/core/content_hash.h>
#include <wax/serialization/byte_buffer.h>
#include <wax/serialization/byte_span.h>
#include <wax/containers/string.h>
#include <wax/containers/string_view.h>
#include <comb/default_allocator.h>

namespace nectar
{
    /// Content-Addressable Store.
    /// Stores immutable blobs on disk, identified by their ContentHash.
    /// Uses 2-level directory sharding: hash "7f3a8b..." -> root/7f/3a/7f3a8b...
    class CasStore
    {
    public:
        CasStore(comb::DefaultAllocator& alloc, wax::StringView root_dir);

        /// Store a blob. Returns its ContentHash.
        /// If the blob already exists (same hash), this is a no-op.
        [[nodiscard]] ContentHash Store(wax::ByteSpan data);

        /// Load a blob by hash. Returns empty buffer if not found.
        [[nodiscard]] wax::ByteBuffer<> Load(ContentHash hash);

        /// Check if a blob exists.
        [[nodiscard]] bool Contains(ContentHash hash) const;

        /// Remove a blob (for GC). Returns false if not found.
        bool Remove(ContentHash hash);

        [[nodiscard]] wax::StringView RootDir() const noexcept;

    private:
        void BuildBlobPath(ContentHash hash, wax::String<>& out) const;
        void EnsureDirectoryExists(wax::StringView dir_path) const;

        comb::DefaultAllocator* alloc_;
        wax::String<> root_dir_;
    };
}
