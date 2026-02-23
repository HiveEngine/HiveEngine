#include <comb/precomp.h>
#include <comb/combmodule.h>
#include <hive/core/log.h>

namespace comb
{
    const hive::LogCategory LogCombRoot{"Comb", &hive::LogHiveRoot};

    void CombModule::DoConfigure([[maybe_unused]] hive::ModuleContext& context)
    {
        hive::LogInfo(hive::LogHiveRoot, "Comb module configured");
    }

    void CombModule::DoInitialize()
    {
        hive::LogInfo(hive::LogHiveRoot, "Comb module initialized");
    }

    void CombModule::DoShutdown()
    {
        hive::LogInfo(hive::LogHiveRoot, "Comb module shutdown");
    }
}
