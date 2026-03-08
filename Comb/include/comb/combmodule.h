#pragma once

#include <hive/core/log.h>
#include <hive/core/module.h>

namespace comb
{
    extern const hive::LogCategory LOG_COMB_ROOT;

    class CombModule : public hive::Module
    {
    public:
        CombModule() = default;
        ~CombModule() override = default;

        const char* GetName() const override { return GetStaticName(); }
        static const char* GetStaticName() { return "Comb"; }

    protected:
        void DoConfigure(hive::ModuleContext& context) override;
        void DoInitialize() override;
        void DoShutdown() override;
    };
} // namespace comb
