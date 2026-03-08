#include <nectar/project/project_file.h>

#include <waggle/project/project_scaffolder.h>

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
} // namespace

namespace waggle
{
    wax::String ProjectScaffolder::GenerateProjectManifest(const ProjectScaffoldConfig& config,
                                                           comb::DefaultAllocator& alloc)
    {
        wax::String out{alloc};
        out.Reserve(1024);

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
        out.Append("    \"editor\": \"hive-editor\",\n");
        out.Append("    \"game\": \"hive-game\",\n");
        out.Append("    \"headless\": \"hive-headless\"\n");
        out.Append("  }\n");
        out.Append("}\n");

        return out;
    }

    wax::String ProjectScaffolder::GenerateUserPresets(const ProjectScaffoldConfig& config,
                                                       comb::DefaultAllocator& alloc)
    {
        wax::String out{alloc};
        out.Reserve(4096);

        wax::String enginePath = NormalizePath(config.m_cmake.m_enginePath, alloc);
        wax::String presetBase{alloc, config.m_presetBase.IsEmpty() ? wax::StringView{DetectDefaultPresetBase()}
                                                                    : config.m_presetBase};

        out.Append("{\n");
        out.Append("  \"version\": 6,\n");
        out.Append("  \"configurePresets\": [\n");

        struct PresetEntry
        {
            const char* m_name;
            const char* m_mode;
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
            {"hive-editor", "Editor", "Debug", true, false, true, true, true, true, true, true, true, true},
            {"hive-game", "Game", "Debug", true, false, true, false, false, false, false, false, false, false},
            {"hive-headless", "Headless", "Debug", false, false, false, false, true, true, true, false, true, false},
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

            out.Append("    {\n");
            out.Append("      \"name\": ");
            AppendJsonEscaped(out, entry.m_name);
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

        if (!CMakeGenerator::WriteToProject(config.m_cmake, alloc))
        {
            return false;
        }

        if (config.m_writeUserPresets)
        {
            const wax::String presets = GenerateUserPresets(config, alloc);
            if (!WriteTextFile(root / "CMakeUserPresets.json", presets.View()))
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
            });

            if (!projectFile.SaveToDisk((root / "project.hive").generic_string().c_str()))
            {
                return false;
            }
        }

        return true;
    }
} // namespace waggle
