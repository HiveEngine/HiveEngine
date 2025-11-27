#include <comb/precomp.h>
#include <comb/combmodule.h>
#include <hive/core/log.h>

namespace comb
{
    const hive::LogCategory LogCombRoot{"Comb", &hive::LogHiveRoot};

    void CombModule::DoConfigure([[maybe_unused]] hive::ModuleContext& context)
    {
        // Add dependencies here
        // Example: context.AddDependency<OtherModule>();

        hive::LogInfo(hive::LogHiveRoot, "Comb module configured");
    }

    void CombModule::DoInitialize()
    {
        hive::LogInfo(hive::LogHiveRoot, "Comb module initialized");

        // Initialize your module here
    }

    void CombModule::DoShutdown()
    {
        hive::LogInfo(hive::LogHiveRoot, "Comb module shutdown");

        // Cleanup your module here
    }
}
