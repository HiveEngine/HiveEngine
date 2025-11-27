#pragma once
#include <hive/core/module.h>
#include <hive/utils/singleton.h>

#include <memory>
#include <vector>
namespace hive
{

    class ModuleRegistry : public Singleton<ModuleRegistry>
    {
    public:
        ModuleRegistry() = default;
        ~ModuleRegistry() = default;

        using ModuleFactoryFn = std::unique_ptr<Module>(*)();
        void RegisterModule(ModuleFactoryFn fn);

        void CreateModules();
        void ConfigureModules();
        void InitModules();
        void ShutdownModules();

    private:
        std::vector<ModuleFactoryFn> m_ModuleFactories;
        std::vector<std::unique_ptr<Module>> m_Modules;
    };

    template<typename ModuleClass>
    class ModuleRegistrar
    {
    public:
        ModuleRegistrar()
        {
            ModuleRegistry::GetInstance().RegisterModule([]() -> std::unique_ptr<Module>
            {
                return std::make_unique<ModuleClass>();
            });
        }
    };
}

#define REGISTER_MODULE(ModuleClass)                                                                \
    void Register##ModuleClass()                                                                    \
    {                                                                                               \
        hive::ModuleRegistry::GetInstance().RegisterModule([]() -> std::unique_ptr<hive::Module>    \
        {                                                                                           \
            return std::make_unique<ModuleClass>();                                                 \
        });                                                                                         \
    }                                                                                               \

