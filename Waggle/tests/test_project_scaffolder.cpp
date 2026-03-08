#include <comb/default_allocator.h>

#include <nectar/project/project_file.h>

#include <waggle/project/project_scaffolder.h>

#include <larvae/larvae.h>

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>
#include <system_error>

namespace
{
    bool ShouldKeepTempDirs()
    {
#if defined(_WIN32)
        char* value = nullptr;
        size_t size = 0;
        const errno_t result = _dupenv_s(&value, &size, "HIVE_KEEP_TEMP_TEST_DIRS");
        const bool keep = result == 0 && value != nullptr && size > 0;
        std::free(value);
        return keep;
#else
        return std::getenv("HIVE_KEEP_TEMP_TEST_DIRS") != nullptr;
#endif
    }

    auto& GetAlloc()
    {
        static comb::ModuleAllocator alloc{"TestProjectScaffolder", 4 * 1024 * 1024};
        return alloc.Get();
    }

    struct TempDir
    {
        std::filesystem::path m_path;

        explicit TempDir(const char* name)
        {
            m_path = std::filesystem::temp_directory_path() / name;
            std::error_code ec;
            std::filesystem::remove_all(m_path, ec);
            std::filesystem::create_directories(m_path, ec);
        }

        ~TempDir()
        {
            if (ShouldKeepTempDirs())
            {
                return;
            }

            std::error_code ec;
            std::filesystem::remove_all(m_path, ec);
        }
    };

    struct WorkingDirScope
    {
        std::filesystem::path m_previous;
        bool m_active{false};

        explicit WorkingDirScope(const std::filesystem::path& path)
        {
            std::error_code ec;
            m_previous = std::filesystem::current_path(ec);
            if (ec)
            {
                return;
            }

            std::filesystem::current_path(path, ec);
            m_active = !ec;
        }

        ~WorkingDirScope()
        {
            if (!m_active)
            {
                return;
            }

            std::error_code ec;
            std::filesystem::current_path(m_previous, ec);
        }
    };

    std::string ReadTextFile(const std::filesystem::path& path)
    {
        std::ifstream file{path, std::ios::binary};
        return std::string{std::istreambuf_iterator<char>{file}, std::istreambuf_iterator<char>{}};
    }

    std::filesystem::path GetEngineRoot()
    {
        return std::filesystem::path{__FILE__}.parent_path().parent_path().parent_path();
    }

    bool RunCommandWithLog(const std::filesystem::path& workingDir, const char* command, const char* logName,
                           std::string& output)
    {
        WorkingDirScope scope{workingDir};
        if (!scope.m_active)
        {
            return false;
        }

        const std::filesystem::path logPath = workingDir / logName;
        std::error_code ec;
        std::filesystem::remove(logPath, ec);

        std::string fullCommand{command};
        fullCommand += " > \"";
        fullCommand += logName;
        fullCommand += "\" 2>&1";

        const int exitCode = std::system(fullCommand.c_str());
        output = ReadTextFile(logPath);
        return exitCode == 0;
    }

    const char* GetGameplayModuleName()
    {
#if defined(_WIN32)
        return "gameplay.dll";
#else
        return "gameplay.so";
#endif
    }

    bool ContainsFileRecursive(const std::filesystem::path& root, const char* fileName)
    {
        std::error_code ec;
        std::filesystem::recursive_directory_iterator it{root, ec};
        std::filesystem::recursive_directory_iterator end{};
        while (!ec && it != end)
        {
            if (it->path().filename() == fileName)
            {
                return true;
            }

            it.increment(ec);
        }

        return false;
    }

    auto t_manifest = larvae::RegisterTest("WaggleProjectScaffolder", "GenerateManifest", []() {
        auto& alloc = GetAlloc();

        waggle::ProjectScaffoldConfig config{};
        config.m_cmake.m_projectName = "StandaloneGame";
        config.m_cmake.m_projectRoot = "C:/Projects/StandaloneGame";
        config.m_cmake.m_enginePath = "C:/Dev/HE";
        config.m_cmake.m_linkSwarm = true;
        config.m_cmake.m_linkTerra = true;
        config.m_cmake.m_linkAntennae = true;
        config.m_supportGame = false;

        const wax::String manifest = waggle::ProjectScaffolder::GenerateProjectManifest(config, alloc);
        const wax::StringView view = manifest.View();

        larvae::AssertTrue(view.Contains("\"projectName\": \"StandaloneGame\""));
        larvae::AssertTrue(view.Contains("\"Editor\""));
        larvae::AssertFalse(view.Contains("\"Game\""));
        larvae::AssertTrue(view.Contains("\"Headless\""));
        larvae::AssertTrue(view.Contains("\"Swarm\": true"));
        larvae::AssertTrue(view.Contains("\"Terra\": true"));
        larvae::AssertTrue(view.Contains("\"Antennae\": true"));
    });

    auto t_presets = larvae::RegisterTest("WaggleProjectScaffolder", "GenerateUserPresets", []() {
        auto& alloc = GetAlloc();

        waggle::ProjectScaffoldConfig config{};
        config.m_cmake.m_projectName = "StandaloneGame";
        config.m_cmake.m_projectRoot = "C:/Projects/StandaloneGame";
        config.m_cmake.m_enginePath = "C:\\Dev\\HE";
        config.m_cmake.m_linkSwarm = true;
        config.m_cmake.m_linkTerra = true;
        config.m_cmake.m_linkAntennae = true;
        config.m_presetBase = "llvm-windows-base";

        const wax::String presets = waggle::ProjectScaffolder::GenerateUserPresets(config, alloc);
        const wax::StringView view = presets.View();

        larvae::AssertTrue(view.Contains("\"name\": \"hive-editor\""));
        larvae::AssertTrue(view.Contains("\"name\": \"hive-game\""));
        larvae::AssertTrue(view.Contains("\"name\": \"hive-headless\""));
        larvae::AssertTrue(view.Contains("\"generator\": \"Ninja\""));
        larvae::AssertTrue(view.Contains("\"toolchainFile\": \"C:/Dev/HE/cmake/toolchains/llvm-windows.cmake\""));
        larvae::AssertTrue(view.Contains("\"CMAKE_EXPORT_COMPILE_COMMANDS\": \"ON\""));
        larvae::AssertTrue(view.Contains("\"HIVE_ENGINE_DIR\": \"C:/Dev/HE\""));
        larvae::AssertTrue(view.Contains("\"HIVE_ENGINE_MODE\": \"Editor\""));
        larvae::AssertTrue(view.Contains("\"HIVE_ENGINE_MODE\": \"Game\""));
        larvae::AssertTrue(view.Contains("\"HIVE_ENGINE_MODE\": \"Headless\""));
        larvae::AssertTrue(view.Contains("\"HIVE_FEATURE_VULKAN\": \"OFF\""));
    });

    auto t_write = larvae::RegisterTest("WaggleProjectScaffolder", "WriteToProject", []() {
        TempDir dir{"waggle_project_scaffolder_write"};
        auto& alloc = GetAlloc();

        const std::string root = dir.m_path.generic_string();

        waggle::ProjectScaffoldConfig config{};
        config.m_cmake.m_projectName = "WriteProject";
        config.m_cmake.m_projectRoot = wax::StringView{root.c_str(), root.size()};
        config.m_cmake.m_enginePath = "C:/Dev/HE";
        config.m_cmake.m_linkSwarm = true;
        config.m_cmake.m_linkTerra = true;
        config.m_cmake.m_linkAntennae = true;
        config.m_projectVersion = "1.2.3";
        config.m_presetBase = "llvm-windows-base";

        larvae::AssertTrue(waggle::ProjectScaffolder::WriteToProject(config, alloc));
        larvae::AssertTrue(std::filesystem::exists(dir.m_path / "CMakeLists.txt"));
        larvae::AssertTrue(std::filesystem::exists(dir.m_path / "CMakeUserPresets.json"));
        larvae::AssertTrue(std::filesystem::exists(dir.m_path / "HiveProject.json"));
        larvae::AssertTrue(std::filesystem::exists(dir.m_path / "project.hive"));
        larvae::AssertTrue(std::filesystem::exists(dir.m_path / "src" / "gameplay.cpp"));

        const std::string cmake = ReadTextFile(dir.m_path / "CMakeLists.txt");
        larvae::AssertTrue(wax::StringView{cmake.c_str(), cmake.size()}.Contains("if(TARGET Swarm)"));
        larvae::AssertTrue(wax::StringView{cmake.c_str(), cmake.size()}.Contains("if(TARGET Terra)"));
        larvae::AssertTrue(wax::StringView{cmake.c_str(), cmake.size()}.Contains("if(TARGET Antennae)"));

        nectar::ProjectFile projectFile{alloc};
        const std::string projectPath = (dir.m_path / "project.hive").generic_string();
        const auto loadResult = projectFile.LoadFromDisk(wax::StringView{projectPath.c_str(), projectPath.size()});

        larvae::AssertTrue(loadResult.m_success);
        larvae::AssertTrue(projectFile.Name() == wax::StringView{"WriteProject"});
        larvae::AssertTrue(projectFile.Version() == wax::StringView{"1.2.3"});
        larvae::AssertTrue(projectFile.Backend() == wax::StringView{"vulkan"});
    });

    auto t_smoke = larvae::RegisterTest("WaggleProjectScaffolder", "HeadlessStandaloneBootstrap", []() {
        TempDir dir{"waggle_project_scaffolder_smoke"};
        auto& alloc = GetAlloc();

        const std::filesystem::path engineRoot = GetEngineRoot();
        const std::string root = dir.m_path.generic_string();
        const std::string engine = engineRoot.generic_string();

        waggle::ProjectScaffoldConfig config{};
        config.m_cmake.m_projectName = "StandaloneSmoke";
        config.m_cmake.m_projectRoot = wax::StringView{root.c_str(), root.size()};
        config.m_cmake.m_enginePath = wax::StringView{engine.c_str(), engine.size()};
        config.m_projectVersion = "0.2.0";
        config.m_supportEditor = false;
        config.m_supportGame = false;
        config.m_supportHeadless = true;

        larvae::AssertTrue(waggle::ProjectScaffolder::WriteToProject(config, alloc));

        std::string listLog;
        larvae::AssertTrue(RunCommandWithLog(dir.m_path, "cmake --list-presets", "list-presets.log", listLog));
        larvae::AssertTrue(wax::StringView{listLog.c_str(), listLog.size()}.Contains("hive-headless"));

        std::string configureLog;
        larvae::AssertTrue(
            RunCommandWithLog(dir.m_path, "cmake --preset hive-headless", "configure.log", configureLog));
        larvae::AssertTrue(std::filesystem::exists(dir.m_path / "out" / "build" / "hive-headless" / "build.ninja"));

        std::string buildLog;
        larvae::AssertTrue(
            RunCommandWithLog(dir.m_path, "cmake --build \"out/build/hive-headless\" --config Debug --target gameplay",
                              "build.log", buildLog));

        larvae::AssertTrue(ContainsFileRecursive(dir.m_path, GetGameplayModuleName()));
    });
} // namespace
