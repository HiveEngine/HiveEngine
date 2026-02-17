#pragma once

#include <nectar/pipeline/i_asset_cooker.h>

namespace nectar
{
    /// Per-type cook trait: converts intermediate format to platform-optimized format.
    /// Inherits IAssetCooker for type-erased pipeline access.
    template<typename T>
    class AssetCooker : public IAssetCooker
    {
    public:
        ~AssetCooker() override = default;
    };
}
