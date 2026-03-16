#include <comb/default_allocator.h>

#include <nectar/project/project_file.h>

#include <queen/reflect/component_registry.h>
#include <queen/reflect/world_deserializer.h>
#include <queen/world/world.h>

#include <waggle/components/camera.h>
#include <waggle/components/lighting.h>
#include <waggle/components/transform.h>
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
        larvae::AssertTrue(view.Contains("\"editor\": \"standalone-game-editor\""));
        larvae::AssertFalse(view.Contains("\"game\":"));
        larvae::AssertTrue(view.Contains("\"headless\": \"standalone-game-headless\""));
    });

    auto t_presets = larvae::RegisterTest("WaggleProjectScaffolder", "GenerateCMakePresets", []() {
        auto& alloc = GetAlloc();

        waggle::ProjectScaffoldConfig config{};
        config.m_cmake.m_projectName = "StandaloneGame";
        config.m_cmake.m_projectRoot = "C:/Projects/StandaloneGame";
        config.m_cmake.m_enginePath = "C:\\Dev\\HE";
        config.m_cmake.m_linkSwarm = true;
        config.m_cmake.m_linkTerra = true;
        config.m_cmake.m_linkAntennae = true;
        config.m_presetBase = "llvm-windows-base";

        const wax::String presets = waggle::ProjectScaffolder::GenerateCMakePresets(config, alloc);
        const wax::StringView view = presets.View();

        larvae::AssertTrue(view.Contains("\"name\": \"standalone-game-editor\""));
        larvae::AssertTrue(view.Contains("\"name\": \"standalone-game-game\""));
        larvae::AssertTrue(view.Contains("\"name\": \"standalone-game-headless\""));
        larvae::AssertTrue(view.Contains("\"displayName\": \"StandaloneGame Editor\""));
        larvae::AssertTrue(view.Contains("\"displayName\": \"StandaloneGame Headless Run\""));
        larvae::AssertTrue(view.Contains("\"buildPresets\""));
        larvae::AssertTrue(view.Contains("\"name\": \"standalone-game-editor-build\""));
        larvae::AssertTrue(view.Contains("\"name\": \"standalone-game-editor-run\""));
        larvae::AssertTrue(view.Contains("\"name\": \"standalone-game-game-build\""));
        larvae::AssertTrue(view.Contains("\"name\": \"standalone-game-game-run\""));
        larvae::AssertTrue(view.Contains("\"name\": \"standalone-game-headless-build\""));
        larvae::AssertTrue(view.Contains("\"name\": \"standalone-game-headless-run\""));
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
        larvae::AssertTrue(std::filesystem::exists(dir.m_path / "CMakePresets.json"));
        larvae::AssertTrue(std::filesystem::exists(dir.m_path / "HiveProject.json"));
        larvae::AssertTrue(std::filesystem::exists(dir.m_path / "project.hive"));
        larvae::AssertTrue(std::filesystem::exists(dir.m_path / "src" / "gameplay.cpp"));
        larvae::AssertTrue(std::filesystem::exists(dir.m_path / "assets" / "scenes" / "main.hscene"));

        const std::string cmake = ReadTextFile(dir.m_path / "CMakeLists.txt");
        const wax::StringView cmakeView{cmake.c_str(), cmake.size()};
        larvae::AssertTrue(cmakeView.Contains("set(HIVE_BUILD_EMBEDDED_PROJECTS OFF"));
        larvae::AssertTrue(cmakeView.Contains("include(${HIVE_ENGINE_DIR}/cmake/HiveProject.cmake)"));
        larvae::AssertTrue(cmakeView.Contains("hive_add_game_project("));
        larvae::AssertTrue(cmakeView.Contains("TARGET gameplay"));
        larvae::AssertTrue(cmakeView.Contains("SOURCES ${GAMEPLAY_SOURCES}"));
        larvae::AssertTrue(cmakeView.Contains("LINK_SWARM"));
        larvae::AssertTrue(cmakeView.Contains("LINK_TERRA"));
        larvae::AssertTrue(cmakeView.Contains("LINK_ANTENNAE"));

        const std::string gameplayStub = ReadTextFile(dir.m_path / "src" / "gameplay.cpp");
        larvae::AssertTrue(wax::StringView{gameplayStub.c_str(), gameplayStub.size()}.Contains("HiveGameplayApiVersion"));
        larvae::AssertTrue(
            wax::StringView{gameplayStub.c_str(), gameplayStub.size()}.Contains("HiveGameplayBuildSignature"));

        const std::string manifest = ReadTextFile(dir.m_path / "HiveProject.json");
        const wax::StringView manifestView{manifest.c_str(), manifest.size()};
        larvae::AssertTrue(manifestView.Contains("\"editor\": \"write-project-editor\""));
        larvae::AssertTrue(manifestView.Contains("\"game\": \"write-project-game\""));
        larvae::AssertTrue(manifestView.Contains("\"headless\": \"write-project-headless\""));

        nectar::ProjectFile projectFile{alloc};
        const std::string projectPath = (dir.m_path / "project.hive").generic_string();
        const auto loadResult = projectFile.LoadFromDisk(wax::StringView{projectPath.c_str(), projectPath.size()});

        larvae::AssertTrue(loadResult.m_success);
        larvae::AssertTrue(projectFile.Name() == wax::StringView{"WriteProject"});
        larvae::AssertTrue(projectFile.Version() == wax::StringView{"1.2.3"});
        larvae::AssertTrue(projectFile.Backend() == wax::StringView{"vulkan"});
        larvae::AssertTrue(projectFile.StartupSceneRelative() == wax::StringView{"scenes/main.hscene"});

        const std::string sceneContent = ReadTextFile(dir.m_path / "assets" / "scenes" / "main.hscene");
        queen::ComponentRegistry<8> registry{};
        registry.Register<waggle::Transform>();
        registry.Register<waggle::Camera>();
        registry.Register<waggle::DirectionalLight>();
        registry.Register<waggle::AmbientLight>();

        queen::World world{};
        const auto sceneResult = queen::WorldDeserializer::Deserialize(world, registry, sceneContent.c_str());
        larvae::AssertTrue(sceneResult.m_success);
        larvae::AssertEqual(world.EntityCount(), size_t{3});
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
        larvae::AssertTrue(wax::StringView{listLog.c_str(), listLog.size()}.Contains("standalone-smoke-headless"));

        std::string buildPresetListLog;
        larvae::AssertTrue(
            RunCommandWithLog(dir.m_path, "cmake --list-presets=build", "list-build-presets.log", buildPresetListLog));
        larvae::AssertTrue(
            wax::StringView{buildPresetListLog.c_str(), buildPresetListLog.size()}.Contains("standalone-smoke-headless-build"));
        larvae::AssertTrue(
            wax::StringView{buildPresetListLog.c_str(), buildPresetListLog.size()}.Contains("standalone-smoke-headless-run"));

        std::string configureLog;
        larvae::AssertTrue(
            RunCommandWithLog(dir.m_path, "cmake --preset standalone-smoke-headless", "configure.log", configureLog));
        larvae::AssertTrue(
            std::filesystem::exists(dir.m_path / "out" / "build" / "standalone-smoke-headless" / "build.ninja"));
        const std::string buildNinja =
            ReadTextFile(dir.m_path / "out" / "build" / "standalone-smoke-headless" / "build.ninja");
        const wax::StringView buildNinjaView{buildNinja.c_str(), buildNinja.size()};
        larvae::AssertTrue(buildNinjaView.Contains("hive_run_project"));
        larvae::AssertTrue(buildNinjaView.Contains("hive_run_headless"));
        larvae::AssertTrue(buildNinjaView.Contains("--headless"));
        larvae::AssertTrue(buildNinjaView.Contains("--project"));

        std::string buildLog;
        larvae::AssertTrue(RunCommandWithLog(dir.m_path,
                                             "cmake --build --preset standalone-smoke-headless-build", "build.log",
                                             buildLog));

        larvae::AssertTrue(ContainsFileRecursive(dir.m_path, GetGameplayModuleName()));
    });
} // namespace
