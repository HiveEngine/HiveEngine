#include <larvae/assertions.h>
#include <larvae/capabilities.h>
#include <larvae/precomp.h>
#include <larvae/test_registry.h>
#include <larvae/test_runner.h>

#include <algorithm>
#include <chrono>
#include <iostream>
#include <random>

namespace larvae
{
    TestRunner::TestRunner(const TestRunnerConfig& config)
        : config_{config}
    {
        if (!config_.capabilities_overridden)
        {
            config_.available_capabilities = DetectBuildCapabilities();
        }
    }

    std::vector<TestInfo> TestRunner::CollectMatchingTests() const
    {
        std::vector<TestInfo> filtered_tests;
        const auto& tests = TestRegistry::GetInstance().GetTests();
        filtered_tests.reserve(tests.size());

        for (const auto& test : tests)
        {
            if (MatchesFilter(test))
            {
                filtered_tests.push_back(test);
            }
        }

        return filtered_tests;
    }

    CapabilityMask TestRunner::GetMissingCapabilities(const TestInfo& test_info) const
    {
        return test_info.required_capabilities & ~config_.available_capabilities;
    }

    int TestRunner::Run()
    {
        results_.clear();

        const auto& tests = TestRegistry::GetInstance().GetTests();
        if (tests.empty())
        {
            std::cout << "No tests registered!\n";
            return 0;
        }

        std::vector<TestInfo> filtered_tests = CollectMatchingTests();
        if (filtered_tests.empty())
        {
            std::cout << "No tests match the filter!\n";
            return 0;
        }

        if (config_.list_only)
        {
            PrintSelection(filtered_tests);

            bool has_missing_capabilities = false;
            for (const auto& test : filtered_tests)
            {
                if (GetMissingCapabilities(test) != 0)
                {
                    has_missing_capabilities = true;
                    break;
                }
            }
            return config_.fail_on_skip && has_missing_capabilities ? 1 : 0;
        }

        if (config_.shuffle)
        {
            std::random_device rd;
            std::mt19937 g{rd()};
            std::ranges::shuffle(filtered_tests, g);
        }

        std::cout << "[==========] Running " << filtered_tests.size() << " test(s)\n";

        std::string current_suite;

        for (int repeat = 0; repeat < config_.repeat_count; ++repeat)
        {
            if (config_.repeat_count > 1)
            {
                std::cout << "\n[----------] Iteration " << (repeat + 1) << " of " << config_.repeat_count << "\n";
            }

            for (const auto& test : filtered_tests)
            {
                if (test.suite_name != current_suite)
                {
                    if (!current_suite.empty())
                    {
                        std::cout << "\n";
                    }
                    current_suite = test.suite_name;
                    std::cout << "[----------] Running tests from " << current_suite << "\n";
                }

                if (!HasAllCapabilities(config_.available_capabilities, test.required_capabilities))
                {
                    const CapabilityMask missing = GetMissingCapabilities(test);
                    TestResult skipped = MakeSkippedResult(test, missing);
                    results_.push_back(skipped);
                    std::cout << "[ SKIPPED  ] " << test.GetFullName() << " (missing capabilities: "
                              << FormatCapabilities(missing) << ")\n";
                    continue;
                }

                TestResult result = RunTest(test);
                results_.push_back(result);

                if (config_.stop_on_failure && result.status == TestStatus::Failed)
                {
                    std::cout << "\n[==========] Stopped due to failure\n";
                    PrintSummary();
                    return 1;
                }
            }
        }

        std::cout << "\n";
        PrintSummary();

        return GetFailedTests() > 0 || (config_.fail_on_skip && GetSkippedTests() > 0) ? 1 : 0;
    }

    TestResult TestRunner::RunTest(const TestInfo& test_info) const
    {
        TestResult result;
        result.suite_name = test_info.suite_name;
        result.test_name = test_info.test_name;

        std::cout << "[   RUN    ] " << test_info.GetFullName() << "\n";

        auto start_time = std::chrono::high_resolution_clock::now();

        // Install custom assertion handler to capture failures
        std::string assertion_error;
        bool test_failed = false;

        // Use a static variable to communicate between handler and test runner
        static thread_local std::string* s_current_error = nullptr;
        static thread_local bool* s_current_failed = nullptr;
        s_current_error = &assertion_error;
        s_current_failed = &test_failed;

        SetAssertionFailureHandler([](const char* message) -> bool {
            if (s_current_error && s_current_failed)
            {
                *s_current_error = message;
                *s_current_failed = true;
            }
            return true; // Return true to prevent abort and continue test execution
        });

        test_info.func();

        // Restore default handler
        SetAssertionFailureHandler(nullptr);
        s_current_error = nullptr;
        s_current_failed = nullptr;

        if (test_failed)
        {
            result.status = TestStatus::Failed;
            result.error_message = assertion_error;
        }
        else
        {
            result.status = TestStatus::Passed;
        }

        auto end_time = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);
        result.duration_ms = static_cast<double>(duration.count()) / 1000.0;

        if (result.status == TestStatus::Passed)
        {
            std::cout << "[    OK    ] " << test_info.GetFullName() << " (" << result.duration_ms << " ms)\n";
        }
        else
        {
            std::cout << "[  FAILED  ] " << test_info.GetFullName() << " (" << result.duration_ms << " ms)\n";

            if (config_.verbose)
            {
                std::cout << result.error_message << "\n";
            }
        }

        return result;
    }

    TestResult TestRunner::MakeSkippedResult(const TestInfo& test_info, CapabilityMask missing_capabilities) const
    {
        TestResult result;
        result.suite_name = test_info.suite_name;
        result.test_name = test_info.test_name;
        result.status = TestStatus::Skipped;
        result.error_message = "Missing capabilities: " + FormatCapabilities(missing_capabilities);
        result.duration_ms = 0.0;
        return result;
    }

    bool TestRunner::MatchesFilter(const TestInfo& test_info) const
    {
        if (!config_.suite_filter.empty())
        {
            if (test_info.suite_name != config_.suite_filter)
            {
                return false;
            }
        }

        if (!config_.filter_pattern.empty())
        {
            const auto full_name = test_info.GetFullName();
            const auto& pattern = config_.filter_pattern;

            // *pattern* = contains
            if (pattern.front() == '*' && pattern.back() == '*')
            {
                const auto substr = pattern.substr(1, pattern.length() - 2);
                return full_name.find(substr) != std::string::npos;
            }
            // *pattern = ends with
            else if (pattern.front() == '*')
            {
                const auto suffix = pattern.substr(1);
                return full_name.ends_with(suffix);
            }
            // pattern* = starts with
            else if (pattern.back() == '*')
            {
                const auto prefix = pattern.substr(0, pattern.length() - 1);
                return full_name.starts_with(prefix);
            }
            else
            {
                return full_name == pattern;
            }
        }

        return true;
    }

    void TestRunner::PrintSelection(const std::vector<TestInfo>& tests) const
    {
        std::cout << "Listing " << tests.size() << " test(s)\n";
        std::cout << "Available capabilities: " << FormatCapabilities(config_.available_capabilities) << "\n";

        for (const auto& test : tests)
        {
            const CapabilityMask missing = GetMissingCapabilities(test);
            std::cout << (missing == 0 ? "[ RUNNABLE ] " : "[ SKIPPED  ] ") << test.GetFullName()
                      << " (requires: " << FormatCapabilities(test.required_capabilities) << ")";
            if (missing != 0)
            {
                std::cout << " (missing: " << FormatCapabilities(missing) << ")";
            }
            std::cout << "\n";
        }
    }

    void TestRunner::PrintSummary() const
    {
        std::cout << "[==========] " << GetTotalTests() << " test(s) ran (" << GetTotalTime() << " ms total)\n";
        std::cout << "[  PASSED  ] " << GetPassedTests() << " test(s)\n";

        if (GetFailedTests() > 0)
        {
            std::cout << "[  FAILED  ] " << GetFailedTests() << " test(s)\n";
            std::cout << "\nFailed tests:\n";

            for (const auto& result : results_)
            {
                if (result.status == TestStatus::Failed)
                {
                    std::cout << "  " << result.suite_name << "." << result.test_name << "\n";

                    if (!result.error_message.empty())
                    {
                        std::istringstream iss{result.error_message};
                        std::string line;
                        while (std::getline(iss, line))
                        {
                            std::cout << "    " << line << "\n";
                        }
                    }
                }
            }
        }

        if (GetSkippedTests() > 0)
        {
            std::cout << "[ SKIPPED  ] " << GetSkippedTests() << " test(s)\n";
        }
    }

    int TestRunner::GetTotalTests() const
    {
        return static_cast<int>(results_.size());
    }

    int TestRunner::GetPassedTests() const
    {
        int count = 0;
        for (const auto& result : results_)
        {
            if (result.status == TestStatus::Passed)
                ++count;
        }
        return count;
    }

    int TestRunner::GetFailedTests() const
    {
        int count = 0;
        for (const auto& result : results_)
        {
            if (result.status == TestStatus::Failed)
                ++count;
        }
        return count;
    }

    int TestRunner::GetSkippedTests() const
    {
        int count = 0;
        for (const auto& result : results_)
        {
            if (result.status == TestStatus::Skipped)
                ++count;
        }
        return count;
    }

    double TestRunner::GetTotalTime() const
    {
        double total = 0.0;
        for (const auto& result : results_)
        {
            total += result.duration_ms;
        }
        return total;
    }

    TestRunnerConfig ParseCommandLine(int argc, char** argv)
    {
        TestRunnerConfig config;
        config.available_capabilities = DetectBuildCapabilities();

        for (int i = 1; i < argc; ++i)
        {
            const auto arg = std::string{argv[i]};

            if (arg == "--verbose" || arg == "-v")
            {
                config.verbose = true;
            }
            else if (arg == "--shuffle")
            {
                config.shuffle = true;
            }
            else if (arg == "--list")
            {
                config.list_only = true;
            }
            else if (arg == "--stop-on-failure")
            {
                config.stop_on_failure = true;
            }
            else if (arg == "--fail-on-skip")
            {
                config.fail_on_skip = true;
            }
            else if (arg.starts_with("--filter="))
            {
                config.filter_pattern = arg.substr(9);
            }
            else if (arg.starts_with("--suite="))
            {
                config.suite_filter = arg.substr(8);
            }
            else if (arg.starts_with("--repeat="))
            {
                config.repeat_count = std::stoi(arg.substr(9));
            }
            else if (arg.starts_with("--capabilities="))
            {
                bool ok = false;
                const CapabilityMask parsed = ParseCapabilityList(arg.substr(15), &ok);
                if (ok)
                {
                    config.available_capabilities = parsed;
                    config.capabilities_overridden = true;
                }
            }
        }

        return config;
    }
} // namespace larvae
