#pragma once

#include <hive/hive_config.h>

#include <comb/default_allocator.h>

#include <wax/containers/string_view.h>

namespace nectar
{
    class DependencyGraph;

    HIVE_API bool SaveDependencyCache(wax::StringView path, const DependencyGraph& graph, comb::DefaultAllocator& alloc);
    HIVE_API bool LoadDependencyCache(wax::StringView path, DependencyGraph& graph, comb::DefaultAllocator& alloc);
} // namespace nectar
