#pragma once

#include <comb/default_allocator.h>

#include <wax/containers/string_view.h>

namespace nectar
{
    class DependencyGraph;

    bool SaveDependencyCache(wax::StringView path, const DependencyGraph& graph, comb::DefaultAllocator& alloc);
    bool LoadDependencyCache(wax::StringView path, DependencyGraph& graph, comb::DefaultAllocator& alloc);
} // namespace nectar
