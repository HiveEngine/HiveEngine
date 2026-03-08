#include <larvae/larvae.h>

#include <filesystem>
#include <string>
#include <vector>

#if HIVE_PLATFORM_WINDOWS
#include <Windows.h>
#endif

namespace
{
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

    auto t_launcher_headless_process_smoke =
        larvae::RegisterTest("HiveLauncher", "headless_exit_after_setup_process_smoke", []() {
#if HIVE_PLATFORM_WINDOWS
            const auto repoRoot = FindRepoRoot();
            larvae::AssertTrue(!repoRoot.empty());

            const auto launcherPath = GetCurrentExecutablePath().parent_path() / "hive_launcher.exe";
            const auto projectPath = repoRoot / "projects" / "sponza_demo" / "project.hive";

            larvae::AssertTrue(std::filesystem::exists(launcherPath));
            larvae::AssertTrue(std::filesystem::exists(projectPath));

            const int exitCode =
                RunProcess(launcherPath, {L"--headless", L"--exit-after-setup", projectPath.native()}, repoRoot, 30000);
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

            const int exitCode = RunProcess(launcherPath, {missingProjectPath.native()}, repoRoot, 10000);
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
} // namespace
