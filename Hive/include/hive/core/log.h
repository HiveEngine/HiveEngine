#pragma once

#include <hive/utils/functor.h>
#include <hive/utils/singleton.h>
#include <hive/core/assert.h>

#include <array>
#include <string>
#include <utility>
#include <fmt/format.h>

// Undefine Windows macros that conflict with our enum names
#ifdef ERROR
#undef ERROR
#endif

namespace hive
{
    class LogCategory
    {
    public:
        constexpr explicit LogCategory(const char *name, const LogCategory *parentCategory = nullptr) : m_Name(name),
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
        const LogCategory *m_ParentCategory;
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
            Assert(m_Count + 1 < m_Loggers.size(), "Too many loggers registered");
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
        Assert(LogManager::IsInitialized(), "LogManager not initialized");
        LogManager::GetInstance().Log(cat, sev, msg);
    }

    // Primary API: Formatted logging (modern {fmt})
    // Use these by default - type-safe and efficient
    template<typename... Args>
    void LogTrace(const LogCategory &category, fmt::format_string<Args...> format, Args&&... args)
    {
        auto msg = fmt::format(format, std::forward<Args>(args)...);
        LogGeneral(category, LogSeverity::TRACE, msg.c_str());
    }

    template<typename... Args>
    void LogInfo(const LogCategory &category, fmt::format_string<Args...> format, Args&&... args)
    {
        auto msg = fmt::format(format, std::forward<Args>(args)...);
        LogGeneral(category, LogSeverity::INFO, msg.c_str());
    }

    template<typename... Args>
    void LogWarning(const LogCategory &category, fmt::format_string<Args...> format, Args&&... args)
    {
        auto msg = fmt::format(format, std::forward<Args>(args)...);
        LogGeneral(category, LogSeverity::WARN, msg.c_str());
    }

    template<typename... Args>
    void LogError(const LogCategory &category, fmt::format_string<Args...> format, Args&&... args)
    {
        auto msg = fmt::format(format, std::forward<Args>(args)...);
        LogGeneral(category, LogSeverity::ERROR, msg.c_str());
    }

    // Overloads for simple strings (no formatting needed)
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
