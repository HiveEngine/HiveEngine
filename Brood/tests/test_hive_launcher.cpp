#include <comb/default_allocator.h>

#include <nectar/hive/hive_parser.h>

#include <waggle/project/project_scaffolder.h>

#include <larvae/larvae.h>

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#if HIVE_PLATFORM_WINDOWS
#include <Windows.h>
#endif

namespace
{
    auto& GetAlloc()
    {
        static comb::ModuleAllocator alloc{"TestHiveLauncher", 4 * 1024 * 1024};
        return alloc.Get();
    }

#if HIVE_PLATFORM_WINDOWS
    std::filesystem::path GetCurrentExecutablePath();
#endif

    std::filesystem::path FindBuildRoot()
    {
#if HIVE_PLATFORM_WINDOWS
        auto current = GetCurrentExecutablePath().parent_path();
        for (int i = 0; i < 4; ++i)
        {
            if (std::filesystem::exists(current / "CMakeCache.txt"))
            {
                return current;
            }

            current = current.parent_path();
        }
#endif
        return {};
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
            std::error_code ec;
            std::filesystem::remove_all(m_path, ec);
        }
    };

#if HIVE_PLATFORM_WINDOWS
    std::filesystem::path GetCurrentExecutablePath()
    {
        std::wstring buffer(static_cast<size_t>(MAX_PATH), L'\0');

        while (true)
        {
            const DWORD length = GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
            larvae::AssertTrue(length > 0);

            if (length < buffer.size() - 1)
            {
                buffer.resize(length);
                return std::filesystem::path{buffer};
            }

            buffer.resize(buffer.size() * 2);
        }
    }

    std::filesystem::path FindRepoRoot()
    {
        auto current = GetCurrentExecutablePath().parent_path();
        for (int i = 0; i < 8; ++i)
        {
            if (std::filesystem::exists(current / "CMakeLists.txt") && std::filesystem::exists(current / "projects"))
            {
                return current;
            }

            current = current.parent_path();
        }

        return {};
    }

    std::wstring QuoteArgument(const std::wstring& arg)
    {
        std::wstring quoted = L"\"";
        for (wchar_t ch : arg)
        {
            if (ch == L'"')
            {
                quoted += L'\\';
            }
            quoted += ch;
        }
        quoted += L"\"";
        return quoted;
    }

    int RunProcess(const std::filesystem::path& executable, const std::vector<std::wstring>& arguments,
                   const std::filesystem::path& workingDirectory, DWORD timeoutMs)
    {
        std::wstring commandLine = QuoteArgument(executable.native());
        for (const auto& arg : arguments)
        {
            commandLine += L" ";
            commandLine += QuoteArgument(arg);
        }

        STARTUPINFOW startupInfo{};
        startupInfo.cb = sizeof(startupInfo);

        PROCESS_INFORMATION processInfo{};
        std::wstring mutableCommandLine = commandLine;
        const std::wstring workingDirectoryWide = workingDirectory.native();
        const BOOL created = CreateProcessW(executable.c_str(), mutableCommandLine.data(), nullptr, nullptr, FALSE, 0,
                                            nullptr, workingDirectoryWide.c_str(), &startupInfo, &processInfo);
        larvae::AssertTrue(created != FALSE);

        const DWORD waitResult = WaitForSingleObject(processInfo.hProcess, timeoutMs);
        larvae::AssertEqual(waitResult, static_cast<DWORD>(WAIT_OBJECT_0));

        DWORD exitCode = 0;
        const BOOL gotExitCode = GetExitCodeProcess(processInfo.hProcess, &exitCode);
        larvae::AssertTrue(gotExitCode != FALSE);

        CloseHandle(processInfo.hThread);
        CloseHandle(processInfo.hProcess);
        return static_cast<int>(exitCode);
    }
#endif

    std::string ReadTextFile(const std::filesystem::path& path)
    {
        std::ifstream file{path, std::ios::binary};
        return std::string{std::istreambuf_iterator<char>{file}, std::istreambuf_iterator<char>{}};
    }

#if HIVE_PLATFORM_WINDOWS
    std::filesystem::path FindCMakeExecutable()
    {
        std::wstring buffer(static_cast<size_t>(MAX_PATH), L'\0');
        const DWORD length =
            SearchPathW(nullptr, L"cmake.exe", nullptr, static_cast<DWORD>(buffer.size()), buffer.data(), nullptr);
        larvae::AssertTrue(length > 0 && length < buffer.size());
        buffer.resize(length);
        return std::filesystem::path{buffer};
    }

    void EnsureSponzaDemoBuilt(const std::filesystem::path& buildRoot, const std::filesystem::path& repoRoot)
    {
        static bool s_built = false;
        if (s_built)
        {
            return;
        }

        larvae::AssertTrue(!buildRoot.empty());

        const auto cmakePath = FindCMakeExecutable();
        larvae::AssertTrue(std::filesystem::exists(cmakePath));

        const auto moduleDir = repoRoot / "projects" / "sponza_demo" / ".hive" / "modules";

        std::error_code ec;
        std::filesystem::remove_all(moduleDir, ec);

        const int exitCode =
            RunProcess(cmakePath, {L"--build", buildRoot.native(), L"--target", L"sponza_demo"}, repoRoot, 120000);
        larvae::AssertEqual(exitCode, 0);

        bool foundDll = false;
        if (std::filesystem::exists(moduleDir, ec))
        {
            for (const auto& entry : std::filesystem::recursive_directory_iterator{moduleDir, ec})
            {
                if (entry.path().filename() == "gameplay.dll")
                {
                    foundDll = true;
                    break;
                }
            }
        }
        larvae::AssertTrue(foundDll);
        s_built = true;
    }
#endif

    auto t_launcher_headless_process_smoke =
        larvae::RegisterTest("HiveLauncher", "headless_exit_after_setup_process_smoke", []() {
#if HIVE_PLATFORM_WINDOWS
            const auto repoRoot = FindRepoRoot();
            larvae::AssertTrue(!repoRoot.empty());
            const auto buildRoot = FindBuildRoot();
            larvae::AssertTrue(!buildRoot.empty());

            const auto launcherPath = GetCurrentExecutablePath().parent_path() / "hive_launcher.exe";
            const auto projectPath = repoRoot / "projects" / "sponza_demo" / "project.hive";

            larvae::AssertTrue(std::filesystem::exists(launcherPath));
            larvae::AssertTrue(std::filesystem::exists(projectPath));
            EnsureSponzaDemoBuilt(buildRoot, repoRoot);

            const int exitCode =
                RunProcess(launcherPath, {L"--headless", L"--project", projectPath.native(), L"--exit-after-setup"},
                           repoRoot, 30000);
            larvae::AssertEqual(exitCode, 0);
#else
            larvae::AssertTrue(true);
#endif
        });

    auto t_launcher_headless_scaffolded_project_process_smoke =
        larvae::RegisterTest("HiveLauncher", "headless_scaffolded_project_exit_after_setup", []() {
#if HIVE_PLATFORM_WINDOWS
            const auto repoRoot = FindRepoRoot();
            larvae::AssertTrue(!repoRoot.empty());

            const auto launcherPath = GetCurrentExecutablePath().parent_path() / "hive_launcher.exe";
            larvae::AssertTrue(std::filesystem::exists(launcherPath));

            TempDir dir{"hive_launcher_scaffolded_project"};
            const std::string root = dir.m_path.generic_string();
            const std::string engine = repoRoot.generic_string();

            auto& alloc = GetAlloc();
            waggle::ProjectScaffoldConfig config{};
            config.m_cmake.m_projectName = "LauncherSmoke";
            config.m_cmake.m_projectRoot = wax::StringView{root.c_str(), root.size()};
            config.m_cmake.m_enginePath = wax::StringView{engine.c_str(), engine.size()};
            config.m_projectVersion = "0.1.0";

            larvae::AssertTrue(waggle::ProjectScaffolder::WriteToProject(config, alloc));

            const auto projectPath = dir.m_path / "project.hive";
            larvae::AssertTrue(std::filesystem::exists(projectPath));

            const int exitCode =
                RunProcess(launcherPath, {L"--headless", L"--project", projectPath.native(), L"--exit-after-setup"},
                           repoRoot, 30000);
            larvae::AssertEqual(exitCode, 0);
#else
            larvae::AssertTrue(true);
#endif
        });

    auto t_launcher_invalid_project_process_smoke =
        larvae::RegisterTest("HiveLauncher", "invalid_project_returns_1", []() {
#if HIVE_PLATFORM_WINDOWS
            const auto repoRoot = FindRepoRoot();
            larvae::AssertTrue(!repoRoot.empty());

            const auto launcherPath = GetCurrentExecutablePath().parent_path() / "hive_launcher.exe";
            const auto missingProjectPath = repoRoot / "projects" / "__missing__" / "project.hive";

            larvae::AssertTrue(std::filesystem::exists(launcherPath));

            const int exitCode = RunProcess(launcherPath, {L"--project", missingProjectPath.native()}, repoRoot, 10000);
            larvae::AssertEqual(exitCode, 1);
#else
            larvae::AssertTrue(true);
#endif
        });

    auto t_launcher_headless_without_project_returns_1 =
        larvae::RegisterTest("HiveLauncher", "headless_without_project_returns_1", []() {
#if HIVE_PLATFORM_WINDOWS
            const auto repoRoot = FindRepoRoot();
            larvae::AssertTrue(!repoRoot.empty());

            const auto launcherPath = GetCurrentExecutablePath().parent_path() / "hive_launcher.exe";
            larvae::AssertTrue(std::filesystem::exists(launcherPath));

            const int exitCode = RunProcess(launcherPath, {L"--headless"}, repoRoot, 10000);
            larvae::AssertEqual(exitCode, 1);
#else
            larvae::AssertTrue(true);
#endif
        });

    auto t_launcher_headless_project_runs_gameplay =
        larvae::RegisterTest("HiveLauncher", "headless_project_runs_gameplay", []() {
#if HIVE_PLATFORM_WINDOWS
            const auto repoRoot = FindRepoRoot();
            larvae::AssertTrue(!repoRoot.empty());
            const auto buildRoot = FindBuildRoot();
            larvae::AssertTrue(!buildRoot.empty());

            const auto launcherPath = GetCurrentExecutablePath().parent_path() / "hive_launcher.exe";
            const auto projectPath = repoRoot / "projects" / "sponza_demo" / "project.hive";
            const auto reportPath =
                repoRoot / "projects" / "sponza_demo" / "assets" / ".nectar" / "headless_report.hive";

            larvae::AssertTrue(std::filesystem::exists(launcherPath));
            larvae::AssertTrue(std::filesystem::exists(projectPath));
            EnsureSponzaDemoBuilt(buildRoot, repoRoot);

            std::error_code ec;
            std::filesystem::remove(reportPath, ec);

            const int exitCode =
                RunProcess(launcherPath, {L"--headless", L"--project", projectPath.native()}, repoRoot, 30000);
            larvae::AssertEqual(exitCode, 0);
            larvae::AssertTrue(std::filesystem::exists(reportPath));

            const std::string reportContent = ReadTextFile(reportPath);
            larvae::AssertTrue(!reportContent.empty());

            auto& alloc = GetAlloc();
            const nectar::HiveParseResult parseResult =
                nectar::HiveParser::Parse(wax::StringView{reportContent.c_str(), reportContent.size()}, alloc);
            larvae::AssertTrue(parseResult.m_errors.IsEmpty());

            const nectar::HiveDocument& report = parseResult.m_document;
            larvae::AssertTrue(report.GetString("scenario", "source") == wax::StringView{"headless_simulation.hive"});
            larvae::AssertEqual(report.GetInt("scenario", "entity_count"), int64_t{3});
            larvae::AssertEqual(report.GetInt("scenario", "step_count"), int64_t{5});
            larvae::AssertEqual(report.GetInt("result", "entity_count"), int64_t{3});
            larvae::AssertEqual(report.GetInt("result", "completed_tick"), int64_t{5});
            const wax::StringView reportView{reportContent.c_str(), reportContent.size()};
            larvae::AssertTrue(reportView.Contains("position_sum = 45"));
            larvae::AssertTrue(reportView.Contains("min_position = 5"));
            larvae::AssertTrue(reportView.Contains("max_position = 25"));
#else
            larvae::AssertTrue(true);
#endif
        });

    auto t_launcher_explicit_mode_flag = larvae::RegisterTest("HiveLauncher", "explicit_mode_flag", []() {
#if HIVE_PLATFORM_WINDOWS
        const auto repoRoot = FindRepoRoot();
        larvae::AssertTrue(!repoRoot.empty());

        const auto launcherPath = GetCurrentExecutablePath().parent_path() / "hive_launcher.exe";
        const auto projectPath = repoRoot / "projects" / "sponza_demo" / "project.hive";
        larvae::AssertTrue(std::filesystem::exists(launcherPath));
        larvae::AssertTrue(std::filesystem::exists(projectPath));

#if HIVE_MODE_HEADLESS
        const int exitCode =
            RunProcess(launcherPath, {L"--editor", L"--project", projectPath.native()}, repoRoot, 10000);
        larvae::AssertEqual(exitCode, 1);
#elif HIVE_MODE_EDITOR
            const int exitCode = RunProcess(
                launcherPath, {L"--editor", L"--project", projectPath.native(), L"--exit-after-setup"}, repoRoot,
                30000);
            larvae::AssertEqual(exitCode, 0);
#else
            const int exitCode =
                RunProcess(launcherPath, {L"--game", L"--project", projectPath.native(), L"--exit-after-setup"},
                           repoRoot, 30000);
            larvae::AssertEqual(exitCode, 0);
#endif
#else
            larvae::AssertTrue(true);
#endif
    });
} // namespace
