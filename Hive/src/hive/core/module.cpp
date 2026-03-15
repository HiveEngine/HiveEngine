#include <hive/core/module.h>
#include <hive/precomp.h>

#include <hive/core/log.h>

namespace hive
{
    void Module::Configure()
    {
        hive::LogTrace(LOG_HIVE_ROOT, "Configuring module: {}", GetName());
        DoConfigure(m_context);
    }

    void Module::Initialize()
    {
        hive::LogTrace(LOG_HIVE_ROOT, "Initializing module: {}", GetName());
        DoInitialize();
    }

    void Module::Shutdown()
    {
        hive::LogTrace(LOG_HIVE_ROOT, "Shutdown module: {}", GetName());
        DoShutdown();
    }

    bool Module::CanInitialize(const std::unordered_set<std::string>& initModulesNames) const
    {
        int depCount{0};
        for (auto depName : m_context.GetDependencies())
        {
            if (initModulesNames.find(depName) != initModulesNames.end())
            {
                depCount++;
            }
        }

        return depCount == static_cast<int>(m_context.GetDependencies().size());
    }
} // namespace hive
