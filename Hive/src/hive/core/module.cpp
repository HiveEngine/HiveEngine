#include <hive/core/module.h>
#include <hive/precomp.h>

namespace hive
{
    void Module::Configure()
    {
        DoConfigure(m_context);
    }

    void Module::Initialize()
    {
        DoInitialize();
    }

    void Module::Shutdown()
    {
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
