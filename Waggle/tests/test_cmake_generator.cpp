#include <comb/default_allocator.h>

#include <waggle/project/cmake_generator.h>

#include <larvae/larvae.h>

#include <filesystem>
#include <fstream>

namespace
{

    auto& GetAlloc()
    {
        static comb::ModuleAllocator alloc{"TestCMakeGen", 4 * 1024 * 1024};
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
    // Generate
    // =========================================================================

    auto t_minimal = larvae::RegisterTest("WaggleCMakeGenerator", "GenerateMinimal", []() {
        auto& alloc = GetAlloc();
        waggle::CMakeGenConfig config{
            .m_projectName = "TestApp", .m_projectRoot = "/tmp", .m_enginePath = "C:/Engine/HiveEngine"};

        auto result = waggle::CMakeGenerator::Generate(config, alloc);
        wax::StringView view = result.View();

        larvae::AssertTrue(view.Contains("project(TestApp LANGUAGES CXX)"));
        larvae::AssertTrue(view.Contains("cmake_minimum_required(VERSION 3.28)"));
        larvae::AssertTrue(view.Contains("set(CMAKE_CXX_STANDARD 20)"));
    });

    auto t_engine_path = larvae::RegisterTest("WaggleCMakeGenerator", "ContainsEnginePath", []() {
        auto& alloc = GetAlloc();
        waggle::CMakeGenConfig config{
            .m_projectName = "MyGame", .m_projectRoot = "/tmp", .m_enginePath = "D:/Dev/HiveEngine"};

        auto result = waggle::CMakeGenerator::Generate(config, alloc);
        wax::StringView view = result.View();

        larvae::AssertTrue(view.Contains("D:/Dev/HiveEngine"));
        larvae::AssertTrue(view.Contains("add_subdirectory(${HIVE_ENGINE_DIR}"));
    });

    auto t_core_libs = larvae::RegisterTest("WaggleCMakeGenerator", "ContainsCoreLibs", []() {
        auto& alloc = GetAlloc();
        waggle::CMakeGenConfig config{.m_projectName = "TestApp", .m_projectRoot = "/tmp", .m_enginePath = "C:/Engine"};

        auto result = waggle::CMakeGenerator::Generate(config, alloc);
        wax::StringView view = result.View();

        larvae::AssertTrue(view.Contains("Queen Waggle Hive Comb Wax Nectar"));
        larvae::AssertFalse(view.Contains("Swarm"));
        larvae::AssertFalse(view.Contains("Terra"));
        larvae::AssertFalse(view.Contains("Antennae"));
    });

    auto t_optional_swarm = larvae::RegisterTest("WaggleCMakeGenerator", "OptionalSwarm", []() {
        auto& alloc = GetAlloc();
        waggle::CMakeGenConfig config{.m_projectName = "RenderApp",
                                      .m_projectRoot = "/tmp",
                                      .m_enginePath = "C:/Engine",
                                      .m_linkSwarm = true,
                                      .m_linkTerra = true,
                                      .m_linkAntennae = true};

        auto result = waggle::CMakeGenerator::Generate(config, alloc);
        wax::StringView view = result.View();

        larvae::AssertTrue(view.Contains("Swarm"));
        larvae::AssertTrue(view.Contains("Terra"));
        larvae::AssertTrue(view.Contains("Antennae"));
    });

    // =========================================================================
    // WriteToProject
    // =========================================================================

    auto t_write = larvae::RegisterTest("WaggleCMakeGenerator", "WriteToProject", []() {
        TempDir dir{"waggle_cmake_gen"};
        auto& alloc = GetAlloc();

        auto root_str = dir.path.generic_string();
        waggle::CMakeGenConfig config{.m_projectName = "WriteTest",
                                      .m_projectRoot = wax::StringView{root_str.c_str(), root_str.size()},
                                      .m_enginePath = "C:/Engine"};

        larvae::AssertTrue(waggle::CMakeGenerator::WriteToProject(config, alloc));

        auto cmake_path = dir.path / "CMakeLists.txt";
        larvae::AssertTrue(std::filesystem::exists(cmake_path));

        std::ifstream file{cmake_path.string()};
        std::string content{std::istreambuf_iterator<char>{file}, {}};
        larvae::AssertTrue(wax::StringView{content.c_str(), content.size()}.Contains("cmake_minimum_required"));
        larvae::AssertTrue(wax::StringView{content.c_str(), content.size()}.Contains("project(WriteTest"));
    });

} // namespace
