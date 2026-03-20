#include <hive/core/log.h>

#include <larvae/larvae.h>

#include <string>

namespace
{

    // Simple test logger that records calls
    struct SpyLogger
    {
        int m_callCount{0};
        std::string m_lastMessage;
        hive::LogSeverity m_lastSeverity{};

        void Log(const hive::LogCategory&, hive::LogSeverity sev, const char* msg)
        {
            ++m_callCount;
            m_lastMessage = msg;
            m_lastSeverity = sev;
        }
    };

    // LogCategory

    auto t_category_name = larvae::RegisterTest("HiveLog", "CategoryName", []() {
        hive::LogCategory cat{"Test"};
        larvae::AssertStringEqual(cat.GetName(), "Test");
        larvae::AssertStringEqual(cat.GetFullPath(), "Test");
    });

    auto t_category_hierarchy = larvae::RegisterTest("HiveLog", "CategoryHierarchy", []() {
        hive::LogCategory root{"Root"};
        hive::LogCategory child{"Child", &root};
        larvae::AssertStringEqual(child.GetFullPath(), "Root/Child");
        larvae::AssertEqual(child.GetParentCategory(), &root);
    });

    // LogManager: register, log, unregister

    auto t_register_and_log = larvae::RegisterTest("HiveLog", "RegisterLoggerAndLog", []() {
        hive::LogManager mgr;
        SpyLogger spy;
        hive::LogCategory cat{"Test"};

        auto id = mgr.RegisterLogger(&spy, &SpyLogger::Log);
        mgr.Log(cat, hive::LogSeverity::INFO, "hello");

        larvae::AssertEqual(spy.m_callCount, 1);
        larvae::AssertStringEqual(spy.m_lastMessage, "hello");
        larvae::AssertTrue(spy.m_lastSeverity == hive::LogSeverity::INFO);

        mgr.UnregisterLogger(id);
    });

    auto t_unregister = larvae::RegisterTest("HiveLog", "UnregisterStopsCallback", []() {
        hive::LogManager mgr;
        SpyLogger spy;
        hive::LogCategory cat{"Test"};

        auto id = mgr.RegisterLogger(&spy, &SpyLogger::Log);
        mgr.UnregisterLogger(id);

        mgr.Log(cat, hive::LogSeverity::WARN, "should not arrive");
        larvae::AssertEqual(spy.m_callCount, 0);
    });

    auto t_multiple_loggers = larvae::RegisterTest("HiveLog", "MultipleLoggers", []() {
        hive::LogManager mgr;
        SpyLogger spyA;
        SpyLogger spyB;
        hive::LogCategory cat{"Test"};

        auto idA = mgr.RegisterLogger(&spyA, &SpyLogger::Log);
        auto idB = mgr.RegisterLogger(&spyB, &SpyLogger::Log);

        mgr.Log(cat, hive::LogSeverity::ERROR, "boom");

        larvae::AssertEqual(spyA.m_callCount, 1);
        larvae::AssertEqual(spyB.m_callCount, 1);
        larvae::AssertStringEqual(spyA.m_lastMessage, "boom");
        larvae::AssertStringEqual(spyB.m_lastMessage, "boom");

        mgr.UnregisterLogger(idA);
        mgr.Log(cat, hive::LogSeverity::TRACE, "second");

        larvae::AssertEqual(spyA.m_callCount, 1); // no more calls
        larvae::AssertEqual(spyB.m_callCount, 2);

        mgr.UnregisterLogger(idB);
    });

    auto t_unregister_invalid = larvae::RegisterTest("HiveLog", "UnregisterInvalidIdIsNoop", []() {
        hive::LogManager mgr;
        // Should not crash
        mgr.UnregisterLogger(9999);
    });

} // namespace
