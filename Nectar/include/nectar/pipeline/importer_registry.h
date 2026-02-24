#pragma once

#include <nectar/pipeline/i_asset_importer.h>
#include <wax/containers/hash_map.h>
#include <wax/containers/string.h>
#include <wax/containers/string_view.h>
#include <comb/default_allocator.h>

namespace nectar
{
    /// Maps file extensions to importers.
    /// Extensions are stored lowercase with the leading dot (e.g. ".png").
    class ImporterRegistry
    {
    public:
        explicit ImporterRegistry(comb::DefaultAllocator& alloc);

        /// Register an importer for all its declared extensions.
        void Register(IAssetImporter* importer);

        /// Find importer by extension (e.g. ".png"). nullptr if not found.
        [[nodiscard]] IAssetImporter* FindByExtension(wax::StringView extension) const;

        /// Find importer for a path by extracting its extension.
        [[nodiscard]] IAssetImporter* FindByPath(wax::StringView path) const;

        [[nodiscard]] size_t Count() const noexcept;

    private:
        comb::DefaultAllocator* alloc_;
        wax::HashMap<wax::String<>, IAssetImporter*> extension_map_;
    };
}
