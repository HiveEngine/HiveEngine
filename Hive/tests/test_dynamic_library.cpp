#include <larvae/larvae.h>
#include <hive/platform/dynamic_library.h>
#include <hive/HiveConfig.h>

#include <cstring>
#include <utility>

namespace {

using hive::DynamicLibrary;

// =========================================================================
// Construction & basic state
// =========================================================================

auto t_default = larvae::RegisterTest("HiveDynamicLibrary", "DefaultConstruction", []() {
    DynamicLibrary lib;
    larvae::AssertFalse(lib.IsLoaded());
    larvae::AssertEqual(lib.GetError()[0], '\0');
});

auto t_load_nonexistent = larvae::RegisterTest("HiveDynamicLibrary", "LoadNonExistent", []() {
    DynamicLibrary lib;
    bool ok = lib.Load("__nonexistent_library_42__.dll");
    larvae::AssertFalse(ok);
    larvae::AssertFalse(lib.IsLoaded());
    larvae::AssertTrue(std::strlen(lib.GetError()) > 0);
});

auto t_symbol_before_load = larvae::RegisterTest("HiveDynamicLibrary", "GetSymbolBeforeLoad", []() {
    DynamicLibrary lib;
    void* sym = lib.GetSymbol("anything");
    larvae::AssertNull(sym);
});

// =========================================================================
// Move semantics
// =========================================================================

auto t_move_ctor = larvae::RegisterTest("HiveDynamicLibrary", "MoveConstructor", []() {
    DynamicLibrary lib;
#if HIVE_PLATFORM_WINDOWS
    larvae::AssertTrue(lib.Load("kernel32.dll"));
#elif HIVE_PLATFORM_LINUX
    larvae::AssertTrue(lib.Load("libc.so.6"));
#endif

    DynamicLibrary moved{std::move(lib)};
    larvae::AssertTrue(moved.IsLoaded());
    larvae::AssertFalse(lib.IsLoaded());
});

auto t_move_assign = larvae::RegisterTest("HiveDynamicLibrary", "MoveAssignment", []() {
    DynamicLibrary lib;
#if HIVE_PLATFORM_WINDOWS
    larvae::AssertTrue(lib.Load("kernel32.dll"));
#elif HIVE_PLATFORM_LINUX
    larvae::AssertTrue(lib.Load("libc.so.6"));
#endif

    DynamicLibrary other;
    other = std::move(lib);
    larvae::AssertTrue(other.IsLoaded());
    larvae::AssertFalse(lib.IsLoaded());
});

// =========================================================================
// Load system library + symbol lookup
// =========================================================================

auto t_load_system = larvae::RegisterTest("HiveDynamicLibrary", "LoadSystemLibrary", []() {
    DynamicLibrary lib;
#if HIVE_PLATFORM_WINDOWS
    bool ok = lib.Load("kernel32.dll");
#elif HIVE_PLATFORM_LINUX
    bool ok = lib.Load("libc.so.6");
#endif
    larvae::AssertTrue(ok);
    larvae::AssertTrue(lib.IsLoaded());
});

auto t_known_symbol = larvae::RegisterTest("HiveDynamicLibrary", "GetKnownSymbol", []() {
    DynamicLibrary lib;
#if HIVE_PLATFORM_WINDOWS
    larvae::AssertTrue(lib.Load("kernel32.dll"));
    void* sym = lib.GetSymbol("GetProcAddress");
#elif HIVE_PLATFORM_LINUX
    larvae::AssertTrue(lib.Load("libc.so.6"));
    void* sym = lib.GetSymbol("strlen");
#endif
    larvae::AssertNotNull(sym);
});

// =========================================================================
// Unload
// =========================================================================

auto t_unload_twice = larvae::RegisterTest("HiveDynamicLibrary", "UnloadTwice", []() {
    DynamicLibrary lib;
#if HIVE_PLATFORM_WINDOWS
    larvae::AssertTrue(lib.Load("kernel32.dll"));
#elif HIVE_PLATFORM_LINUX
    larvae::AssertTrue(lib.Load("libc.so.6"));
#endif
    larvae::AssertTrue(lib.IsLoaded());
    lib.Unload();
    larvae::AssertFalse(lib.IsLoaded());
    lib.Unload();
    larvae::AssertFalse(lib.IsLoaded());
});

} // namespace
