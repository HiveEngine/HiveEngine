#pragma once

#include <comb/default_allocator.h>

#include <wax/containers/string.h>
#include <wax/containers/string_view.h>

#include <waggle/project/cmake_generator.h>

namespace waggle
{
    struct ProjectScaffoldConfig
    {
        CMakeGenConfig m_cmake;

        wax::StringView m_projectVersion{"0.1.0"};
        wax::StringView m_runtimeBackend{};
        wax::StringView m_presetBase{};
        wax::StringView m_defaultSceneRelative{"scenes/main.hscene"};

        bool m_supportEditor{true};
        bool m_supportGame{true};
        bool m_supportHeadless{true};

        bool m_writeProjectHive{true};
        bool m_writeProjectManifest{true};
        bool m_writeCMakePresets{true};
        bool m_writeGameplayStub{true};
        bool m_writeDefaultScene{true};
    };

    class ProjectScaffolder
    {
    public:
        [[nodiscard]] static wax::String GenerateProjectManifest(const ProjectScaffoldConfig& config,
                                                                 comb::DefaultAllocator& alloc);
        [[nodiscard]] static wax::String GenerateCMakePresets(const ProjectScaffoldConfig& config,
                                                              comb::DefaultAllocator& alloc);
        [[nodiscard]] static wax::String GenerateGameplayStub(const ProjectScaffoldConfig& config,
                                                              comb::DefaultAllocator& alloc);

        [[nodiscard]] static bool WriteToProject(const ProjectScaffoldConfig& config, comb::DefaultAllocator& alloc);
    };
} // namespace waggle
