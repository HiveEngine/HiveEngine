#include <hive/precomp.h>
#include <hive/core/log.h>

#include <iostream>
namespace hive
{
    const LogCategory LogHiveRoot { "Hive" };

    void LogManager::UnregisterLogger(LoggerId id)
    {
        const auto isLoggerWithId = [id](const auto &loggerPair)
        {
            return loggerPair.first == id;
        };

        auto it = std::find_if(m_Loggers.begin(), m_Loggers.begin() + m_Count, isLoggerWithId);

        const bool isLoggerFound = it != m_Loggers.end();
        if (isLoggerFound)
        {
            *it = m_Loggers[m_Count - 1];
            m_Count--;
        }
    }

    void LogManager::Log(const LogCategory &cat, LogSeverity sev, const char *msg)
    {
        const auto callLoggerFunc = [&](auto &loggerPair)
        {
            loggerPair.second(cat, sev, msg);
        };

        std::for_each(m_Loggers.begin(), m_Loggers.begin() + m_Count, callLoggerFunc);
    }

    ConsoleLogger::ConsoleLogger(LogManager &manager) : m_Manager(manager),
                                                        m_LoggerId(m_Manager.RegisterLogger(this, &ConsoleLogger::Log))
    {
    }

    ConsoleLogger::~ConsoleLogger()
    {
        m_Manager.UnregisterLogger(m_LoggerId);
    }

    void ConsoleLogger::Log(const LogCategory &category, LogSeverity severity, const char *message)
    {
        //TODO use array instead for better performance
        static const std::unordered_map<LogSeverity, const char*> severityLabels = {
            {LogSeverity::TRACE, "[TRACE] "},
            {LogSeverity::INFO,  "[INFO] "},
            {LogSeverity::WARN,  "[WARN] "},
            {LogSeverity::ERROR, "[ERROR] "}
        };

        // Print severity label
        auto it = severityLabels.find(severity);
        // HIVE_ASSERT(it != severityLabels.end());

        std::cout << it->second;

        // Print categories using STL
        std::cout << category.GetFullPath() << " - " << message << std::endl;
    }
}
