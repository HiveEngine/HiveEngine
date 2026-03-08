#pragma once

#include <comb/default_allocator.h>

#include <wax/containers/string.h>
#include <wax/containers/string_view.h>

namespace waggle
{
    struct CMakeGenConfig
    {
        wax::StringView m_projectName;
        wax::StringView m_projectRoot;
        wax::StringView m_enginePath;

        bool m_linkSwarm{false};
        bool m_linkTerra{false};
        bool m_linkAntennae{false};
    };

    class CMakeGenerator
    {
    public:
        [[nodiscard]] static wax::String Generate(const CMakeGenConfig& config, comb::DefaultAllocator& alloc);

        [[nodiscard]] static bool WriteToProject(const CMakeGenConfig& config, comb::DefaultAllocator& alloc);
    };
} // namespace waggle
