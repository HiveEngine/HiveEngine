#include <hive/precomp.h>
#include <hive/core/moduleregistry.h>

namespace hive
{
    void ModuleRegistry::RegisterModule(ModuleFactoryFn fn)
    {
        m_ModuleFactories.push_back(fn);
    }

    void ModuleRegistry::CreateModules()
    {
        const auto createModuleFromFactory = [this](const auto &fn)
        {
            m_Modules.push_back(fn());
        };

        std::for_each(m_ModuleFactories.begin(), m_ModuleFactories.end(), createModuleFromFactory);    }

    void ModuleRegistry::ConfigureModules()
    {
        std::for_each(m_Modules.begin(), m_Modules.end(),
              [](auto &module)
              { module->Configure(); });

        std::unordered_set<std::string> initializedModules;
        std::vector<std::unique_ptr<Module>> orderedModules;

        std::vector<std::unique_ptr<Module>> remainingModules = std::move(m_Modules);

        while (!remainingModules.empty())
        {
            const auto canModuleInitialize = [&initializedModules](const auto &module)
            {
                return module->CanInitialize(initializedModules);
            };

            auto it = std::find_if(
                remainingModules.begin(),
                remainingModules.end(),
                canModuleInitialize);

            if (it == remainingModules.end())
            {
                throw std::runtime_error("Unresolvable module dependency detected.");
            }

            initializedModules.insert((*it)->GetName());
            orderedModules.push_back(std::move(*it));
            remainingModules.erase(it);
        }

        m_Modules = std::move(orderedModules);
    }

    void ModuleRegistry::InitModules()
    {
        const auto moduleInit = [](const auto &module)
        {
            module->Initialize();
        };

        std::for_each(m_Modules.begin(), m_Modules.end(), moduleInit);
    }

    void ModuleRegistry::ShutdownModules()
    {
        const auto moduleShutdown = [](const auto &module)
        {
            module->Shutdown();
        };

        std::for_each(m_Modules.rbegin(), m_Modules.rend(), moduleShutdown);
    }
}
