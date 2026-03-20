#pragma once

#include <larvae/capabilities.h>
#include <larvae/test_info.h>
#include <larvae/test_result.h>

#include <string>
#include <vector>

namespace larvae
{
    struct TestRunnerConfig
    {
        std::string m_filterPattern;
        std::string m_suiteFilter;
        std::string m_excludeSuiteFilter;
        std::string m_excludeFilterPattern;
        bool m_verbose = false;
        bool m_listOnly = false;
        bool m_shuffle = false;
        int m_repeatCount = 1;
        bool m_stopOnFailure = false;
        bool m_failOnSkip = false;
        CapabilityMask m_availableCapabilities{0};
        bool m_capabilitiesOverridden = false;

        TestRunnerConfig() = default;
    };

    class TestRunner
    {
    public:
        explicit TestRunner(const TestRunnerConfig& config = TestRunnerConfig{});

        int Run();
        const std::vector<TestResult>& GetResults() const
        {
            return m_results;
        }

        int GetTotalTests() const;
        int GetPassedTests() const;
        int GetFailedTests() const;
        int GetSkippedTests() const;
        double GetTotalTime() const;

    private:
        [[nodiscard]] std::vector<TestInfo> CollectMatchingTests() const;
        [[nodiscard]] CapabilityMask GetMissingCapabilities(const TestInfo& test_info) const;
        [[nodiscard]] TestResult RunTest(const TestInfo& test_info) const;
        [[nodiscard]] TestResult MakeSkippedResult(const TestInfo& test_info, CapabilityMask missing_capabilities) const;
        [[nodiscard]] bool MatchesFilter(const TestInfo& test_info) const;
        void PrintSelection(const std::vector<TestInfo>& tests) const;
        void PrintSummary() const;

        TestRunnerConfig m_config;
        std::vector<TestResult> m_results;
    };

    TestRunnerConfig ParseCommandLine(int argc, char** argv);
} // namespace larvae
