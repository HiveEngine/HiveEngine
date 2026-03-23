#pragma once

#include <hive/hive_config.h>

#include <comb/default_allocator.h>

#include <wax/containers/hash_map.h>
#include <wax/containers/string.h>
#include <wax/containers/string_view.h>

#include <nectar/pipeline/i_asset_cooker.h>

namespace nectar
{
    /// Maps asset type names to cookers (e.g. "Texture" → TextureCooker).
    class HIVE_API CookerRegistry
    {
    public:
        explicit CookerRegistry(comb::DefaultAllocator& alloc);

        /// Register a cooker for its declared TypeName.
        void Register(IAssetCooker* cooker);

        /// Find cooker by type name. nullptr if not found.
        [[nodiscard]] IAssetCooker* FindByType(wax::StringView typeName) const;

        [[nodiscard]] size_t Count() const noexcept;

    private:
        comb::DefaultAllocator* m_alloc;
        wax::HashMap<wax::String, IAssetCooker*> m_typeMap;
    };
} // namespace nectar
