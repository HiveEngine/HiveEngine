#include <larvae/larvae.h>
#include <nectar/project/project_file.h>
#include <comb/default_allocator.h>
#include <filesystem>
#include <cstdio>

namespace {

auto& GetAlloc()
{
    static comb::ModuleAllocator alloc{"TestProjectFile", 4 * 1024 * 1024};
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

// =========================================================================
// Parsing
// =========================================================================

auto t_minimal = larvae::RegisterTest("NectarProjectFile", "ParseMinimalProject", []() {
    auto& alloc = GetAlloc();
    nectar::ProjectFile pf{alloc};

    auto result = pf.Load("[project]\nname = \"TestApp\"\n");
    larvae::AssertTrue(result.success);
    larvae::AssertTrue(pf.Name().Equals("TestApp"));
    larvae::AssertTrue(pf.Version().IsEmpty());
    larvae::AssertTrue(pf.AssetsRelative().Equals("assets"));
    larvae::AssertTrue(pf.CacheRelative().Equals(".hive-cache"));
    larvae::AssertTrue(pf.SourceRelative().Equals("src"));
});

auto t_full = larvae::RegisterTest("NectarProjectFile", "ParseFullProject", []() {
    auto& alloc = GetAlloc();
    nectar::ProjectFile pf{alloc};

    const char* content =
        "[project]\n"
        "name = \"Sponza Demo\"\n"
        "version = \"1.0.0\"\n"
        "\n"
        "[paths]\n"
        "assets = \"data\"\n"
        "cache = \"build-cache\"\n"
        "source = \"code\"\n"
        "\n"
        "[engine]\n"
        "path = \"C:/Engine/HiveEngine\"\n"
        "\n"
        "[render]\n"
        "backend = \"vulkan\"\n";

    auto result = pf.Load(content);
    larvae::AssertTrue(result.success);
    larvae::AssertTrue(pf.Name().Equals("Sponza Demo"));
    larvae::AssertTrue(pf.Version().Equals("1.0.0"));
    larvae::AssertTrue(pf.EnginePath().Equals("C:/Engine/HiveEngine"));
    larvae::AssertTrue(pf.Backend().Equals("vulkan"));
    larvae::AssertTrue(pf.AssetsRelative().Equals("data"));
    larvae::AssertTrue(pf.CacheRelative().Equals("build-cache"));
    larvae::AssertTrue(pf.SourceRelative().Equals("code"));
});

auto t_missing_name = larvae::RegisterTest("NectarProjectFile", "ParseMissingName", []() {
    auto& alloc = GetAlloc();
    nectar::ProjectFile pf{alloc};

    auto result = pf.Load("[project]\nversion = \"1.0\"\n");
    larvae::AssertTrue(!result.success);
    larvae::AssertTrue(!result.errors.IsEmpty());
});

auto t_bad_syntax = larvae::RegisterTest("NectarProjectFile", "ParseBadSyntax", []() {
    auto& alloc = GetAlloc();
    nectar::ProjectFile pf{alloc};

    auto result = pf.Load("[project\nname = broken\n");
    larvae::AssertTrue(!result.success);
});

// =========================================================================
// Create + Serialize round-trip
// =========================================================================

auto t_roundtrip = larvae::RegisterTest("NectarProjectFile", "CreateAndSerialize", []() {
    auto& alloc = GetAlloc();

    nectar::ProjectFile pf1{alloc};
    nectar::ProjectDesc desc{};
    desc.name = "RoundTrip";
    desc.version = "2.0.0";
    desc.engine_path = "C:/Dev/Engine";
    desc.backend = "d3d12";
    pf1.Create(desc);

    larvae::AssertTrue(pf1.Name().Equals("RoundTrip"));
    larvae::AssertTrue(pf1.Version().Equals("2.0.0"));

    wax::String<> serialized = pf1.Serialize();
    larvae::AssertTrue(serialized.Size() > 0);

    nectar::ProjectFile pf2{alloc};
    auto result = pf2.Load(serialized.View());
    larvae::AssertTrue(result.success);
    larvae::AssertTrue(pf2.Name().Equals("RoundTrip"));
    larvae::AssertTrue(pf2.Version().Equals("2.0.0"));
    larvae::AssertTrue(pf2.EnginePath().Equals("C:/Dev/Engine"));
    larvae::AssertTrue(pf2.Backend().Equals("d3d12"));
});

// =========================================================================
// ResolvePaths
// =========================================================================

auto t_resolve = larvae::RegisterTest("NectarProjectFile", "ResolvePaths", []() {
    auto& alloc = GetAlloc();
    nectar::ProjectFile pf{alloc};

    const char* content =
        "[project]\n"
        "name = \"PathTest\"\n"
        "\n"
        "[paths]\n"
        "assets = \"myassets\"\n"
        "cache = \".cache\"\n"
        "source = \"src\"\n";

    auto result = pf.Load(content);
    larvae::AssertTrue(result.success);

    auto paths = pf.ResolvePaths("C:/Projects/Game");
    larvae::AssertTrue(paths.root.View().Equals("C:/Projects/Game"));
    larvae::AssertTrue(paths.assets.View().Equals("C:/Projects/Game/myassets"));
    larvae::AssertTrue(paths.cache.View().Equals("C:/Projects/Game/.cache"));
    larvae::AssertTrue(paths.source.View().Equals("C:/Projects/Game/src"));
    larvae::AssertTrue(paths.cas.View().Equals("C:/Projects/Game/.cache/cas"));
    larvae::AssertTrue(paths.import_cache.View().Equals("C:/Projects/Game/.cache/import_cache.bin"));
});

auto t_resolve_backslash = larvae::RegisterTest("NectarProjectFile", "ResolvePathsNormalizesBackslashes", []() {
    auto& alloc = GetAlloc();
    nectar::ProjectFile pf{alloc};

    auto result = pf.Load("[project]\nname = \"SlashTest\"\n");
    larvae::AssertTrue(result.success);

    auto paths = pf.ResolvePaths("C:\\Users\\dev\\project");
    larvae::AssertTrue(paths.root.View().Equals("C:/Users/dev/project"));
    larvae::AssertTrue(paths.assets.View().Equals("C:/Users/dev/project/assets"));
});

// =========================================================================
// Default values
// =========================================================================

auto t_defaults = larvae::RegisterTest("NectarProjectFile", "DefaultValues", []() {
    auto& alloc = GetAlloc();
    nectar::ProjectFile pf{alloc};

    nectar::ProjectDesc desc{};
    desc.name = "Defaults";
    pf.Create(desc);

    larvae::AssertTrue(pf.AssetsRelative().Equals("assets"));
    larvae::AssertTrue(pf.CacheRelative().Equals(".hive-cache"));
    larvae::AssertTrue(pf.SourceRelative().Equals("src"));
    larvae::AssertTrue(pf.EnginePath().IsEmpty());
    larvae::AssertTrue(pf.Backend().IsEmpty());
});

// =========================================================================
// Disk I/O
// =========================================================================

auto t_disk = larvae::RegisterTest("NectarProjectFile", "SaveAndLoadFromDisk", []() {
    auto& alloc = GetAlloc();
    TempDir dir{"nectar_project_test"};

    nectar::ProjectFile pf1{alloc};
    nectar::ProjectDesc desc{};
    desc.name = "DiskTest";
    desc.version = "3.0.0";
    desc.backend = "vulkan";
    pf1.Create(desc);

    auto file_path = dir.path / "project.hive";
    std::string path_str = file_path.string();
    wax::StringView path_sv{path_str.c_str()};

    bool saved = pf1.SaveToDisk(path_sv);
    larvae::AssertTrue(saved);

    nectar::ProjectFile pf2{alloc};
    auto result = pf2.LoadFromDisk(path_sv);
    larvae::AssertTrue(result.success);
    larvae::AssertTrue(pf2.Name().Equals("DiskTest"));
    larvae::AssertTrue(pf2.Version().Equals("3.0.0"));
    larvae::AssertTrue(pf2.Backend().Equals("vulkan"));
});

// =========================================================================
// Custom sections preserved
// =========================================================================

auto t_custom_sections = larvae::RegisterTest("NectarProjectFile", "CustomSectionsPreserved", []() {
    auto& alloc = GetAlloc();
    nectar::ProjectFile pf{alloc};

    const char* content =
        "[project]\n"
        "name = \"Custom\"\n"
        "\n"
        "[import.textures]\n"
        "max_size = 2048\n"
        "format = \"bc7\"\n";

    auto result = pf.Load(content);
    larvae::AssertTrue(result.success);

    larvae::AssertTrue(pf.Document().HasSection("import.textures"));
    auto max_size = pf.Document().GetInt("import.textures", "max_size");
    larvae::AssertEqual(max_size, int64_t{2048});
    larvae::AssertTrue(pf.Document().GetString("import.textures", "format").Equals("bc7"));
});

} // namespace
