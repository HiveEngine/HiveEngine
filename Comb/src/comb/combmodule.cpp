#include <comb/combmodule.h>

#include <hive/core/log.h>

#include <comb/precomp.h>

namespace comb
{
    const hive::LogCategory LOG_COMB_ROOT{"Comb", &hive::LOG_HIVE_ROOT};

    void CombModule::DoConfigure([[maybe_unused]] hive::ModuleContext& context)
    {
        hive::LogInfo(hive::LOG_HIVE_ROOT, "Comb module configured");
    }

    void CombModule::DoInitialize()
    {
        hive::LogInfo(hive::LOG_HIVE_ROOT, "Comb module initialized");
    }

    void CombModule::DoShutdown()
    {
        hive::LogInfo(hive::LOG_HIVE_ROOT, "Comb module shutdown");
    }
} // namespace comb
