#include <hive/core/log.h>
#include <hive/precomp.h>

#include <array>
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
        static constexpr std::array<const char*, 4> kSeverityLabels = {
            "[TRACE] ",
            "[INFO] ",
            "[WARN] ",
            "[ERROR] ",
        };

        const auto index = static_cast<size_t>(severity);
        if (index >= kSeverityLabels.size())
            return;

        std::cout << kSeverityLabels[index] << category.GetFullPath() << " - " << message << std::endl;
    }
} // namespace hive
