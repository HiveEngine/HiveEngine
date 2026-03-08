#pragma once

#include <wax/containers/string.h>
#include <wax/containers/vector.h>

#include <nectar/core/asset_id.h>
#include <nectar/core/content_hash.h>

#include <cstdint>

namespace nectar
{
    struct AssetRecord
    {
        AssetId m_uuid;
        wax::String m_path;             // virtual path ("textures/hero.png")
        wax::String m_type;             // type name ("Texture", "Mesh")
        wax::String m_name;             // short name ("hero")
        ContentHash m_contentHash;      // source data hash (change detection)
        ContentHash m_intermediateHash; // CAS hash of intermediate blob
        uint32_t m_importVersion{0};
        wax::Vector<wax::String> m_labels;
    };
} // namespace nectar
