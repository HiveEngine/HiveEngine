#pragma once

#include <hive/hive_config.h>

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

    HIVE_API bool WriteHiveId(const char* assetPath, const HiveIdData& data);
    HIVE_API bool ReadHiveId(const char* assetPath, HiveIdData& data, comb::DefaultAllocator& alloc);
} // namespace nectar
