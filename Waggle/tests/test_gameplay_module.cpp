#include <larvae/larvae.h>
#include <waggle/project/gameplay_module.h>

#include <cstring>
#include <utility>

namespace {

// =========================================================================
// Default state
// =========================================================================

auto t_default = larvae::RegisterTest("WaggleGameplayModule", "DefaultState", []() {
    waggle::GameplayModule mod;
    larvae::AssertFalse(mod.IsLoaded());
    larvae::AssertFalse(mod.IsRegistered());
    larvae::AssertEqual(std::strlen(mod.Version()), size_t{0});
});

// =========================================================================
// Load non-existent
// =========================================================================

auto t_load_fail = larvae::RegisterTest("WaggleGameplayModule", "LoadNonExistent", []() {
    waggle::GameplayModule mod;
    larvae::AssertFalse(mod.Load("__nonexistent_gameplay_42__.dll"));
    larvae::AssertFalse(mod.IsLoaded());
    larvae::AssertTrue(std::strlen(mod.GetError()) > 0);
});

// =========================================================================
// Move semantics
// =========================================================================

auto t_move_ctor = larvae::RegisterTest("WaggleGameplayModule", "MoveConstructor", []() {
    waggle::GameplayModule mod;
    waggle::GameplayModule moved{std::move(mod)};
    larvae::AssertFalse(moved.IsLoaded());
    larvae::AssertFalse(moved.IsRegistered());
    larvae::AssertFalse(mod.IsLoaded());
    larvae::AssertFalse(mod.IsRegistered());
});

auto t_move_assign = larvae::RegisterTest("WaggleGameplayModule", "MoveAssignment", []() {
    waggle::GameplayModule mod;
    waggle::GameplayModule other;
    other = std::move(mod);
    larvae::AssertFalse(other.IsLoaded());
    larvae::AssertFalse(other.IsRegistered());
    larvae::AssertFalse(mod.IsLoaded());
    larvae::AssertFalse(mod.IsRegistered());
});

} // namespace
