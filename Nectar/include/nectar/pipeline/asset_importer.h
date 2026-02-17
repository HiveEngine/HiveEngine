#pragma once

#include <nectar/pipeline/i_asset_importer.h>

namespace nectar
{
    /// Per-type import trait: converts source files to intermediate format.
    /// Implementations run at build time, not at runtime.
    /// Inherits IAssetImporter for type-erased pipeline access.
    template<typename T>
    class AssetImporter : public IAssetImporter
    {
    public:
        ~AssetImporter() override = default;
    };
}
