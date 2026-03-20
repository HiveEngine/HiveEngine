#pragma once

#include <comb/new.h>

#include <nectar/hive/hive_parser.h>
#include <nectar/material/material_data.h>
#include <nectar/server/asset_loader.h>

#include <cstring>

namespace nectar
{
    struct MaterialAsset
    {
        MaterialData m_data;
    };

    class MaterialAssetLoader final : public AssetLoader<MaterialAsset>
    {
    public:
        [[nodiscard]] MaterialAsset* Load(wax::ByteSpan data, comb::DefaultAllocator& alloc) override
        {
            wax::StringView content{reinterpret_cast<const char*>(data.Data()), data.Size()};
            auto parseResult = HiveParser::Parse(content, alloc);
            if (!parseResult.Success())
                return nullptr;

            auto* asset = comb::New<MaterialAsset>(alloc);
            asset->m_data.m_name = wax::String{alloc, parseResult.m_document.GetString("material", "name")};
            asset->m_data.m_shader =
                wax::String{alloc, parseResult.m_document.GetString("material", "shader", "standard_pbr")};

            return asset;
        }

        void Unload(MaterialAsset* asset, comb::DefaultAllocator& alloc) override
        {
            if (asset)
                comb::Delete(alloc, asset);
        }

        [[nodiscard]] size_t SizeOf(const MaterialAsset*) const override
        {
            return sizeof(MaterialAsset);
        }
    };

} // namespace nectar
