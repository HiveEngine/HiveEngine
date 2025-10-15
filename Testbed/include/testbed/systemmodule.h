#pragma once
#include <hive/core/module.h>
#include <hive/core/log.h>
class SystemModule : public hive::Module
{
public:
    SystemModule();
    ~SystemModule() override = default;
    const char *GetName() const override { return "SystemModule"; }

protected:
    void DoInitialize() override;

    void DoShutdown() override;

private:
    hive::LogManager m_LogManager;
    hive::ConsoleLogger m_Logger;
};

