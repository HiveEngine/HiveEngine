#pragma once

#include <nectar/pipeline/i_asset_cooker.h>
#include <wax/containers/hash_map.h>
#include <wax/containers/string.h>
#include <wax/containers/string_view.h>
#include <comb/default_allocator.h>

namespace nectar
{
    /// Maps asset type names to cookers (e.g. "Texture" → TextureCooker).
    class CookerRegistry
    {
    public:
        explicit CookerRegistry(comb::DefaultAllocator& alloc);

        /// Register a cooker for its declared TypeName.
        void Register(IAssetCooker* cooker);

        /// Find cooker by type name. nullptr if not found.
        [[nodiscard]] IAssetCooker* FindByType(wax::StringView type_name) const;

        [[nodiscard]] size_t Count() const noexcept;

    private:
        comb::DefaultAllocator* alloc_;
        wax::HashMap<wax::String<>, IAssetCooker*> type_map_;
    };
}
