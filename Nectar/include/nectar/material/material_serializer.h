#pragma once

#include <comb/default_allocator.h>

#include <wax/containers/string_view.h>

#include <nectar/material/material_data.h>

namespace nectar
{
    bool SaveMaterial(const MaterialData& mat, wax::StringView path, comb::DefaultAllocator& alloc);
    bool LoadMaterial(MaterialData& mat, wax::StringView path, comb::DefaultAllocator& alloc);
} // namespace nectar
