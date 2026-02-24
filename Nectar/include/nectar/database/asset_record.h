#pragma once

#include <nectar/core/asset_id.h>
#include <nectar/core/content_hash.h>
#include <wax/containers/string.h>
#include <wax/containers/vector.h>
#include <cstdint>

namespace nectar
{
    struct AssetRecord
    {
        AssetId uuid;
        wax::String<> path;            // virtual path ("textures/hero.png")
        wax::String<> type;            // type name ("Texture", "Mesh")
        wax::String<> name;            // short name ("hero")
        ContentHash content_hash;          // source data hash (change detection)
        ContentHash intermediate_hash;     // CAS hash of intermediate blob
        uint32_t import_version{0};
        wax::Vector<wax::String<>> labels;
    };
}
