#pragma once

#include <vector>
#include <string>
#include <unordered_set>

namespace hive
{
    class ModuleContext
    {
    public:
        template<typename T>
        void AddDependency()
        {
            m_Dependencies.emplace_back(T::GetStaticName());
        }

        const std::vector<std::string> &GetDependencies() const { return m_Dependencies; }
    private:
        std::vector<std::string> m_Dependencies;
    };

    class Module
    {
    public:
        Module() = default;
        virtual ~Module() = default;

        [[nodiscard]] virtual const char* GetName() const = 0;

        void Configure();
        void Initialize();

        void Shutdown();

        [[nodiscard]] bool CanInitialize(const std::unordered_set<std::string> &initModulesNames) const;

        [[nodiscard]] bool IsInitialized() const { return m_IsInitialized; }

    protected:
        virtual void DoConfigure([[maybe_unused]] ModuleContext &context)
        {
        }

        virtual void DoInitialize()
        {
        }

        virtual void DoShutdown()
        {
        }

    private:
        ModuleContext m_Context;
        bool m_IsInitialized{false};
    };


}
