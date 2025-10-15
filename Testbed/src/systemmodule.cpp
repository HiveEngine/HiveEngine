#include <testbed/precomp.h>
#include <testbed/systemmodule.h>

#include <hive/core/moduleregistry.h>

void RegisterSystemModule()
{
    hive::ModuleRegistry::GetInstance().RegisterModule([]() -> std::unique_ptr<hive::Module>
    {
        return std::make_unique<SystemModule>();
    });
}

SystemModule::SystemModule() : m_Logger(m_LogManager)
{
}

void SystemModule::DoInitialize()
{
    Module::DoInitialize();
}

void SystemModule::DoShutdown()
{
    Module::DoShutdown();
}
