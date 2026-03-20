#include <hive/core/module.h>

#include <larvae/larvae.h>

#include <string>
#include <unordered_set>

namespace
{

    // Dummy module with no dependencies
    class TestModule : public hive::Module
    {
    public:
        static const char* GetStaticName() { return "TestModule"; }
        const char* GetName() const override { return GetStaticName(); }

        bool m_configured{false};
        bool m_initialized{false};
        bool m_shutdown{false};

    protected:
        void DoConfigure(hive::ModuleContext&) override { m_configured = true; }
        void DoInitialize() override { m_initialized = true; }
        void DoShutdown() override { m_shutdown = true; }
    };

    // Module that declares a dependency on TestModule
    class DependentModule : public hive::Module
    {
    public:
        static const char* GetStaticName() { return "DependentModule"; }
        const char* GetName() const override { return GetStaticName(); }

    protected:
        void DoConfigure(hive::ModuleContext& ctx) override { ctx.AddDependency<TestModule>(); }
    };

    // Module lifecycle

    auto t_initial_state = larvae::RegisterTest("HiveModule", "InitiallyNotInitialized", []() {
        TestModule mod;
        larvae::AssertFalse(mod.IsInitialized());
    });

    auto t_configure = larvae::RegisterTest("HiveModule", "ConfigureCalled", []() {
        TestModule mod;
        mod.Configure();
        larvae::AssertTrue(mod.m_configured);
        larvae::AssertFalse(mod.IsInitialized());
    });

    auto t_initialize = larvae::RegisterTest("HiveModule", "InitializeSetsFlag", []() {
        TestModule mod;
        mod.Initialize();
        larvae::AssertTrue(mod.IsInitialized());
        larvae::AssertTrue(mod.m_initialized);
    });

    auto t_shutdown = larvae::RegisterTest("HiveModule", "ShutdownClearsFlag", []() {
        TestModule mod;
        mod.Initialize();
        larvae::AssertTrue(mod.IsInitialized());

        mod.Shutdown();
        larvae::AssertFalse(mod.IsInitialized());
        larvae::AssertTrue(mod.m_shutdown);
    });

    // CanInitialize (dependency resolution)

    auto t_no_deps = larvae::RegisterTest("HiveModule", "CanInitializeWithNoDeps", []() {
        TestModule mod;
        mod.Configure();
        std::unordered_set<std::string> initialized;
        larvae::AssertTrue(mod.CanInitialize(initialized));
    });

    auto t_dep_missing = larvae::RegisterTest("HiveModule", "CannotInitializeWithMissingDep", []() {
        DependentModule mod;
        mod.Configure();
        std::unordered_set<std::string> initialized;
        larvae::AssertFalse(mod.CanInitialize(initialized));
    });

    auto t_dep_satisfied = larvae::RegisterTest("HiveModule", "CanInitializeWithSatisfiedDep", []() {
        DependentModule mod;
        mod.Configure();
        std::unordered_set<std::string> initialized{"TestModule"};
        larvae::AssertTrue(mod.CanInitialize(initialized));
    });

} // namespace
