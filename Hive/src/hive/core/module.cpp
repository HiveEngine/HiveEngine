#include <hive/precomp.h>
#include <hive/core/module.h>

namespace hive
{
    void Module::Configure()
    {
        DoConfigure(m_Context);
    }

    void Module::Initialize()
    {
        DoInitialize();
    }

    void Module::Shutdown()
    {
        DoShutdown();
    }

    bool Module::CanInitialize(const std::unordered_set<std::string> &initModulesNames) const
    {
        int depCount {0};
        for (auto depName : m_Context.GetDependencies())
        {
            if (initModulesNames.find(depName) != initModulesNames.end())
            {
                depCount++;
            }
        }

        return depCount == m_Context.GetDependencies().size();
    }
}
