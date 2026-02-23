#pragma once

#include <wax/containers/string.h>
#include <wax/containers/string_view.h>
#include <comb/default_allocator.h>

namespace waggle
{
    struct CMakeGenConfig
    {
        wax::StringView project_name;
        wax::StringView project_root;
        wax::StringView engine_path;

        bool link_swarm{false};
        bool link_terra{false};
        bool link_antennae{false};
    };

    class CMakeGenerator
    {
    public:
        [[nodiscard]] static wax::String<> Generate(
            const CMakeGenConfig& config,
            comb::DefaultAllocator& alloc);

        [[nodiscard]] static bool WriteToProject(
            const CMakeGenConfig& config,
            comb::DefaultAllocator& alloc);
    };
}
