#pragma once

#include <hive/core/assert.h>
#include <hive/utils/functor.h>
#include <hive/utils/singleton.h>

#include <fmt/format.h>

#include <array>
#include <string>
#include <utility>

// Undefine Windows macros that conflict with our enum names
#ifdef ERROR
#undef ERROR
#endif

namespace hive
{
    class LogCategory
    {
    public:
        constexpr explicit LogCategory(const char* name, const LogCategory* parentCategory = nullptr)
            : m_name{name}
            , m_parentCategory{parentCategory}
        {
            if (m_parentCategory)
                m_fullPath = std::string(m_parentCategory->GetFullPath()) + "/" + name;
            else
                m_fullPath = name;
        }

        [[nodiscard]] constexpr const char* GetName() const
        {
            return m_name;
        }
        [[nodiscard]] constexpr const LogCategory* GetParentCategory() const
        {
            return m_parentCategory;
        }
        [[nodiscard]] const std::string& GetFullPath() const
        {
            return m_fullPath;
        }

        LogCategory(const LogCategory& other) = delete;
        LogCategory(LogCategory&& other) = delete;
        LogCategory& operator=(const LogCategory& other) = delete;
        LogCategory& operator=(LogCategory&& other) = delete;

    private:
        const char* m_name;
        std::string m_fullPath;
        const LogCategory* m_parentCategory;
    };

    extern const LogCategory LOG_HIVE_ROOT;

    enum class LogSeverity
    {
        TRACE,
        INFO,
        WARN,
        ERROR
    };

    class LogManager final : public Singleton<LogManager>
    {
    public:
        using LoggerId = unsigned int;
        using LogCallback = Functor<void, const LogCategory&, LogSeverity, const char*>;

        void UnregisterLogger(LoggerId id);

        void Log(const LogCategory& cat, LogSeverity sev, const char* msg);

        template <typename T>
        [[nodiscard]] LoggerId RegisterLogger(T* obj, void (T::*method)(const LogCategory&, LogSeverity, const char*))
        {
            Assert(m_count < m_loggers.size(), "Too many loggers registered");
            LoggerId newId = ++m_idCount;
            m_loggers[m_count++] = {newId, LogCallback{obj, method}};
            return newId;
        }

    private:
        std::array<std::pair<LoggerId, LogCallback>, 10> m_loggers;
        unsigned int m_count = 0;
        unsigned int m_idCount = 0;
    };

    class ConsoleLogger
    {
    public:
        explicit ConsoleLogger(LogManager& manager);

        ~ConsoleLogger();

        void Log(const LogCategory& category, LogSeverity severity, const char* message);

    private:
        LogManager& m_manager; // the LogManager must have a longer lifetime than this ConsoleLogger
        LogManager::LoggerId m_loggerId;
    };

    inline void LogGeneral(const LogCategory& cat, LogSeverity sev, const char* msg)
    {
        if (!LogManager::IsInitialized())
            return;
        LogManager::GetInstance().Log(cat, sev, msg);
    }

    // Primary API: Formatted logging (modern {fmt})
    template <typename... Args>
    void LogTrace(const LogCategory& category, fmt::format_string<Args...> format, Args&&... args)
    {
        auto msg = fmt::format(format, std::forward<Args>(args)...);
        LogGeneral(category, LogSeverity::TRACE, msg.c_str());
    }

    template <typename... Args>
    void LogInfo(const LogCategory& category, fmt::format_string<Args...> format, Args&&... args)
    {
        auto msg = fmt::format(format, std::forward<Args>(args)...);
        LogGeneral(category, LogSeverity::INFO, msg.c_str());
    }

    template <typename... Args>
    void LogWarning(const LogCategory& category, fmt::format_string<Args...> format, Args&&... args)
    {
        auto msg = fmt::format(format, std::forward<Args>(args)...);
        LogGeneral(category, LogSeverity::WARN, msg.c_str());
    }

    template <typename... Args>
    void LogError(const LogCategory& category, fmt::format_string<Args...> format, Args&&... args)
    {
        auto msg = fmt::format(format, std::forward<Args>(args)...);
        LogGeneral(category, LogSeverity::ERROR, msg.c_str());
    }

    inline void LogTrace(const LogCategory& category, const char* message)
    {
        LogGeneral(category, LogSeverity::TRACE, message);
    }

    inline void LogInfo(const LogCategory& category, const char* message)
    {
        LogGeneral(category, LogSeverity::INFO, message);
    }

    inline void LogWarning(const LogCategory& category, const char* message)
    {
        LogGeneral(category, LogSeverity::WARN, message);
    }

    inline void LogError(const LogCategory& category, const char* message)
    {
        LogGeneral(category, LogSeverity::ERROR, message);
    }
} // namespace hive
