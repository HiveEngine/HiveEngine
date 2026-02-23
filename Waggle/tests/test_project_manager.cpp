#include <larvae/larvae.h>
#include <waggle/project/project_manager.h>
#include <nectar/project/project_file.h>
#include <nectar/vfs/virtual_filesystem.h>
#include <nectar/database/asset_database.h>
#include <nectar/pipeline/hot_reload.h>
#include <comb/default_allocator.h>

#include <filesystem>
#include <fstream>

namespace {

auto& GetAlloc()
{
    static comb::ModuleAllocator alloc{"TestProjectManager", 64 * 1024 * 1024};
    return alloc.Get();
}

struct TempDir
{
    std::filesystem::path path;
    explicit TempDir(const char* name)
    {
        path = std::filesystem::temp_directory_path() / name;
        std::error_code ec;
        std::filesystem::remove_all(path, ec);
        std::filesystem::create_directories(path);
    }
    ~TempDir()
    {
        std::error_code ec;
        std::filesystem::remove_all(path, ec);
    }
};

void WriteProjectHive(const std::filesystem::path& dir, const char* name = "TestProject")
{
    auto& alloc = GetAlloc();
    nectar::ProjectFile pf{alloc};
    pf.Create(nectar::ProjectDesc{
        .name = name,
        .version = "1.0.0",
        .engine_path = "",
        .backend = "vulkan"
    });
    auto file_path = dir / "project.hive";
    bool saved = pf.SaveToDisk(file_path.generic_string().c_str());
    (void)saved;

    std::filesystem::create_directories(dir / "assets");
}

std::string ProjectHivePath(const TempDir& dir)
{
    return (dir.path / "project.hive").generic_string();
}

// =========================================================================
// Open / Close
// =========================================================================

auto t_open = larvae::RegisterTest("WaggleProjectManager", "OpenValidProject", []() {
    TempDir dir{"waggle_pm_open"};
    WriteProjectHive(dir.path);

    auto& alloc = GetAlloc();
    waggle::ProjectManager pm{alloc};

    larvae::AssertFalse(pm.IsOpen());
    larvae::AssertTrue(pm.Open(ProjectHivePath(dir).c_str()));
    larvae::AssertTrue(pm.IsOpen());
});

auto t_open_invalid = larvae::RegisterTest("WaggleProjectManager", "OpenInvalidPath", []() {
    auto& alloc = GetAlloc();
    waggle::ProjectManager pm{alloc};

    larvae::AssertFalse(pm.Open("__nonexistent__/project.hive"));
    larvae::AssertFalse(pm.IsOpen());
});

auto t_open_malformed = larvae::RegisterTest("WaggleProjectManager", "OpenMalformedFile", []() {
    TempDir dir{"waggle_pm_malformed"};

    auto file_path = dir.path / "project.hive";
    {
        std::ofstream f{file_path};
        f << "this is not valid hive format {{{";
    }

    auto& alloc = GetAlloc();
    waggle::ProjectManager pm{alloc};
    larvae::AssertFalse(pm.Open(file_path.generic_string().c_str()));
    larvae::AssertFalse(pm.IsOpen());
});

// =========================================================================
// Cache directories
// =========================================================================

auto t_cache_dirs = larvae::RegisterTest("WaggleProjectManager", "CreateCacheDirectories", []() {
    TempDir dir{"waggle_pm_cache"};
    WriteProjectHive(dir.path);

    auto& alloc = GetAlloc();
    waggle::ProjectManager pm{alloc};
    larvae::AssertTrue(pm.Open(ProjectHivePath(dir).c_str()));

    larvae::AssertTrue(std::filesystem::exists(dir.path / ".hive-cache"));
    larvae::AssertTrue(std::filesystem::exists(dir.path / ".hive-cache" / "cas"));
});

// =========================================================================
// VFS
// =========================================================================

auto t_vfs = larvae::RegisterTest("WaggleProjectManager", "VFSMountedCorrectly", []() {
    TempDir dir{"waggle_pm_vfs"};
    WriteProjectHive(dir.path);

    {
        std::ofstream f{(dir.path / "assets" / "test.txt").string()};
        f << "hello";
    }

    auto& alloc = GetAlloc();
    waggle::ProjectManager pm{alloc};
    larvae::AssertTrue(pm.Open(ProjectHivePath(dir).c_str()));

    larvae::AssertTrue(pm.VFS().Exists("test.txt"));
    larvae::AssertFalse(pm.VFS().Exists("nonexistent.txt"));
});

// =========================================================================
// Close and reopen
// =========================================================================

auto t_close_reopen = larvae::RegisterTest("WaggleProjectManager", "CloseAndReopen", []() {
    TempDir dir{"waggle_pm_reopen"};
    WriteProjectHive(dir.path);

    auto& alloc = GetAlloc();
    waggle::ProjectManager pm{alloc};

    larvae::AssertTrue(pm.Open(ProjectHivePath(dir).c_str()));
    larvae::AssertTrue(pm.IsOpen());

    pm.Close();
    larvae::AssertFalse(pm.IsOpen());

    larvae::AssertTrue(pm.Open(ProjectHivePath(dir).c_str()));
    larvae::AssertTrue(pm.IsOpen());
});

// =========================================================================
// Hot reload
// =========================================================================

auto t_hot_reload_off = larvae::RegisterTest("WaggleProjectManager", "HotReloadDisabledByDefault", []() {
    TempDir dir{"waggle_pm_hr_off"};
    WriteProjectHive(dir.path);

    auto& alloc = GetAlloc();
    waggle::ProjectManager pm{alloc};
    larvae::AssertTrue(pm.Open(ProjectHivePath(dir).c_str()));
    larvae::AssertNull(pm.HotReload());
});

auto t_hot_reload_on = larvae::RegisterTest("WaggleProjectManager", "HotReloadEnabled", []() {
    TempDir dir{"waggle_pm_hr_on"};
    WriteProjectHive(dir.path);

    auto& alloc = GetAlloc();
    waggle::ProjectManager pm{alloc};

    waggle::ProjectConfig config{.enable_hot_reload = true};
    larvae::AssertTrue(pm.Open(ProjectHivePath(dir).c_str(), config));
    larvae::AssertNotNull(pm.HotReload());
});

} // namespace
