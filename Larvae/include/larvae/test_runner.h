#pragma once

#include <larvae/test_info.h>
#include <larvae/test_result.h>
#include <vector>
#include <string>

namespace larvae
{
    struct TestRunnerConfig
    {
        std::string filter_pattern;
        std::string suite_filter;
        bool verbose = false;
        bool shuffle = false;
        int repeat_count = 1;
        bool stop_on_failure = false;

        TestRunnerConfig() = default;
    };

    class TestRunner
    {
    public:
        explicit TestRunner(const TestRunnerConfig& config = TestRunnerConfig{});

        int Run();
        const std::vector<TestResult>& GetResults() const { return results_; }

        int GetTotalTests() const;
        int GetPassedTests() const;
        int GetFailedTests() const;
        int GetSkippedTests() const;
        double GetTotalTime() const;

    private:
        [[nodiscard]] TestResult RunTest(const TestInfo& test_info) const;
        [[nodiscard]] bool MatchesFilter(const TestInfo& test_info) const;
        void PrintSummary() const;

        TestRunnerConfig config_;
        std::vector<TestResult> results_;
    };

    TestRunnerConfig ParseCommandLine(int argc, char** argv);
}
