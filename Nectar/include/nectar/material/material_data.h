#pragma once

#include <wax/containers/string.h>

#include <nectar/core/asset_id.h>

#include <cstdint>

namespace nectar
{
    struct MaterialData
    {
        wax::String m_name;
        wax::String m_shader{"standard_pbr"};

        float m_baseColorFactor[4]{1.f, 1.f, 1.f, 1.f};
        float m_metallicFactor{1.f};
        float m_roughnessFactor{1.f};
        float m_alphaCutoff{0.5f};
        bool m_doubleSided{false};

        AssetId m_albedoTexture;
        AssetId m_normalTexture;
        AssetId m_metallicRoughnessTexture;
        AssetId m_emissiveTexture;
        AssetId m_aoTexture;

        enum class BlendMode : uint8_t
        {
            BLEND_OPAQUE,
            ALPHA_TEST,
            ALPHA_BLEND
        };
        BlendMode m_blendMode{BlendMode::BLEND_OPAQUE};
    };
} // namespace nectar
