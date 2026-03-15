#pragma once
#include <hive/core/module.h>
#include <hive/core/moduleregistry.h>
#include <hive/utils/singleton.h>

#include <comb/default_allocator.h>
namespace terra
{
    class TerraModule : public hive::Module, public hive::Singleton<TerraModule>
    {
    public:
        TerraModule();
        ~TerraModule() override = default;
        [[nodiscard]] const char* GetName() const override
        {
            return "Terra";
        }

        comb::DefaultAllocator& GetAllocator()
        {
            return m_moduleAllocator.Get();
        }

    private:
        comb::ModuleAllocator m_moduleAllocator;
    };

 
    REGISTER_MODULE(TerraModule)
} // namespace terra