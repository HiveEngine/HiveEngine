#pragma once

#include <hive/hive_config.h>

#include <string>
#include <unordered_set>
#include <vector>

namespace hive
{
    class ModuleContext
    {
    public:
        template <typename T> void AddDependency()
        {
            m_dependencies.emplace_back(T::GetStaticName());
        }

        const std::vector<std::string>& GetDependencies() const
        {
            return m_dependencies;
        }

    private:
        std::vector<std::string> m_dependencies;
    };

    class Module
    {
    public:
        Module() = default;
        virtual ~Module() = default;

        [[nodiscard]] virtual const char* GetName() const = 0;

        HIVE_API void Configure();
        HIVE_API void Initialize();
        HIVE_API void Shutdown();

        [[nodiscard]] HIVE_API bool CanInitialize(const std::unordered_set<std::string>& initModulesNames) const;

        [[nodiscard]] bool IsInitialized() const
        {
            return m_isInitialized;
        }

    protected:
        virtual void DoConfigure([[maybe_unused]] ModuleContext& context)
        {
        }

        virtual void DoInitialize()
        {
        }

        virtual void DoShutdown()
        {
        }

    private:
        ModuleContext m_context;
        bool m_isInitialized{false};
    };

} // namespace hive
