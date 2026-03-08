#include <hive/core/log.h>
#include <hive/precomp.h>

#include <iostream>
namespace hive
{
    const LogCategory LOG_HIVE_ROOT{"Hive"};

    void LogManager::UnregisterLogger(LoggerId id)
    {
        const auto isLoggerWithId = [id](const auto& loggerPair) {
            return loggerPair.first == id;
        };

        auto it = std::find_if(m_loggers.begin(), m_loggers.begin() + m_count, isLoggerWithId);

        const bool isLoggerFound = it != m_loggers.end();
        if (isLoggerFound)
        {
            *it = m_loggers[m_count - 1];
            m_count--;
        }
    }

    void LogManager::Log(const LogCategory& cat, LogSeverity sev, const char* msg)
    {
        const auto callLoggerFunc = [&](auto& loggerPair) {
            loggerPair.second(cat, sev, msg);
        };

        std::for_each(m_loggers.begin(), m_loggers.begin() + m_count, callLoggerFunc);
    }

    ConsoleLogger::ConsoleLogger(LogManager& manager)
        : m_manager{manager}
        , m_loggerId{m_manager.RegisterLogger(this, &ConsoleLogger::Log)}
    {
    }

    ConsoleLogger::~ConsoleLogger()
    {
        m_manager.UnregisterLogger(m_loggerId);
    }

    void ConsoleLogger::Log(const LogCategory& category, LogSeverity severity, const char* message)
    {
        // TODO use array instead for better performance
        static const std::unordered_map<LogSeverity, const char*> SEVERITY_LABELS = {{LogSeverity::TRACE, "[TRACE] "},
                                                                                     {LogSeverity::INFO, "[INFO] "},
                                                                                     {LogSeverity::WARN, "[WARN] "},
                                                                                     {LogSeverity::ERROR, "[ERROR] "}};

        auto it = SEVERITY_LABELS.find(severity);
        if (it == SEVERITY_LABELS.end())
            return;

        std::cout << it->second;

        std::cout << category.GetFullPath() << " - " << message << std::endl;
    }
} // namespace hive
