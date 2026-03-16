#include <nectar/project/project_file.h>

#include <queen/reflect/component_registry.h>
#include <queen/reflect/world_serializer.h>
#include <queen/world/world.h>

#include <waggle/components/camera.h>
#include <waggle/components/lighting.h>
#include <waggle/components/transform.h>
#include <waggle/project/project_scaffolder.h>

#include <cctype>
#include <filesystem>
#include <fstream>
#include <string>

namespace
{
    void AppendJsonEscaped(wax::String& out, wax::StringView value)
    {
        out.Append('"');
        for (size_t i = 0; i < value.Size(); ++i)
        {
            const char c = value.Data()[i];
            switch (c)
            {
                case '\\':
                    out.Append("\\\\");
                    break;
                case '"':
                    out.Append("\\\"");
                    break;
                case '\n':
                    out.Append("\\n");
                    break;
                case '\r':
                    out.Append("\\r");
                    break;
                case '\t':
                    out.Append("\\t");
                    break;
                default:
                    out.Append(&c, 1);
                    break;
            }
        }
        out.Append('"');
    }

    wax::String NormalizePath(wax::StringView path, comb::DefaultAllocator& alloc)
    {
        wax::String normalized{alloc, path};
        char* data = normalized.Data();
        for (size_t i = 0; i < normalized.Size(); ++i)
        {
            if (data[i] == '\\')
            {
                data[i] = '/';
            }
        }
        return normalized;
    }

    char ToLowerAscii(char value)
    {
        return static_cast<char>(std::tolower(static_cast<unsigned char>(value)));
    }

    wax::String MakePresetPrefix(wax::StringView projectName, comb::DefaultAllocator& alloc)
    {
        wax::String prefix{alloc};
        bool needsSeparator = false;
        bool previousWasLowerOrDigit = false;

        for (size_t i = 0; i < projectName.Size(); ++i)
        {
            const char value = projectName.Data()[i];
            if (std::isalnum(static_cast<unsigned char>(value)) != 0)
            {
                const bool isUpper = value >= 'A' && value <= 'Z';
                const bool shouldInsertSeparator = needsSeparator || (isUpper && previousWasLowerOrDigit);
                if (shouldInsertSeparator && !prefix.IsEmpty())
                {
                    prefix.Append("-");
                }

                const char normalized = ToLowerAscii(value);
                prefix.Append(&normalized, 1);
                needsSeparator = false;
                previousWasLowerOrDigit = (value >= 'a' && value <= 'z') || (value >= '0' && value <= '9');
            }
            else
            {
                needsSeparator = !prefix.IsEmpty();
                previousWasLowerOrDigit = false;
            }
        }

        if (prefix.IsEmpty())
        {
            prefix.Append("hive-project");
        }

        return prefix;
    }

    wax::String MakeModePresetName(wax::StringView prefix, wax::StringView modeSuffix, comb::DefaultAllocator& alloc)
    {
        wax::String name{alloc, prefix};
        if (!name.IsEmpty())
        {
            name.Append("-");
        }
        name.Append(modeSuffix);
        return name;
    }

    wax::String MakeDisplayPresetName(wax::StringView projectName, wax::StringView suffix,
                                      comb::DefaultAllocator& alloc)
    {
        wax::String displayName{alloc, projectName.IsEmpty() ? wax::StringView{"Hive Project"} : projectName};
        if (!suffix.IsEmpty())
        {
            displayName.Append(" ");
            displayName.Append(suffix);
        }
        return displayName;
    }

    const char* DetectDefaultPresetBase()
    {
#if defined(_WIN32)
#if defined(__clang__)
        return "llvm-windows-base";
#else
        return "msvc-windows-base";
#endif
#else
#if defined(__clang__)
        return "llvm-linux-base";
#else
        return "gcc-linux-base";
#endif
#endif
    }

    bool WriteTextFile(const std::filesystem::path& path, wax::StringView content)
    {
        std::ofstream file{path, std::ios::binary};
        if (!file)
        {
            return false;
        }

        file.write(content.Data(), static_cast<std::streamsize>(content.Size()));
        return file.good();
    }

    wax::String GenerateDefaultScene(comb::DefaultAllocator& alloc)
    {
        queen::World world{};
        queen::ComponentRegistry<8> registry{};
        registry.Register<waggle::Transform>();
        registry.Register<waggle::Camera>();
        registry.Register<waggle::DirectionalLight>();
        registry.Register<waggle::AmbientLight>();

        waggle::Transform cameraTransform{};
        cameraTransform.m_position = hive::math::Float3{0.0f, 1.5f, 5.0f};
        (void)world.Spawn(waggle::Transform{cameraTransform}, waggle::Camera{});

        waggle::DirectionalLight sun{};
        sun.direction = hive::math::Float3{0.35f, -1.0f, 0.15f};
        (void)world.Spawn(waggle::DirectionalLight{sun});

        waggle::AmbientLight ambient{};
        ambient.color = hive::math::Float3{0.15f, 0.15f, 0.18f};
        (void)world.Spawn(waggle::AmbientLight{ambient});

        queen::WorldSerializer<8192> serializer{};
        const auto result = serializer.Serialize(world, registry);

        wax::String scene{alloc};
        if (result.m_success)
        {
            scene.Append(serializer.CStr(), serializer.Size());
        }
        return scene;
    }

    bool SupportsMode(const waggle::ProjectScaffoldConfig& config, wax::StringView mode)
    {
        if (mode == wax::StringView{"Editor"})
        {
            return config.m_supportEditor;
        }
        if (mode == wax::StringView{"Game"})
        {
            return config.m_supportGame;
        }
        if (mode == wax::StringView{"Headless"})
        {
            return config.m_supportHeadless;
        }
        return false;
    }

    void AppendModeEntry(wax::String& out, bool& firstMode, wax::StringView mode)
    {
        if (!firstMode)
        {
            out.Append(", ");
        }
        AppendJsonEscaped(out, mode);
        firstMode = false;
    }

    void AppendPresetHeader(wax::String& out, wax::StringView presetBase, wax::StringView enginePath,
                            comb::DefaultAllocator& alloc)
    {
        out.Append("      \"generator\": \"Ninja\",\n");

        if (presetBase == wax::StringView{"llvm-windows-base"})
        {
            wax::String toolchainPath{alloc, enginePath};
            toolchainPath.Append("/cmake/toolchains/llvm-windows.cmake");
            out.Append("      \"toolchainFile\": ");
            AppendJsonEscaped(out, toolchainPath.View());
            out.Append(",\n");
        }
    }

    void AppendPresetBaseCacheVariables(wax::String& out, wax::StringView presetBase)
    {
        out.Append("        \"CMAKE_EXPORT_COMPILE_COMMANDS\": \"ON\",\n");

        if (presetBase == wax::StringView{"msvc-windows-base"})
        {
            out.Append("        \"CMAKE_C_COMPILER\": \"cl\",\n");
            out.Append("        \"CMAKE_CXX_COMPILER\": \"cl\",\n");
        }
        else if (presetBase == wax::StringView{"gcc-linux-base"})
        {
            out.Append("        \"CMAKE_C_COMPILER\": \"gcc\",\n");
            out.Append("        \"CMAKE_CXX_COMPILER\": \"g++\",\n");
        }
        else if (presetBase == wax::StringView{"llvm-linux-base"})
        {
            out.Append("        \"CMAKE_C_COMPILER\": \"clang\",\n");
            out.Append("        \"CMAKE_CXX_COMPILER\": \"clang++\",\n");
        }
    }

    void AppendBuildPreset(wax::String& out, bool& firstPreset, wax::StringView name, wax::StringView displayName,
                           wax::StringView configurePreset, wax::StringView target)
    {
        if (!firstPreset)
        {
            out.Append(",\n");
        }
        firstPreset = false;

        out.Append("    {\n");
        out.Append("      \"name\": ");
        AppendJsonEscaped(out, name);
        out.Append(",\n");
        out.Append("      \"displayName\": ");
        AppendJsonEscaped(out, displayName);
        out.Append(",\n");
        out.Append("      \"configurePreset\": ");
        AppendJsonEscaped(out, configurePreset);
        out.Append(",\n");
        out.Append("      \"targets\": [");
        AppendJsonEscaped(out, target);
        out.Append("]\n");
        out.Append("    }");
    }
} // namespace

namespace waggle
{
    wax::String ProjectScaffolder::GenerateProjectManifest(const ProjectScaffoldConfig& config,
                                                           comb::DefaultAllocator& alloc)
    {
        wax::String out{alloc};
        out.Reserve(1024);
        const wax::String presetPrefix = MakePresetPrefix(config.m_cmake.m_projectName, alloc);

        out.Append("{\n");
        out.Append("  \"version\": 1,\n");
        out.Append("  \"projectName\": ");
        AppendJsonEscaped(out, config.m_cmake.m_projectName);
        out.Append(",\n");
        out.Append("  \"supportedModes\": [");

        bool firstMode = true;
        if (config.m_supportEditor)
        {
            AppendModeEntry(out, firstMode, "Editor");
        }
        if (config.m_supportGame)
        {
            AppendModeEntry(out, firstMode, "Game");
        }
        if (config.m_supportHeadless)
        {
            AppendModeEntry(out, firstMode, "Headless");
        }

        out.Append("],\n");
        out.Append("  \"moduleDependencies\": {\n");
        out.Append("    \"Swarm\": ");
        out.Append(config.m_cmake.m_linkSwarm ? "true" : "false");
        out.Append(",\n");
        out.Append("    \"Terra\": ");
        out.Append(config.m_cmake.m_linkTerra ? "true" : "false");
        out.Append(",\n");
        out.Append("    \"Antennae\": ");
        out.Append(config.m_cmake.m_linkAntennae ? "true" : "false");
        out.Append("\n");
        out.Append("  },\n");
        out.Append("  \"presetNames\": {\n");
        bool firstPresetName = true;
        const auto appendPresetName = [&](wax::StringView key, wax::StringView modeSuffix) {
            if (!firstPresetName)
            {
                out.Append(",\n");
            }

            out.Append("    ");
            AppendJsonEscaped(out, key);
            out.Append(": ");

            const wax::String presetName = MakeModePresetName(presetPrefix.View(), modeSuffix, alloc);
            AppendJsonEscaped(out, presetName.View());
            firstPresetName = false;
        };

        if (config.m_supportEditor)
        {
            appendPresetName("editor", "editor");
        }
        if (config.m_supportGame)
        {
            appendPresetName("game", "game");
        }
        if (config.m_supportHeadless)
        {
            appendPresetName("headless", "headless");
        }
        out.Append("\n");
        out.Append("  }\n");
        out.Append("}\n");

        return out;
    }

    wax::String ProjectScaffolder::GenerateCMakePresets(const ProjectScaffoldConfig& config,
                                                        comb::DefaultAllocator& alloc)
    {
        wax::String out{alloc};
        out.Reserve(10240);

        wax::String enginePath = NormalizePath(config.m_cmake.m_enginePath, alloc);
        wax::String presetBase{alloc, config.m_presetBase.IsEmpty() ? wax::StringView{DetectDefaultPresetBase()}
                                                                    : config.m_presetBase};
        const wax::String presetPrefix = MakePresetPrefix(config.m_cmake.m_projectName, alloc);

        out.Append("{\n");
        out.Append("  \"version\": 6,\n");
        out.Append("  \"configurePresets\": [\n");

        struct PresetEntry
        {
            const char* m_suffix;
            const char* m_mode;
            const char* m_runTarget;
            const char* m_buildType;
            bool m_vulkan;
            bool m_d3d12;
            bool m_glfw;
            bool m_imgui;
            bool m_logging;
            bool m_asserts;
            bool m_profiler;
            bool m_memDebug;
            bool m_console;
            bool m_hotReload;
        };

        const PresetEntry presetEntries[] = {
            {"editor", "Editor", "hive_run_editor", "Debug", true, false, true, true, true, true, true, true,
             true, true},
            {"game", "Game", "hive_run_game", "Debug", true, false, true, false, false, false, false, false,
             false, false},
            {"headless", "Headless", "hive_run_headless", "Debug", false, false, false, false, true, true, true,
             false, true, false},
        };

        bool firstPreset = true;
        for (const PresetEntry& entry : presetEntries)
        {
            if (!SupportsMode(config, entry.m_mode))
            {
                continue;
            }

            if (!firstPreset)
            {
                out.Append(",\n");
            }
            firstPreset = false;

            const wax::String presetName = MakeModePresetName(presetPrefix.View(), entry.m_suffix, alloc);
            const wax::String displayName =
                MakeDisplayPresetName(config.m_cmake.m_projectName, entry.m_mode, alloc);

            out.Append("    {\n");
            out.Append("      \"name\": ");
            AppendJsonEscaped(out, presetName.View());
            out.Append(",\n");
            out.Append("      \"displayName\": ");
            AppendJsonEscaped(out, displayName.View());
            out.Append(",\n");
            AppendPresetHeader(out, presetBase.View(), enginePath.View(), alloc);
            out.Append("      \"binaryDir\": \"${sourceDir}/out/build/${presetName}\",\n");
            out.Append("      \"cacheVariables\": {\n");
            AppendPresetBaseCacheVariables(out, presetBase.View());
            out.Append("        \"CMAKE_BUILD_TYPE\": ");
            AppendJsonEscaped(out, entry.m_buildType);
            out.Append(",\n");
            out.Append("        \"HIVE_ENGINE_DIR\": ");
            AppendJsonEscaped(out, wax::StringView{enginePath.CStr(), enginePath.Size()});
            out.Append(",\n");
            out.Append("        \"HIVE_ENGINE_MODE\": ");
            AppendJsonEscaped(out, entry.m_mode);
            out.Append(",\n");
            out.Append("        \"HIVE_FEATURE_VULKAN\": ");
            AppendJsonEscaped(out, entry.m_vulkan ? "ON" : "OFF");
            out.Append(",\n");
            out.Append("        \"HIVE_FEATURE_D3D12\": ");
            AppendJsonEscaped(out, entry.m_d3d12 ? "ON" : "OFF");
            out.Append(",\n");
            out.Append("        \"HIVE_FEATURE_GLFW\": ");
            AppendJsonEscaped(out, entry.m_glfw ? "ON" : "OFF");
            out.Append(",\n");
            out.Append("        \"HIVE_FEATURE_IMGUI\": ");
            AppendJsonEscaped(out, entry.m_imgui ? "ON" : "OFF");
            out.Append(",\n");
            out.Append("        \"HIVE_FEATURE_LOGGING\": ");
            AppendJsonEscaped(out, entry.m_logging ? "ON" : "OFF");
            out.Append(",\n");
            out.Append("        \"HIVE_FEATURE_ASSERTS\": ");
            AppendJsonEscaped(out, entry.m_asserts ? "ON" : "OFF");
            out.Append(",\n");
            out.Append("        \"HIVE_FEATURE_PROFILER\": ");
            AppendJsonEscaped(out, entry.m_profiler ? "ON" : "OFF");
            out.Append(",\n");
            out.Append("        \"HIVE_FEATURE_MEM_DEBUG\": ");
            AppendJsonEscaped(out, entry.m_memDebug ? "ON" : "OFF");
            out.Append(",\n");
            out.Append("        \"HIVE_FEATURE_CONSOLE\": ");
            AppendJsonEscaped(out, entry.m_console ? "ON" : "OFF");
            out.Append(",\n");
            out.Append("        \"HIVE_FEATURE_HOT_RELOAD\": ");
            AppendJsonEscaped(out, entry.m_hotReload ? "ON" : "OFF");
            out.Append(",\n");
            out.Append("        \"HIVE_ENABLE_SANITIZERS\": \"OFF\",\n");
            out.Append("        \"HIVE_SANITIZER_TYPE\": \"Address\"\n");
            out.Append("      }\n");
            out.Append("    }");
        }

        out.Append("\n  ],\n");
        out.Append("  \"buildPresets\": [\n");

        bool firstBuildPreset = true;
        for (const PresetEntry& entry : presetEntries)
        {
            if (!SupportsMode(config, entry.m_mode))
            {
                continue;
            }

            const wax::String presetName = MakeModePresetName(presetPrefix.View(), entry.m_suffix, alloc);

            wax::String buildName{alloc, presetName};
            buildName.Append("-build");
            wax::String buildDisplayName =
                MakeDisplayPresetName(config.m_cmake.m_projectName, entry.m_mode, alloc);
            buildDisplayName.Append(" Build");
            AppendBuildPreset(out, firstBuildPreset, buildName.View(), buildDisplayName.View(), presetName.View(),
                              "hive_launcher");

            wax::String runName{alloc, presetName};
            runName.Append("-run");
            wax::String runDisplayName =
                MakeDisplayPresetName(config.m_cmake.m_projectName, entry.m_mode, alloc);
            runDisplayName.Append(" Run");
            AppendBuildPreset(out, firstBuildPreset, runName.View(), runDisplayName.View(), presetName.View(),
                              entry.m_runTarget);
        }

        out.Append("\n  ]\n");
        out.Append("}\n");

        return out;
    }

    wax::String ProjectScaffolder::GenerateGameplayStub(const ProjectScaffoldConfig& config,
                                                        comb::DefaultAllocator& alloc)
    {
        wax::String out{alloc};
        out.Reserve(512);

        out.Append("#include <hive/project/gameplay_api.h>\n\n");
        out.Append("namespace queen\n");
        out.Append("{\n");
        out.Append("    class World;\n");
        out.Append("}\n\n");
        out.Append("HIVE_GAMEPLAY_EXPORT uint32_t HiveGameplayApiVersion() {\n");
        out.Append("    return HIVE_GAMEPLAY_API_VERSION;\n");
        out.Append("}\n\n");
        out.Append("HIVE_GAMEPLAY_EXPORT const char* HiveGameplayBuildSignature() {\n");
        out.Append("    return HIVE_GAMEPLAY_BUILD_SIGNATURE;\n");
        out.Append("}\n\n");
        out.Append("HIVE_GAMEPLAY_EXPORT void HiveGameplayRegister(queen::World& /*world*/) {}\n\n");
        out.Append("HIVE_GAMEPLAY_EXPORT void HiveGameplayUnregister(queen::World& /*world*/) {}\n\n");
        out.Append("HIVE_GAMEPLAY_EXPORT const char* HiveGameplayVersion() {\n");
        out.Append("    return \"");
        out.Append(config.m_projectVersion);
        out.Append("\";\n");
        out.Append("}\n");

        return out;
    }

    bool ProjectScaffolder::WriteToProject(const ProjectScaffoldConfig& config, comb::DefaultAllocator& alloc)
    {
        if (!config.m_supportEditor && !config.m_supportGame && !config.m_supportHeadless)
        {
            return false;
        }

        const std::filesystem::path root = std::filesystem::path{
            std::string{config.m_cmake.m_projectRoot.Data(), config.m_cmake.m_projectRoot.Size()}};
        std::error_code ec;
        std::filesystem::create_directories(root / "src", ec);
        if (ec)
        {
            return false;
        }
        std::filesystem::create_directories(root / "assets", ec);
        if (ec)
        {
            return false;
        }
        if (config.m_writeDefaultScene && !config.m_defaultSceneRelative.IsEmpty())
        {
            std::filesystem::create_directories(root / "assets" /
                                                    std::filesystem::path{std::string{config.m_defaultSceneRelative.Data(),
                                                                                       config.m_defaultSceneRelative.Size()}}
                                                        .parent_path(),
                                                ec);
            if (ec)
            {
                return false;
            }
        }

        if (!CMakeGenerator::WriteToProject(config.m_cmake, alloc))
        {
            return false;
        }

        if (config.m_writeCMakePresets)
        {
            const wax::String presets = GenerateCMakePresets(config, alloc);
            if (!WriteTextFile(root / "CMakePresets.json", presets.View()))
            {
                return false;
            }
        }

        if (config.m_writeProjectManifest)
        {
            const wax::String manifest = GenerateProjectManifest(config, alloc);
            if (!WriteTextFile(root / "HiveProject.json", manifest.View()))
            {
                return false;
            }
        }

        if (config.m_writeGameplayStub)
        {
            const wax::String stub = GenerateGameplayStub(config, alloc);
            if (!WriteTextFile(root / "src" / "gameplay.cpp", stub.View()))
            {
                return false;
            }
        }

        if (config.m_writeProjectHive)
        {
            nectar::ProjectFile projectFile{alloc};
            const bool hasRenderBackend = config.m_cmake.m_linkSwarm;
            const wax::StringView backend = !config.m_runtimeBackend.IsEmpty()
                                                ? config.m_runtimeBackend
                                                : (hasRenderBackend ? wax::StringView{"vulkan"} : wax::StringView{});
            projectFile.Create(nectar::ProjectDesc{
                .m_name = config.m_cmake.m_projectName,
                .m_version = config.m_projectVersion,
                .m_enginePath = {},
                .m_backend = backend,
                .m_startupScene = config.m_writeDefaultScene ? config.m_defaultSceneRelative : wax::StringView{},
            });

            if (!projectFile.SaveToDisk((root / "project.hive").generic_string().c_str()))
            {
                return false;
            }
        }

        if (config.m_writeDefaultScene && !config.m_defaultSceneRelative.IsEmpty())
        {
            const wax::String scene = GenerateDefaultScene(alloc);
            const std::filesystem::path scenePath =
                root / "assets" /
                std::filesystem::path{std::string{config.m_defaultSceneRelative.Data(), config.m_defaultSceneRelative.Size()}};
            if (!WriteTextFile(scenePath, scene.View()))
            {
                return false;
            }
        }

        return true;
    }
} // namespace waggle
