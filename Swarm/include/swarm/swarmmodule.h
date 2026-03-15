#pragma once
#include <hive/core/module.h>
#include <hive/core/moduleregistry.h>

#include <hive/utils/singleton.h>

#include <comb/default_allocator.h>
namespace swarm
{
    class SwarmModule : public hive::Module, public hive::Singleton<SwarmModule>
    {
    public:
        SwarmModule();
        ~SwarmModule() override = default;

        [[nodiscard]] const char* GetName() const override
        {
            return "Swarm";
        }

        comb::DefaultAllocator& GetAllocator()
        {
            return m_moduleAllocator.Get();
        }

    private:
        comb::ModuleAllocator m_moduleAllocator;
    };

    REGISTER_MODULE(SwarmModule)
}