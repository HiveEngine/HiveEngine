#pragma once

#include <hive/hive_config.h>

#include <comb/default_allocator.h>

#include <wax/containers/string_view.h>

namespace nectar
{
    class CookCache;

    HIVE_API bool SaveCookCache(wax::StringView path, const CookCache& cache, comb::DefaultAllocator& alloc);
    HIVE_API bool LoadCookCache(wax::StringView path, CookCache& cache, comb::DefaultAllocator& alloc);
} // namespace nectar
