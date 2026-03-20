#pragma once

#include <comb/default_allocator.h>

#include <wax/containers/string.h>

#include <nectar/core/asset_id.h>

namespace nectar
{
    struct HiveIdData
    {
        AssetId m_guid;
        wax::String m_type;
    };

    bool WriteHiveId(const char* assetPath, const HiveIdData& data);
    bool ReadHiveId(const char* assetPath, HiveIdData& data, comb::DefaultAllocator& alloc);
} // namespace nectar
