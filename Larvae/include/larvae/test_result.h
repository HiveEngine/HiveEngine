#pragma once

#include <string>

namespace larvae
{
    enum class TestStatus
    {
        Passed,
        Failed,
        Skipped
    };

    struct TestResult
    {
        std::string suite_name;
        std::string test_name;
        TestStatus status;
        std::string error_message;
        double duration_ms;
        size_t memory_allocated;
        size_t memory_leaked;

        TestResult()
            : status{TestStatus::Passed}
            , duration_ms{0.0}
            , memory_allocated{0}
            , memory_leaked{0}
        {
        }
    };
}
