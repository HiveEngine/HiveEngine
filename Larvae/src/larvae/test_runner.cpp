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
    namespace
    {
        bool MatchesPattern(std::string_view value, std::string_view pattern)
        {
            if (pattern.empty())
            {
                return true;
            }

            if (pattern.front() == '*' && pattern.back() == '*')
            {
                const auto substr = pattern.substr(1, pattern.length() - 2);
                return value.find(substr) != std::string_view::npos;
            }

            if (pattern.front() == '*')
            {
                const auto suffix = pattern.substr(1);
                return value.ends_with(suffix);
            }

            if (pattern.back() == '*')
            {
                const auto prefix = pattern.substr(0, pattern.length() - 1);
                return value.starts_with(prefix);
            }

            return value == pattern;
        }
    } // namespace

    TestRunner::TestRunner(const TestRunnerConfig& config)
        : m_config{config}
    {
        if (!m_config.m_capabilitiesOverridden)
        {
            m_config.m_availableCapabilities = DetectBuildCapabilities();
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
        return test_info.m_requiredCapabilities & ~m_config.m_availableCapabilities;
    }

    int TestRunner::Run()
    {
        m_results.clear();

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

        if (m_config.m_listOnly)
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
            return m_config.m_failOnSkip && has_missing_capabilities ? 1 : 0;
        }

        if (m_config.m_shuffle)
        {
            std::random_device rd;
            std::mt19937 g{rd()};
            std::ranges::shuffle(filtered_tests, g);
        }

        std::cout << "[==========] Running " << filtered_tests.size() << " test(s)\n";

        std::string current_suite;

        for (int repeat = 0; repeat < m_config.m_repeatCount; ++repeat)
        {
            if (m_config.m_repeatCount > 1)
            {
                std::cout << "\n[----------] Iteration " << (repeat + 1) << " of " << m_config.m_repeatCount << "\n";
            }

            for (const auto& test : filtered_tests)
            {
                if (test.m_suiteName != current_suite)
                {
                    if (!current_suite.empty())
                    {
                        std::cout << "\n";
                    }
                    current_suite = test.m_suiteName;
                    std::cout << "[----------] Running tests from " << current_suite << "\n";
                }

                if (!HasAllCapabilities(m_config.m_availableCapabilities, test.m_requiredCapabilities))
                {
                    const CapabilityMask missing = GetMissingCapabilities(test);
                    TestResult skipped = MakeSkippedResult(test, missing);
                    m_results.push_back(skipped);
                    std::cout << "[ SKIPPED  ] " << test.GetFullName() << " (missing capabilities: "
                              << FormatCapabilities(missing) << ")\n";
                    continue;
                }

                TestResult result = RunTest(test);
                m_results.push_back(result);

                if (m_config.m_stopOnFailure && result.m_status == TestStatus::FAILED)
                {
                    std::cout << "\n[==========] Stopped due to failure\n";
                    PrintSummary();
                    return 1;
                }
            }
        }

        std::cout << "\n";
        PrintSummary();

        return GetFailedTests() > 0 || (m_config.m_failOnSkip && GetSkippedTests() > 0) ? 1 : 0;
    }

    TestResult TestRunner::RunTest(const TestInfo& test_info) const
    {
        TestResult result;
        result.m_suiteName = test_info.m_suiteName;
        result.m_testName = test_info.m_testName;

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

        test_info.m_func();

        // Restore default handler
        SetAssertionFailureHandler(nullptr);
        s_current_error = nullptr;
        s_current_failed = nullptr;

        if (test_failed)
        {
            result.m_status = TestStatus::FAILED;
            result.m_errorMessage = assertion_error;
        }
        else
        {
            result.m_status = TestStatus::PASSED;
        }

        auto end_time = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);
        result.m_durationMs = static_cast<double>(duration.count()) / 1000.0;

        if (result.m_status == TestStatus::PASSED)
        {
            std::cout << "[    OK    ] " << test_info.GetFullName() << " (" << result.m_durationMs << " ms)\n";
        }
        else
        {
            std::cout << "[  FAILED  ] " << test_info.GetFullName() << " (" << result.m_durationMs << " ms)\n";

            if (m_config.m_verbose)
            {
                std::cout << result.m_errorMessage << "\n";
            }
        }

        return result;
    }

    TestResult TestRunner::MakeSkippedResult(const TestInfo& test_info, CapabilityMask missing_capabilities) const
    {
        TestResult result;
        result.m_suiteName = test_info.m_suiteName;
        result.m_testName = test_info.m_testName;
        result.m_status = TestStatus::SKIPPED;
        result.m_errorMessage = "Missing capabilities: " + FormatCapabilities(missing_capabilities);
        result.m_durationMs = 0.0;
        return result;
    }

    bool TestRunner::MatchesFilter(const TestInfo& test_info) const
    {
        if (!m_config.m_suiteFilter.empty())
        {
            if (test_info.m_suiteName != m_config.m_suiteFilter)
            {
                return false;
            }
        }

        if (!m_config.m_excludeSuiteFilter.empty() && test_info.m_suiteName == m_config.m_excludeSuiteFilter)
        {
            return false;
        }

        if (!m_config.m_filterPattern.empty())
        {
            if (!MatchesPattern(test_info.GetFullName(), m_config.m_filterPattern))
            {
                return false;
            }
        }

        if (!m_config.m_excludeFilterPattern.empty())
        {
            if (MatchesPattern(test_info.GetFullName(), m_config.m_excludeFilterPattern))
            {
                return false;
            }
        }

        return true;
    }

    void TestRunner::PrintSelection(const std::vector<TestInfo>& tests) const
    {
        std::cout << "Listing " << tests.size() << " test(s)\n";
        std::cout << "Available capabilities: " << FormatCapabilities(m_config.m_availableCapabilities) << "\n";

        for (const auto& test : tests)
        {
            const CapabilityMask missing = GetMissingCapabilities(test);
            std::cout << (missing == 0 ? "[ RUNNABLE ] " : "[ SKIPPED  ] ") << test.GetFullName()
                      << " (requires: " << FormatCapabilities(test.m_requiredCapabilities) << ")";
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

            for (const auto& result : m_results)
            {
                if (result.m_status == TestStatus::FAILED)
                {
                    std::cout << "  " << result.m_suiteName << "." << result.m_testName << "\n";

                    if (!result.m_errorMessage.empty())
                    {
                        std::istringstream iss{result.m_errorMessage};
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
        return static_cast<int>(m_results.size());
    }

    int TestRunner::GetPassedTests() const
    {
        int count = 0;
        for (const auto& result : m_results)
        {
            if (result.m_status == TestStatus::PASSED)
                ++count;
        }
        return count;
    }

    int TestRunner::GetFailedTests() const
    {
        int count = 0;
        for (const auto& result : m_results)
        {
            if (result.m_status == TestStatus::FAILED)
                ++count;
        }
        return count;
    }

    int TestRunner::GetSkippedTests() const
    {
        int count = 0;
        for (const auto& result : m_results)
        {
            if (result.m_status == TestStatus::SKIPPED)
                ++count;
        }
        return count;
    }

    double TestRunner::GetTotalTime() const
    {
        double total = 0.0;
        for (const auto& result : m_results)
        {
            total += result.m_durationMs;
        }
        return total;
    }

    TestRunnerConfig ParseCommandLine(int argc, char** argv)
    {
        TestRunnerConfig config;
        config.m_availableCapabilities = DetectBuildCapabilities();

        for (int i = 1; i < argc; ++i)
        {
            const auto arg = std::string{argv[i]};

            if (arg == "--verbose" || arg == "-v")
            {
                config.m_verbose = true;
            }
            else if (arg == "--shuffle")
            {
                config.m_shuffle = true;
            }
            else if (arg == "--list")
            {
                config.m_listOnly = true;
            }
            else if (arg == "--stop-on-failure")
            {
                config.m_stopOnFailure = true;
            }
            else if (arg == "--fail-on-skip")
            {
                config.m_failOnSkip = true;
            }
            else if (arg.starts_with("--filter="))
            {
                config.m_filterPattern = arg.substr(9);
            }
            else if (arg.starts_with("--suite="))
            {
                config.m_suiteFilter = arg.substr(8);
            }
            else if (arg.starts_with("--exclude-suite="))
            {
                config.m_excludeSuiteFilter = arg.substr(16);
            }
            else if (arg.starts_with("--repeat="))
            {
                config.m_repeatCount = std::stoi(arg.substr(9));
            }
            else if (arg.starts_with("--exclude-filter="))
            {
                config.m_excludeFilterPattern = arg.substr(17);
            }
            else if (arg.starts_with("--capabilities="))
            {
                bool ok = false;
                const CapabilityMask parsed = ParseCapabilityList(arg.substr(15), &ok);
                if (ok)
                {
                    config.m_availableCapabilities = parsed;
                    config.m_capabilitiesOverridden = true;
                }
            }
        }

        return config;
    }
} // namespace larvae
