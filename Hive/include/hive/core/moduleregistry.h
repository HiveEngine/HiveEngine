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

        using ModuleFactoryFn = std::unique_ptr<Module> (*)();
        HIVE_API void RegisterModule(ModuleFactoryFn fn);
        HIVE_API void CreateModules();
        HIVE_API void ConfigureModules();
        HIVE_API void InitModules();
        HIVE_API void ShutdownModules();

    private:
        std::vector<ModuleFactoryFn> m_moduleFactories;
        std::vector<std::unique_ptr<Module>> m_modules;
    };

    template <typename ModuleClass> class ModuleRegistrar
    {
    public:
        ModuleRegistrar()
        {
            ModuleRegistry::GetInstance().RegisterModule(
                []() -> std::unique_ptr<Module> { return std::make_unique<ModuleClass>(); });
        }
    };
} // namespace hive

#define REGISTER_MODULE(ModuleClass)                                                                                   \
    inline void Register##ModuleClass()                                                                                       \
    {                                                                                                                  \
        hive::ModuleRegistry::GetInstance().RegisterModule(                                                            \
            []() -> std::unique_ptr<hive::Module> { return std::make_unique<ModuleClass>(); });                        \
    }
