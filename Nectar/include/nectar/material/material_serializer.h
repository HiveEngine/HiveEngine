#pragma once

#include <hive/hive_config.h>

#include <comb/default_allocator.h>

#include <wax/containers/string_view.h>

#include <nectar/material/material_data.h>

namespace nectar
{
    HIVE_API bool SaveMaterial(const MaterialData& mat, wax::StringView path, comb::DefaultAllocator& alloc);
    HIVE_API bool LoadMaterial(MaterialData& mat, wax::StringView path, comb::DefaultAllocator& alloc);
} // namespace nectar
