#pragma once

#include <hive/utils/functor.h>
#include <hive/utils/singleton.h>

#include <array>
#include <string>
#include <utility>

namespace hive
{
    class LogCategory
    {
    public:
        constexpr explicit LogCategory(const char *name, LogCategory *parentCategory = nullptr) : m_Name(name),
            m_ParentCategory(parentCategory)
        {
            if (m_ParentCategory)
                m_FullPath = std::string(m_ParentCategory->GetFullPath()) + "/" + name;
            else
                m_FullPath = name;
        }

        [[nodiscard]] constexpr const char *GetName() const { return m_Name; }
        [[nodiscard]] constexpr const LogCategory *GetParentCategory() const { return m_ParentCategory; }
        [[nodiscard]] const std::string &GetFullPath() const { return m_FullPath; }

        LogCategory(const LogCategory &other) = delete; //Copy constructor
        LogCategory(LogCategory &&other) = delete; //Move constructor
        LogCategory &operator=(const LogCategory &other) = delete; //Copy assignment
        LogCategory &operator=(LogCategory &&other) = delete; //Move assignment
    private:
        const char *m_Name;
        std::string m_FullPath;
        LogCategory *m_ParentCategory;
    };

    extern const LogCategory LogHiveRoot;

    enum class LogSeverity
    {
        TRACE, INFO, WARN, ERROR
    };

    class LogManager final : public Singleton<LogManager>
    {
    public:
        using LoggerId = unsigned int;
        using LogCallback = Functor<void, const LogCategory &, LogSeverity, const char *>;

        void UnregisterLogger(LoggerId id);

        void Log(const LogCategory &cat, LogSeverity sev, const char *msg);

        template<typename T>
        [[nodiscard]] LoggerId RegisterLogger(T *obj, void (T::*method)(const LogCategory &, LogSeverity, const char *))
        {
            // HIVE_ASSERT(m_Count + 1 < m_Loggers.size())
            m_Loggers[m_Count++] = {m_Count, LogCallback{obj, method}};
            return ++m_IdCount;
        }

    private:
        std::array<std::pair<LoggerId, LogCallback>, 10> m_Loggers;
        unsigned int m_Count = 0;
        unsigned int m_IdCount = 0;
    };

    class ConsoleLogger
    {
    public:
        explicit ConsoleLogger(LogManager &manager);

        ~ConsoleLogger();

        void Log(const LogCategory &category, LogSeverity severity, const char *message);

    private:
        LogManager &m_Manager; //the LogManager must have a longer lifetime than this ConsoleLogger
        LogManager::LoggerId m_LoggerId;
    };

    inline void LogGeneral(const LogCategory &cat, LogSeverity sev, const char *msg)
    {
        // HIVE_ASSERT(LogManager::IsInitialized())
        LogManager::GetInstance().Log(cat, sev, msg);
    }

    inline void LogTrace(const LogCategory &category, const char *message)
    {
        LogGeneral(category, LogSeverity::TRACE, message);
    }

    inline void LogInfo(const LogCategory &category, const char *message)
    {
        LogGeneral(category, LogSeverity::INFO, message);
    }

    inline void LogWarning(const LogCategory &category, const char *message)
    {
        LogGeneral(category, LogSeverity::WARN, message);
    }

    inline void LogError(const LogCategory &category, const char *message)
    {
        LogGeneral(category, LogSeverity::ERROR, message);
    }
}
