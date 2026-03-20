#pragma once

#include <comb/default_allocator.h>

#include <wax/containers/string_view.h>

namespace nectar
{
    class CookCache;

    bool SaveCookCache(wax::StringView path, const CookCache& cache, comb::DefaultAllocator& alloc);
    bool LoadCookCache(wax::StringView path, CookCache& cache, comb::DefaultAllocator& alloc);
} // namespace nectar
