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
        std::string filter_pattern;
        std::string suite_filter;
        std::string exclude_suite_filter;
        std::string exclude_filter_pattern;
        bool verbose = false;
        bool list_only = false;
        bool shuffle = false;
        int repeat_count = 1;
        bool stop_on_failure = false;
        bool fail_on_skip = false;
        CapabilityMask available_capabilities{0};
        bool capabilities_overridden = false;

        TestRunnerConfig() = default;
    };

    class TestRunner
    {
    public:
        explicit TestRunner(const TestRunnerConfig& config = TestRunnerConfig{});

        int Run();
        const std::vector<TestResult>& GetResults() const
        {
            return results_;
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

        TestRunnerConfig config_;
        std::vector<TestResult> results_;
    };

    TestRunnerConfig ParseCommandLine(int argc, char** argv);
} // namespace larvae
