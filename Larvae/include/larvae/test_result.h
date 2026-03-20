#pragma once

#include <string>

namespace larvae
{
    enum class TestStatus
    {
        PASSED,
        FAILED,
        SKIPPED
    };

    struct TestResult
    {
        std::string m_suiteName;
        std::string m_testName;
        TestStatus m_status;
        std::string m_errorMessage;
        double m_durationMs;
        size_t m_memoryAllocated;
        size_t m_memoryLeaked;

        TestResult()
            : m_status{TestStatus::PASSED}
            , m_durationMs{0.0}
            , m_memoryAllocated{0}
            , m_memoryLeaked{0}
        {
        }
    };
} // namespace larvae
