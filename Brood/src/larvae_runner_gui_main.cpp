#include <wax/containers/string.h>
#include <wax/containers/vector.h>

#include <larvae/larvae.h>

#include <imgui.h>

#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>

#include <GLFW/glfw3.h>
#include <algorithm>
#include <atomic>
#include <brood/larvae_runner_config.h>
#include <brood/process_runtime.h>
#include <chrono>
#include <ctime>
#include <map>
#include <mutex>
#include <random>
#include <set>
#include <string>
#include <thread>
#include <vector>

namespace
{
    // Layout constants
    constexpr float kSuitesPanelRatio          = 0.65f;
    constexpr float kControlPanelHeight        = 120.0f;
    constexpr float kResultsPanelHeight        = 250.0f;
    constexpr float kLeftColumnWidth           = 520.0f;
    constexpr float kSelectionButtonWidth      = 115.0f;
    constexpr float kPlaylistInputWidth        = 170.0f;
    constexpr float kAddPlaylistButtonWidth    = 25.0f;
    constexpr float kIterationInputWidth       = 100.0f;
    constexpr float kProgressBarWidth          = 300.0f;
    constexpr float kProgressBarHeight         = 20.0f;
    constexpr float kActionButtonWidth         = 120.0f;
    constexpr float kActionButtonHeight        = 40.0f;
    constexpr float kShuffleCheckboxOffset     = 200.0f;
    constexpr float kStopOnFailureOffset       = 300.0f;
    constexpr float kVerboseCheckboxOffset     = 450.0f;
    constexpr float kAutoScrollCheckboxOffset  = 550.0f;
    constexpr float kProgressBarOffset         = 500.0f;
    constexpr float kSubtitleRightMargin       = 200.0f;
    constexpr float kLogClearButtonMargin      = 80.0f;
    constexpr float kIterColumnWidth           = 40.0f;
    constexpr float kStatusColumnWidth         = 60.0f;
    constexpr float kDurationColumnWidth       = 80.0f;
    constexpr float kStatsCountColumnWidth     = 50.0f;
    constexpr float kStatsTimingColumnWidth    = 70.0f;
    constexpr float kTimestampColumnWidth      = 140.0f;
    constexpr float kHistoryCountColumnWidth   = 60.0f;
    constexpr float kHistoryIterColumnWidth    = 70.0f;
    constexpr float kHistoryDurationColumnWidth = 100.0f;
    constexpr int   kMaxRepeatCount            = 1000;
    constexpr size_t kMaxRunHistoryEntries     = 50;

    // Application state
    struct TestEntry
    {
        wax::String m_suiteName;
        wax::String m_testName;
        wax::String m_fullName;
        bool m_selected = true;
    };

    struct TestResultEntry
    {
        wax::String m_suiteName;
        wax::String m_testName;
        larvae::TestStatus m_status;
        wax::String m_errorMessage;
        double m_durationMs;
        int m_iteration;
    };

    struct TestStatistics
    {
        int m_totalRuns = 0;
        int m_passed = 0;
        int m_failed = 0;
        double m_minDurationMs = 0.0;
        double m_maxDurationMs = 0.0;
        double m_avgDurationMs = 0.0;
        double m_totalDurationMs = 0.0;
        wax::String m_lastError;
    };

    struct RunHistoryEntry
    {
        wax::String m_timestamp;
        int m_totalTests = 0;
        int m_passed = 0;
        int m_failed = 0;
        double m_durationMs = 0.0;
        int m_iterations = 0;
    };

    struct RunnerState
    {
        wax::Vector<TestEntry> m_allTests;
        std::map<wax::String, bool> m_suiteSelection;
        std::map<wax::String, bool> m_testSelection;
        wax::Vector<TestResultEntry> m_results;
        wax::Vector<wax::String> m_logLines;

        std::map<wax::String, TestStatistics> m_testStatistics;
        wax::Vector<RunHistoryEntry> m_runHistory;

        std::atomic<bool> m_isRunning{false};
        std::atomic<bool> m_shouldStop{false};
        std::atomic<int> m_currentIteration{0};
        std::atomic<int> m_totalIterations{1};
        std::atomic<int> m_testsCompleted{0};
        std::atomic<int> m_testsTotal{0};
        std::atomic<int> m_testsPassed{0};
        std::atomic<int> m_testsFailed{0};

        std::mutex m_resultsMutex;
        std::mutex m_logMutex;

        std::thread m_runnerThread;

        larvae::RunnerGuiConfig m_config;

        char m_testFilter[256] = "";
        bool m_showIndividualTests = false;
        std::chrono::steady_clock::time_point m_runStartTime;
    };

    RunnerState g_state;

    wax::String IntToStr(int value)
    {
        char buf[32];
        std::snprintf(buf, sizeof(buf), "%d", value);
        return wax::String{buf};
    }

    wax::String DoubleToStr(double value)
    {
        char buf[64];
        std::snprintf(buf, sizeof(buf), "%g", value);
        return wax::String{buf};
    }

    wax::String SizeToStr(size_t value)
    {
        char buf[32];
        std::snprintf(buf, sizeof(buf), "%zu", value);
        return wax::String{buf};
    }

    void AddLogLine(wax::StringView line)
    {
        std::lock_guard<std::mutex> lock{g_state.m_logMutex};
        g_state.m_logLines.PushBack(wax::String{line});
    }

    void ClearLog()
    {
        std::lock_guard<std::mutex> lock{g_state.m_logMutex};
        g_state.m_logLines.Clear();
    }

    void DiscoverTests()
    {
        g_state.m_allTests.Clear();
        g_state.m_suiteSelection.clear();
        g_state.m_testSelection.clear();

        const auto& tests = larvae::TestRegistry::GetInstance().GetTests();

        for (const auto& test : tests)
        {
            TestEntry entry;
            entry.m_suiteName = test.m_suiteName.c_str();
            entry.m_testName = test.m_testName.c_str();
            entry.m_fullName = wax::String{test.GetFullName().c_str()};

            bool suite_enabled = g_state.m_config.m_selectedSuites.IsEmpty();
            if (!suite_enabled)
            {
                for (const auto& s : g_state.m_config.m_selectedSuites)
                {
                    if (s == test.m_suiteName.c_str())
                    {
                        suite_enabled = true;
                        break;
                    }
                }
            }
            entry.m_selected = suite_enabled;
            const wax::String fullName{entry.m_fullName};
            g_state.m_allTests.PushBack(std::move(entry));
            g_state.m_testSelection[fullName] = suite_enabled;

            const wax::String suiteName{test.m_suiteName.c_str()};
            if (g_state.m_suiteSelection.find(suiteName) == g_state.m_suiteSelection.end())
            {
                g_state.m_suiteSelection[suiteName] = suite_enabled;
            }

            if (g_state.m_testStatistics.find(fullName) == g_state.m_testStatistics.end())
            {
                g_state.m_testStatistics[fullName] = TestStatistics{};
            }
        }
    }

    void UpdateTestStatistics(const TestResultEntry& result)
    {
        const wax::String fullName = result.m_suiteName + "." + result.m_testName;
        auto& stats = g_state.m_testStatistics[fullName];

        stats.m_totalRuns++;
        stats.m_totalDurationMs += result.m_durationMs;

        if (result.m_status == larvae::TestStatus::PASSED)
        {
            stats.m_passed++;
        }
        else if (result.m_status == larvae::TestStatus::FAILED)
        {
            stats.m_failed++;
            stats.m_lastError = result.m_errorMessage;
        }

        if (stats.m_totalRuns == 1)
        {
            stats.m_minDurationMs = result.m_durationMs;
            stats.m_maxDurationMs = result.m_durationMs;
        }
        else
        {
            stats.m_minDurationMs = (std::min)(stats.m_minDurationMs, result.m_durationMs);
            stats.m_maxDurationMs = (std::max)(stats.m_maxDurationMs, result.m_durationMs);
        }
        stats.m_avgDurationMs = stats.m_totalDurationMs / stats.m_totalRuns;
    }

    wax::String FormatLocalTime(const char* format)
    {
        const auto now = std::chrono::system_clock::now();
        const auto time = std::chrono::system_clock::to_time_t(now);
        std::tm localTime{};

#if defined(_WIN32)
        localtime_s(&localTime, &time);
#else
        localtime_r(&time, &localTime);
#endif

        char buffer[64];
        std::strftime(buffer, sizeof(buffer), format, &localTime);
        return buffer;
    }

    void AddRunToHistory()
    {
        RunHistoryEntry entry;
        entry.m_timestamp = FormatLocalTime("%Y-%m-%d %H:%M:%S");

        entry.m_totalTests = g_state.m_testsCompleted.load();
        entry.m_passed = g_state.m_testsPassed.load();
        entry.m_failed = g_state.m_testsFailed.load();
        entry.m_iterations = g_state.m_config.m_repeatCount;

        auto endTime = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - g_state.m_runStartTime);
        entry.m_durationMs = static_cast<double>(duration.count());

        g_state.m_runHistory.PushBack(std::move(entry));

        // Keep only last N entries
        if (g_state.m_runHistory.Size() > kMaxRunHistoryEntries)
        {
            // Shift elements left by one, then pop
            for (size_t i = 0; i + 1 < g_state.m_runHistory.Size(); ++i)
            {
                g_state.m_runHistory[i] = std::move(g_state.m_runHistory[i + 1]);
            }
            g_state.m_runHistory.PopBack();
        }
    }

    void RunSelectedTests()
    {
        if (g_state.m_isRunning)
            return;

        g_state.m_isRunning = true;
        g_state.m_shouldStop = false;
        g_state.m_runStartTime = std::chrono::steady_clock::now();

        // Collect selected tests
        std::vector<larvae::TestInfo> selectedTests;
        const auto& allRegistered = larvae::TestRegistry::GetInstance().GetTests();

        for (const auto& test_info : allRegistered)
        {
            const wax::String full_name{test_info.GetFullName().c_str()};
            auto test_it = g_state.m_testSelection.find(full_name);
            if (test_it != g_state.m_testSelection.end() && test_it->second)
            {
                selectedTests.push_back(test_info);
            }
        }

        // Reset counters
        {
            std::lock_guard<std::mutex> lock{g_state.m_resultsMutex};
            g_state.m_results.Clear();
        }
        ClearLog();

        g_state.m_testsCompleted = 0;
        g_state.m_testsTotal = static_cast<int>(selectedTests.size() * g_state.m_config.m_repeatCount);
        g_state.m_testsPassed = 0;
        g_state.m_testsFailed = 0;
        g_state.m_currentIteration = 0;
        g_state.m_totalIterations = g_state.m_config.m_repeatCount;

        // Start runner thread
        g_state.m_runnerThread = std::thread([selectedTests]() {
            AddLogLine(wax::String{"[==========] Running "} + SizeToStr(selectedTests.size()) + " test(s), " +
                       IntToStr(g_state.m_config.m_repeatCount) + " iteration(s)");

            std::vector<larvae::TestInfo> testsToRun = selectedTests;

            for (int iter = 0; iter < g_state.m_config.m_repeatCount; ++iter)
            {
                if (g_state.m_shouldStop)
                    break;

                g_state.m_currentIteration = iter + 1;

                if (g_state.m_config.m_repeatCount > 1)
                {
                    AddLogLine(wax::String{"\n[----------] Iteration "} + IntToStr(iter + 1) + " of " +
                               IntToStr(g_state.m_config.m_repeatCount));
                }

                if (g_state.m_config.m_shuffle)
                {
                    std::random_device rd;
                    std::mt19937 g{rd()};
                    std::ranges::shuffle(testsToRun, g);
                }

                wax::String currentSuite;

                for (const auto& test : testsToRun)
                {
                    if (g_state.m_shouldStop)
                        break;

                    if (test.m_suiteName.c_str() != currentSuite)
                    {
                        currentSuite = test.m_suiteName.c_str();
                        AddLogLine(wax::String{"\n[----------] Running tests from "} + currentSuite);
                    }

                    const wax::String fullName{test.GetFullName().c_str()};
                    AddLogLine(wax::String{"[   RUN    ] "} + fullName);

                    auto start = std::chrono::high_resolution_clock::now();

                    wax::String error_message;
                    bool test_failed = false;

                    static thread_local wax::String* s_current_error = nullptr;
                    static thread_local bool* s_current_failed = nullptr;
                    s_current_error = &error_message;
                    s_current_failed = &test_failed;

                    larvae::SetAssertionFailureHandler([](const char* message) -> bool {
                        if (s_current_error && s_current_failed)
                        {
                            *s_current_error = message;
                            *s_current_failed = true;
                        }
                        return true;
                    });

                    test.m_func();

                    larvae::SetAssertionFailureHandler(nullptr);
                    s_current_error = nullptr;
                    s_current_failed = nullptr;

                    auto end = std::chrono::high_resolution_clock::now();
                    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
                    double duration_ms = static_cast<double>(duration.count()) / 1000.0;

                    TestResultEntry result;
                    result.m_suiteName = test.m_suiteName.c_str();
                    result.m_testName = test.m_testName.c_str();
                    result.m_durationMs = duration_ms;
                    result.m_iteration = iter + 1;

                    if (test_failed)
                    {
                        result.m_status = larvae::TestStatus::FAILED;
                        result.m_errorMessage = error_message;
                        g_state.m_testsFailed++;

                        AddLogLine(wax::String{"[  FAILED  ] "} + fullName + " (" + DoubleToStr(duration_ms) + " ms)");

                        if (g_state.m_config.m_verbose && !error_message.IsEmpty())
                        {
                            AddLogLine(wax::String{"    "} + error_message);
                        }

                        if (g_state.m_config.m_stopOnFailure)
                        {
                            AddLogLine("\n[==========] Stopped due to failure");
                            g_state.m_shouldStop = true;
                        }
                    }
                    else
                    {
                        result.m_status = larvae::TestStatus::PASSED;
                        g_state.m_testsPassed++;

                        AddLogLine(wax::String{"[    OK    ] "} + fullName + " (" + DoubleToStr(duration_ms) + " ms)");
                    }

                    {
                        std::lock_guard<std::mutex> lock{g_state.m_resultsMutex};
                        g_state.m_results.PushBack(result);
                        UpdateTestStatistics(result);
                    }

                    g_state.m_testsCompleted++;
                }
            }

            AddLogLine(wax::String{"\n[==========] "} + IntToStr(g_state.m_testsCompleted.load()) +
                       " test(s) completed");
            AddLogLine(wax::String{"[  PASSED  ] "} + IntToStr(g_state.m_testsPassed.load()) + " test(s)");
            if (g_state.m_testsFailed > 0)
            {
                AddLogLine(wax::String{"[  FAILED  ] "} + IntToStr(g_state.m_testsFailed.load()) + " test(s)");
            }

            AddRunToHistory();
            g_state.m_isRunning = false;
        });

        g_state.m_runnerThread.detach();
    }

    void RunFailedTestsOnly()
    {
        if (g_state.m_isRunning)
            return;

        // Deselect all, then select only failed
        for (auto& [name, selected] : g_state.m_testSelection)
        {
            selected = false;
        }

        {
            std::lock_guard<std::mutex> lock{g_state.m_resultsMutex};
            for (const auto& result : g_state.m_results)
            {
                if (result.m_status == larvae::TestStatus::FAILED)
                {
                    const wax::String full_name = result.m_suiteName + "." + result.m_testName;
                    g_state.m_testSelection[full_name] = true;
                }
            }
        }

        RunSelectedTests();
    }

    void SetupDarkTheme()
    {
        ImGuiStyle& style = ImGui::GetStyle();

        // Window
        style.WindowRounding = 4.0f;
        style.WindowBorderSize = 1.0f;
        style.WindowPadding = ImVec2(10, 10);

        // Frame
        style.FrameRounding = 3.0f;
        style.FrameBorderSize = 0.0f;
        style.FramePadding = ImVec2(4, 3);

        // Items
        style.ItemSpacing = ImVec2(8, 4);
        style.ItemInnerSpacing = ImVec2(4, 4);

        // Scrollbar
        style.ScrollbarSize = 14.0f;
        style.ScrollbarRounding = 3.0f;

        // Tabs
        style.TabRounding = 4.0f;

        // Colors - Modern dark theme
        ImVec4* colors = style.Colors;

        // Background colors
        colors[ImGuiCol_WindowBg] = ImVec4(0.10f, 0.10f, 0.12f, 1.00f);
        colors[ImGuiCol_ChildBg] = ImVec4(0.08f, 0.08f, 0.10f, 1.00f);
        colors[ImGuiCol_PopupBg] = ImVec4(0.12f, 0.12f, 0.14f, 0.95f);

        // Border
        colors[ImGuiCol_Border] = ImVec4(0.25f, 0.25f, 0.28f, 1.00f);
        colors[ImGuiCol_BorderShadow] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);

        // Frame backgrounds
        colors[ImGuiCol_FrameBg] = ImVec4(0.15f, 0.15f, 0.18f, 1.00f);
        colors[ImGuiCol_FrameBgHovered] = ImVec4(0.20f, 0.20f, 0.24f, 1.00f);
        colors[ImGuiCol_FrameBgActive] = ImVec4(0.25f, 0.25f, 0.30f, 1.00f);

        // Title
        colors[ImGuiCol_TitleBg] = ImVec4(0.08f, 0.08f, 0.10f, 1.00f);
        colors[ImGuiCol_TitleBgActive] = ImVec4(0.12f, 0.12f, 0.15f, 1.00f);
        colors[ImGuiCol_TitleBgCollapsed] = ImVec4(0.08f, 0.08f, 0.10f, 0.75f);

        // Menu
        colors[ImGuiCol_MenuBarBg] = ImVec4(0.12f, 0.12f, 0.14f, 1.00f);

        // Scrollbar
        colors[ImGuiCol_ScrollbarBg] = ImVec4(0.08f, 0.08f, 0.10f, 1.00f);
        colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.30f, 0.30f, 0.35f, 1.00f);
        colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.40f, 0.40f, 0.45f, 1.00f);
        colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.50f, 0.50f, 0.55f, 1.00f);

        // Check Mark
        colors[ImGuiCol_CheckMark] = ImVec4(0.40f, 0.80f, 0.40f, 1.00f);

        // Slider
        colors[ImGuiCol_SliderGrab] = ImVec4(0.40f, 0.70f, 0.90f, 1.00f);
        colors[ImGuiCol_SliderGrabActive] = ImVec4(0.50f, 0.80f, 1.00f, 1.00f);

        // Button
        colors[ImGuiCol_Button] = ImVec4(0.25f, 0.50f, 0.75f, 1.00f);
        colors[ImGuiCol_ButtonHovered] = ImVec4(0.30f, 0.60f, 0.85f, 1.00f);
        colors[ImGuiCol_ButtonActive] = ImVec4(0.20f, 0.45f, 0.70f, 1.00f);

        // Header
        colors[ImGuiCol_Header] = ImVec4(0.20f, 0.20f, 0.25f, 1.00f);
        colors[ImGuiCol_HeaderHovered] = ImVec4(0.30f, 0.55f, 0.80f, 0.80f);
        colors[ImGuiCol_HeaderActive] = ImVec4(0.35f, 0.60f, 0.85f, 1.00f);

        // Separator
        colors[ImGuiCol_Separator] = ImVec4(0.25f, 0.25f, 0.28f, 1.00f);
        colors[ImGuiCol_SeparatorHovered] = ImVec4(0.40f, 0.70f, 0.90f, 0.78f);
        colors[ImGuiCol_SeparatorActive] = ImVec4(0.40f, 0.70f, 0.90f, 1.00f);

        // Resize grip
        colors[ImGuiCol_ResizeGrip] = ImVec4(0.30f, 0.30f, 0.35f, 0.50f);
        colors[ImGuiCol_ResizeGripHovered] = ImVec4(0.40f, 0.70f, 0.90f, 0.67f);
        colors[ImGuiCol_ResizeGripActive] = ImVec4(0.40f, 0.70f, 0.90f, 0.95f);

        // Tab
        colors[ImGuiCol_Tab] = ImVec4(0.15f, 0.15f, 0.18f, 1.00f);
        colors[ImGuiCol_TabHovered] = ImVec4(0.30f, 0.55f, 0.80f, 0.80f);
        colors[ImGuiCol_TabActive] = ImVec4(0.25f, 0.50f, 0.75f, 1.00f);
        colors[ImGuiCol_TabUnfocused] = ImVec4(0.12f, 0.12f, 0.15f, 1.00f);
        colors[ImGuiCol_TabUnfocusedActive] = ImVec4(0.20f, 0.40f, 0.60f, 1.00f);

        // Table
        colors[ImGuiCol_TableHeaderBg] = ImVec4(0.15f, 0.15f, 0.18f, 1.00f);
        colors[ImGuiCol_TableBorderStrong] = ImVec4(0.25f, 0.25f, 0.28f, 1.00f);
        colors[ImGuiCol_TableBorderLight] = ImVec4(0.20f, 0.20f, 0.23f, 1.00f);
        colors[ImGuiCol_TableRowBg] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
        colors[ImGuiCol_TableRowBgAlt] = ImVec4(1.00f, 1.00f, 1.00f, 0.03f);

        // Text
        colors[ImGuiCol_Text] = ImVec4(0.92f, 0.92f, 0.94f, 1.00f);
        colors[ImGuiCol_TextDisabled] = ImVec4(0.50f, 0.50f, 0.55f, 1.00f);
        colors[ImGuiCol_TextSelectedBg] = ImVec4(0.30f, 0.55f, 0.80f, 0.35f);

        // Plot
        colors[ImGuiCol_PlotLines] = ImVec4(0.40f, 0.70f, 0.90f, 1.00f);
        colors[ImGuiCol_PlotLinesHovered] = ImVec4(1.00f, 0.43f, 0.35f, 1.00f);
        colors[ImGuiCol_PlotHistogram] = ImVec4(0.40f, 0.70f, 0.90f, 1.00f);
        colors[ImGuiCol_PlotHistogramHovered] = ImVec4(1.00f, 0.60f, 0.00f, 1.00f);

        // Drag/Drop
        colors[ImGuiCol_DragDropTarget] = ImVec4(0.40f, 0.70f, 0.90f, 0.90f);

        // Nav
        colors[ImGuiCol_NavHighlight] = ImVec4(0.40f, 0.70f, 0.90f, 1.00f);
        colors[ImGuiCol_NavWindowingHighlight] = ImVec4(1.00f, 1.00f, 1.00f, 0.70f);
        colors[ImGuiCol_NavWindowingDimBg] = ImVec4(0.80f, 0.80f, 0.80f, 0.20f);

        // Modal
        colors[ImGuiCol_ModalWindowDimBg] = ImVec4(0.00f, 0.00f, 0.00f, 0.60f);
    }

    void RenderSuitesPanel()
    {
        float availableHeight = ImGui::GetContentRegionAvail().y;
        ImGui::BeginChild("SuitesPanel", ImVec2(0, availableHeight * kSuitesPanelRatio), true);

        ImGui::Text("Test Suites");
        ImGui::Separator();

        if (ImGui::Button("Select All", ImVec2(kSelectionButtonWidth, 0)))
        {
            for (auto& [suite, selected] : g_state.m_suiteSelection)
            {
                selected = true;
            }
            for (auto& [test, selected] : g_state.m_testSelection)
            {
                selected = true;
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("Deselect All", ImVec2(kSelectionButtonWidth, 0)))
        {
            for (auto& [suite, selected] : g_state.m_suiteSelection)
            {
                selected = false;
            }
            for (auto& [test, selected] : g_state.m_testSelection)
            {
                selected = false;
            }
        }

        ImGui::Spacing();

        ImGui::SetNextItemWidth(-1);
        ImGui::InputTextWithHint("##TestFilter", "Filter tests...", g_state.m_testFilter, sizeof(g_state.m_testFilter));

        ImGui::Spacing();
        ImGui::Checkbox("Show individual tests", &g_state.m_showIndividualTests);
        ImGui::Spacing();

        for (auto& [suite, suite_selected] : g_state.m_suiteSelection)
        {
            int test_count = 0;
            int selected_count = 0;
            for (const auto& test : g_state.m_allTests)
            {
                if (test.m_suiteName == suite)
                {
                    test_count++;
                    auto it = g_state.m_testSelection.find(test.m_fullName);
                    if (it != g_state.m_testSelection.end() && it->second)
                    {
                        selected_count++;
                    }
                }
            }

            ImGui::PushID(suite.CStr());

            if (g_state.m_showIndividualTests)
            {
                bool prev_selected = suite_selected;
                ImGui::Checkbox("##suite", &suite_selected);
                ImGui::SameLine();

                if (suite_selected != prev_selected)
                {
                    for (auto& [test_name, test_selected] : g_state.m_testSelection)
                    {
                        for (const auto& test : g_state.m_allTests)
                        {
                            if (test.m_fullName == test_name && test.m_suiteName == suite)
                            {
                                test_selected = suite_selected;
                                break;
                            }
                        }
                    }
                }

                const wax::String suite_label =
                    suite + " (" + IntToStr(selected_count) + "/" + IntToStr(test_count) + ")";
                if (ImGui::TreeNode(suite_label.CStr()))
                {
                    for (const auto& test : g_state.m_allTests)
                    {
                        if (test.m_suiteName != suite)
                            continue;

                        if (strlen(g_state.m_testFilter) > 0)
                        {
                            if (test.m_fullName.Find(g_state.m_testFilter) == wax::String::npos &&
                                test.m_testName.Find(g_state.m_testFilter) == wax::String::npos)
                            {
                                continue;
                            }
                        }

                        auto it = g_state.m_testSelection.find(test.m_fullName);
                        if (it != g_state.m_testSelection.end())
                        {
                            bool test_selected = it->second;
                            if (ImGui::Checkbox(test.m_testName.CStr(), &test_selected))
                            {
                                it->second = test_selected;
                            }

                            if (ImGui::IsItemHovered())
                            {
                                auto stats_it = g_state.m_testStatistics.find(test.m_fullName);
                                if (stats_it != g_state.m_testStatistics.end() && stats_it->second.m_totalRuns > 0)
                                {
                                    const auto& stats = stats_it->second;
                                    ImGui::BeginTooltip();
                                    ImGui::Text("Runs: %d (Pass: %d, Fail: %d)", stats.m_totalRuns, stats.m_passed,
                                                stats.m_failed);
                                    ImGui::Text("Duration: %.2f ms (avg), %.2f-%.2f ms (range)", stats.m_avgDurationMs,
                                                stats.m_minDurationMs, stats.m_maxDurationMs);
                                    if (!stats.m_lastError.IsEmpty())
                                    {
                                        ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "Last error: %s",
                                                           stats.m_lastError.CStr());
                                    }
                                    ImGui::EndTooltip();
                                }
                            }
                        }
                    }
                    ImGui::TreePop();
                }
            }
            else
            {
                const wax::String label = suite + " (" + IntToStr(test_count) + ")";
                if (ImGui::Checkbox(label.CStr(), &suite_selected))
                {
                    for (auto& [test_name, test_selected] : g_state.m_testSelection)
                    {
                        for (const auto& test : g_state.m_allTests)
                        {
                            if (test.m_fullName == test_name && test.m_suiteName == suite)
                            {
                                test_selected = suite_selected;
                                break;
                            }
                        }
                    }
                }
            }

            ImGui::PopID();
        }

        ImGui::EndChild();
    }

    void RenderPlaylistsPanel()
    {
        ImGui::BeginChild("PlaylistsPanel", ImVec2(0, 0), true);

        ImGui::Text("Playlists");
        ImGui::Separator();

        static char s_newPlaylistName[64] = "";

        ImGui::SetNextItemWidth(kPlaylistInputWidth);
        ImGui::InputText("##NewPlaylist", s_newPlaylistName, sizeof(s_newPlaylistName));
        ImGui::SameLine();
        if (ImGui::Button("+", ImVec2(kAddPlaylistButtonWidth, 0)))
        {
            if (strlen(s_newPlaylistName) > 0)
            {
                larvae::TestPlaylist playlist;
                playlist.m_name = s_newPlaylistName;
                playlist.m_enabled = true;

                for (const auto& [suite, selected] : g_state.m_suiteSelection)
                {
                    if (selected)
                    {
                        playlist.m_testPatterns.PushBack(suite + ".*");
                    }
                }

                g_state.m_config.m_playlists.PushBack(std::move(playlist));
                s_newPlaylistName[0] = '\0';
            }
        }
        if (ImGui::IsItemHovered())
        {
            ImGui::SetTooltip("Create playlist from current selection");
        }

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        int playlistToRemove = -1;
        for (size_t i = 0; i < g_state.m_config.m_playlists.Size(); ++i)
        {
            auto& playlist = g_state.m_config.m_playlists[i];

            ImGui::PushID(static_cast<int>(i));

            ImGui::Checkbox("##enabled", &playlist.m_enabled);
            ImGui::SameLine();

            if (ImGui::TreeNode(playlist.m_name.CStr()))
            {
                ImGui::Text("Patterns:");
                for (const auto& pattern : playlist.m_testPatterns)
                {
                    ImGui::BulletText("%s", pattern.CStr());
                }

                if (ImGui::Button("Load"))
                {
                    for (auto& [suite, selected] : g_state.m_suiteSelection)
                    {
                        selected = false;
                    }

                    for (const auto& pattern : playlist.m_testPatterns)
                    {
                        if (pattern.EndsWith(".*"))
                        {
                            const wax::String suite_name{pattern.View().Substr(0, pattern.Size() - 2)};
                            auto it = g_state.m_suiteSelection.find(suite_name);
                            if (it != g_state.m_suiteSelection.end())
                            {
                                it->second = true;
                            }
                        }
                    }
                }

                ImGui::SameLine();
                if (ImGui::Button("Delete"))
                {
                    playlistToRemove = static_cast<int>(i);
                }

                ImGui::TreePop();
            }

            ImGui::PopID();
        }

        if (playlistToRemove >= 0)
        {
            const size_t idx = static_cast<size_t>(playlistToRemove);
            for (size_t i = idx; i + 1 < g_state.m_config.m_playlists.Size(); ++i)
            {
                g_state.m_config.m_playlists[i] = std::move(g_state.m_config.m_playlists[i + 1]);
            }
            g_state.m_config.m_playlists.PopBack();
        }

        ImGui::EndChild();
    }

    void RenderControlPanel()
    {
        ImGui::BeginChild("ControlPanel", ImVec2(0, kControlPanelHeight), true);

        ImGui::Text("Run Configuration");
        ImGui::Separator();

        // Repeat count
        ImGui::SetNextItemWidth(kIterationInputWidth);
        ImGui::InputInt("Iterations", &g_state.m_config.m_repeatCount);
        if (g_state.m_config.m_repeatCount < 1)
            g_state.m_config.m_repeatCount = 1;
        if (g_state.m_config.m_repeatCount > kMaxRepeatCount)
            g_state.m_config.m_repeatCount = kMaxRepeatCount;

        ImGui::SameLine(kShuffleCheckboxOffset);
        ImGui::Checkbox("Shuffle", &g_state.m_config.m_shuffle);

        ImGui::SameLine(kStopOnFailureOffset);
        ImGui::Checkbox("Stop on Failure", &g_state.m_config.m_stopOnFailure);

        ImGui::SameLine(kVerboseCheckboxOffset);
        ImGui::Checkbox("Verbose", &g_state.m_config.m_verbose);

        ImGui::SameLine(kAutoScrollCheckboxOffset);
        ImGui::Checkbox("Auto-scroll Log", &g_state.m_config.m_autoScrollLog);

        ImGui::Spacing();

        // Run/Stop buttons
        bool isRunning = g_state.m_isRunning;

        if (!isRunning)
        {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.6f, 0.2f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.7f, 0.3f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.15f, 0.5f, 0.15f, 1.0f));

            if (ImGui::Button("Run Tests", ImVec2(kActionButtonWidth, kActionButtonHeight)))
            {
                RunSelectedTests();
            }

            ImGui::PopStyleColor(3);

            ImGui::SameLine();

            // Rerun Failed button (only if there are failed tests)
            bool hasFailed = false;
            {
                std::lock_guard<std::mutex> lock{g_state.m_resultsMutex};
                for (const auto& result : g_state.m_results)
                {
                    if (result.m_status == larvae::TestStatus::FAILED)
                    {
                        hasFailed = true;
                        break;
                    }
                }
            }

            if (hasFailed)
            {
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.7f, 0.5f, 0.2f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.8f, 0.6f, 0.3f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.6f, 0.4f, 0.15f, 1.0f));

                if (ImGui::Button("Rerun Failed", ImVec2(kActionButtonWidth, kActionButtonHeight)))
                {
                    RunFailedTestsOnly();
                }

                ImGui::PopStyleColor(3);
            }
        }
        else
        {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.7f, 0.2f, 0.2f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.8f, 0.3f, 0.3f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.6f, 0.15f, 0.15f, 1.0f));

            if (ImGui::Button("Stop", ImVec2(kActionButtonWidth, kActionButtonHeight)))
            {
                g_state.m_shouldStop = true;
            }

            ImGui::PopStyleColor(3);
        }

        ImGui::SameLine();

        // Progress info
        if (isRunning)
        {
            ImGui::Text("Iteration %d/%d | Tests: %d/%d | Passed: %d | Failed: %d", g_state.m_currentIteration.load(),
                        g_state.m_totalIterations.load(), g_state.m_testsCompleted.load(), g_state.m_testsTotal.load(),
                        g_state.m_testsPassed.load(), g_state.m_testsFailed.load());

            // Progress bar
            float progress = g_state.m_testsTotal > 0 ? static_cast<float>(g_state.m_testsCompleted) /
                                                            static_cast<float>(g_state.m_testsTotal)
                                                      : 0.0f;

            ImGui::SameLine(kProgressBarOffset);
            ImGui::SetNextItemWidth(kProgressBarWidth);
            ImGui::ProgressBar(progress, ImVec2(0, kProgressBarHeight));
        }
        else if (!g_state.m_results.IsEmpty())
        {
            ImGui::Text("Completed | Passed: %d | Failed: %d", g_state.m_testsPassed.load(),
                        g_state.m_testsFailed.load());
        }

        ImGui::EndChild();
    }

    void RenderResultsPanel()
    {
        ImGui::BeginChild("ResultsPanel", ImVec2(0, kResultsPanelHeight), true);

        if (ImGui::BeginTabBar("ResultsTabs"))
        {
            if (ImGui::BeginTabItem("Results"))
            {
                if (ImGui::BeginTable("ResultsTable", 5,
                                      ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable |
                                          ImGuiTableFlags_ScrollY))
                {
                    ImGui::TableSetupColumn("Iter", ImGuiTableColumnFlags_WidthFixed, kIterColumnWidth);
                    ImGui::TableSetupColumn("Status", ImGuiTableColumnFlags_WidthFixed, kStatusColumnWidth);
                    ImGui::TableSetupColumn("Suite", ImGuiTableColumnFlags_WidthStretch);
                    ImGui::TableSetupColumn("Test", ImGuiTableColumnFlags_WidthStretch);
                    ImGui::TableSetupColumn("Duration", ImGuiTableColumnFlags_WidthFixed, kDurationColumnWidth);
                    ImGui::TableHeadersRow();

                    std::lock_guard<std::mutex> lock{g_state.m_resultsMutex};

                    for (const auto& result : g_state.m_results)
                    {
                        ImGui::TableNextRow();

                        ImGui::TableNextColumn();
                        ImGui::Text("%d", result.m_iteration);

                        ImGui::TableNextColumn();
                        if (result.m_status == larvae::TestStatus::PASSED)
                        {
                            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.4f, 0.9f, 0.4f, 1.0f));
                            ImGui::Text("PASS");
                            ImGui::PopStyleColor();
                        }
                        else if (result.m_status == larvae::TestStatus::FAILED)
                        {
                            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.9f, 0.4f, 0.4f, 1.0f));
                            ImGui::Text("FAIL");
                            ImGui::PopStyleColor();
                        }
                        else
                        {
                            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.9f, 0.9f, 0.4f, 1.0f));
                            ImGui::Text("SKIP");
                            ImGui::PopStyleColor();
                        }

                        ImGui::TableNextColumn();
                        ImGui::Text("%s", result.m_suiteName.CStr());

                        ImGui::TableNextColumn();
                        ImGui::Text("%s", result.m_testName.CStr());

                        ImGui::TableNextColumn();
                        ImGui::Text("%.2f ms", result.m_durationMs);
                    }

                    ImGui::EndTable();
                }
                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("Failed Only"))
            {
                if (ImGui::BeginTable("FailedTable", 5,
                                      ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable |
                                          ImGuiTableFlags_ScrollY))
                {
                    ImGui::TableSetupColumn("Iter", ImGuiTableColumnFlags_WidthFixed, kIterColumnWidth);
                    ImGui::TableSetupColumn("Suite", ImGuiTableColumnFlags_WidthStretch);
                    ImGui::TableSetupColumn("Test", ImGuiTableColumnFlags_WidthStretch);
                    ImGui::TableSetupColumn("Duration", ImGuiTableColumnFlags_WidthFixed, kDurationColumnWidth);
                    ImGui::TableSetupColumn("Error", ImGuiTableColumnFlags_WidthStretch);
                    ImGui::TableHeadersRow();

                    std::lock_guard<std::mutex> lock{g_state.m_resultsMutex};

                    for (const auto& result : g_state.m_results)
                    {
                        if (result.m_status != larvae::TestStatus::FAILED)
                            continue;

                        ImGui::TableNextRow();

                        ImGui::TableNextColumn();
                        ImGui::Text("%d", result.m_iteration);

                        ImGui::TableNextColumn();
                        ImGui::Text("%s", result.m_suiteName.CStr());

                        ImGui::TableNextColumn();
                        ImGui::Text("%s", result.m_testName.CStr());

                        ImGui::TableNextColumn();
                        ImGui::Text("%.2f ms", result.m_durationMs);

                        ImGui::TableNextColumn();
                        ImGui::TextWrapped("%s", result.m_errorMessage.CStr());
                    }

                    ImGui::EndTable();
                }
                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("Statistics"))
            {
                if (ImGui::BeginTable("StatsTable", 7,
                                      ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable |
                                          ImGuiTableFlags_ScrollY | ImGuiTableFlags_Sortable))
                {
                    ImGui::TableSetupColumn("Test", ImGuiTableColumnFlags_WidthStretch);
                    ImGui::TableSetupColumn("Runs", ImGuiTableColumnFlags_WidthFixed, kStatsCountColumnWidth);
                    ImGui::TableSetupColumn("Pass", ImGuiTableColumnFlags_WidthFixed, kStatsCountColumnWidth);
                    ImGui::TableSetupColumn("Fail", ImGuiTableColumnFlags_WidthFixed, kStatsCountColumnWidth);
                    ImGui::TableSetupColumn("Avg (ms)", ImGuiTableColumnFlags_WidthFixed, kStatsTimingColumnWidth);
                    ImGui::TableSetupColumn("Min (ms)", ImGuiTableColumnFlags_WidthFixed, kStatsTimingColumnWidth);
                    ImGui::TableSetupColumn("Max (ms)", ImGuiTableColumnFlags_WidthFixed, kStatsTimingColumnWidth);
                    ImGui::TableHeadersRow();

                    for (const auto& [test_name, stats] : g_state.m_testStatistics)
                    {
                        if (stats.m_totalRuns == 0)
                            continue;

                        ImGui::TableNextRow();

                        ImGui::TableNextColumn();
                        ImGui::Text("%s", test_name.CStr());

                        ImGui::TableNextColumn();
                        ImGui::Text("%d", stats.m_totalRuns);

                        ImGui::TableNextColumn();
                        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.4f, 0.9f, 0.4f, 1.0f));
                        ImGui::Text("%d", stats.m_passed);
                        ImGui::PopStyleColor();

                        ImGui::TableNextColumn();
                        if (stats.m_failed > 0)
                        {
                            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.9f, 0.4f, 0.4f, 1.0f));
                            ImGui::Text("%d", stats.m_failed);
                            ImGui::PopStyleColor();
                        }
                        else
                        {
                            ImGui::Text("0");
                        }

                        ImGui::TableNextColumn();
                        ImGui::Text("%.2f", stats.m_avgDurationMs);

                        ImGui::TableNextColumn();
                        ImGui::Text("%.2f", stats.m_minDurationMs);

                        ImGui::TableNextColumn();
                        ImGui::Text("%.2f", stats.m_maxDurationMs);
                    }

                    ImGui::EndTable();
                }

                ImGui::Spacing();
                if (ImGui::Button("Clear Statistics"))
                {
                    for (auto& [name, stats] : g_state.m_testStatistics)
                    {
                        stats = TestStatistics{};
                    }
                }

                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("History"))
            {
                if (ImGui::BeginTable("HistoryTable", 6,
                                      ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable |
                                          ImGuiTableFlags_ScrollY))
                {
                    ImGui::TableSetupColumn("Timestamp", ImGuiTableColumnFlags_WidthFixed, kTimestampColumnWidth);
                    ImGui::TableSetupColumn("Iterations", ImGuiTableColumnFlags_WidthFixed, kHistoryIterColumnWidth);
                    ImGui::TableSetupColumn("Total", ImGuiTableColumnFlags_WidthFixed, kHistoryCountColumnWidth);
                    ImGui::TableSetupColumn("Passed", ImGuiTableColumnFlags_WidthFixed, kHistoryCountColumnWidth);
                    ImGui::TableSetupColumn("Failed", ImGuiTableColumnFlags_WidthFixed, kHistoryCountColumnWidth);
                    ImGui::TableSetupColumn("Duration", ImGuiTableColumnFlags_WidthFixed, kHistoryDurationColumnWidth);
                    ImGui::TableHeadersRow();

                    // Show most recent first
                    for (size_t ri = g_state.m_runHistory.Size(); ri > 0; --ri)
                    {
                        const auto& entry = g_state.m_runHistory[ri - 1];
                        ImGui::TableNextRow();

                        ImGui::TableNextColumn();
                        ImGui::Text("%s", entry.m_timestamp.CStr());

                        ImGui::TableNextColumn();
                        ImGui::Text("%d", entry.m_iterations);

                        ImGui::TableNextColumn();
                        ImGui::Text("%d", entry.m_totalTests);

                        ImGui::TableNextColumn();
                        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.4f, 0.9f, 0.4f, 1.0f));
                        ImGui::Text("%d", entry.m_passed);
                        ImGui::PopStyleColor();

                        ImGui::TableNextColumn();
                        if (entry.m_failed > 0)
                        {
                            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.9f, 0.4f, 0.4f, 1.0f));
                            ImGui::Text("%d", entry.m_failed);
                            ImGui::PopStyleColor();
                        }
                        else
                        {
                            ImGui::Text("0");
                        }

                        ImGui::TableNextColumn();
                        ImGui::Text("%.0f ms", entry.m_durationMs);
                    }

                    ImGui::EndTable();
                }

                ImGui::Spacing();
                if (ImGui::Button("Clear History"))
                {
                    g_state.m_runHistory.Clear();
                }

                ImGui::EndTabItem();
            }

            ImGui::EndTabBar();
        }

        ImGui::EndChild();
    }

    void RenderLogPanel()
    {
        ImGui::BeginChild("LogPanel", ImVec2(0, 0), true);

        ImGui::Text("Log Output");
        ImGui::SameLine(ImGui::GetWindowWidth() - kLogClearButtonMargin);
        if (ImGui::Button("Clear"))
        {
            ClearLog();
        }

        ImGui::Separator();

        ImGui::BeginChild("LogContent", ImVec2(0, 0), false, ImGuiWindowFlags_HorizontalScrollbar);

        {
            std::lock_guard<std::mutex> lock{g_state.m_logMutex};

            for (const auto& line : g_state.m_logLines)
            {
                // Color coding
                if (line.Find("[  FAILED  ]") != wax::String::npos)
                {
                    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.9f, 0.4f, 0.4f, 1.0f));
                    ImGui::TextUnformatted(line.CStr());
                    ImGui::PopStyleColor();
                }
                else if (line.Find("[    OK    ]") != wax::String::npos)
                {
                    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.4f, 0.9f, 0.4f, 1.0f));
                    ImGui::TextUnformatted(line.CStr());
                    ImGui::PopStyleColor();
                }
                else if (line.Find("[   RUN    ]") != wax::String::npos)
                {
                    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.4f, 0.7f, 0.9f, 1.0f));
                    ImGui::TextUnformatted(line.CStr());
                    ImGui::PopStyleColor();
                }
                else if (line.Find("[==========]") != wax::String::npos ||
                         line.Find("[----------]") != wax::String::npos)
                {
                    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.7f, 0.7f, 0.7f, 1.0f));
                    ImGui::TextUnformatted(line.CStr());
                    ImGui::PopStyleColor();
                }
                else if (line.Find("[  PASSED  ]") != wax::String::npos)
                {
                    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.4f, 0.9f, 0.4f, 1.0f));
                    ImGui::TextUnformatted(line.CStr());
                    ImGui::PopStyleColor();
                }
                else
                {
                    ImGui::TextUnformatted(line.CStr());
                }
            }

            if (g_state.m_config.m_autoScrollLog && ImGui::GetScrollY() >= ImGui::GetScrollMaxY())
            {
                ImGui::SetScrollHereY(1.0f);
            }
        }

        ImGui::EndChild();
        ImGui::EndChild();
    }

    void RenderMainWindow()
    {
        ImGuiViewport* viewport = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(viewport->WorkPos);
        ImGui::SetNextWindowSize(viewport->WorkSize);

        ImGuiWindowFlags windowFlags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                                       ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse |
                                       ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus |
                                       ImGuiWindowFlags_MenuBar;

        ImGui::Begin("Larvae Test Runner", nullptr, windowFlags);

        // Menu bar
        if (ImGui::BeginMenuBar())
        {
            if (ImGui::BeginMenu("File"))
            {
                if (ImGui::MenuItem("Save Config"))
                {
                    // Update selected suites before saving
                    g_state.m_config.m_selectedSuites.Clear();
                    for (const auto& [suite, selected] : g_state.m_suiteSelection)
                    {
                        if (selected)
                        {
                            g_state.m_config.m_selectedSuites.PushBack(suite);
                        }
                    }
                    g_state.m_config.Save();
                }

                if (ImGui::MenuItem("Reload Config"))
                {
                    g_state.m_config.Load();
                    DiscoverTests();
                }

                ImGui::Separator();

                if (ImGui::MenuItem("Exit"))
                {
                    glfwSetWindowShouldClose(glfwGetCurrentContext(), GLFW_TRUE);
                }

                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu("View"))
            {
                ImGui::MenuItem("Auto-scroll Log", nullptr, &g_state.m_config.m_autoScrollLog);
                ImGui::MenuItem("Verbose Output", nullptr, &g_state.m_config.m_verbose);
                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu("Help"))
            {
                if (ImGui::MenuItem("About"))
                {
                    // Could show an about dialog
                }
                ImGui::EndMenu();
            }

            ImGui::EndMenuBar();
        }

        // Title
        ImGui::PushFont(ImGui::GetIO().Fonts->Fonts[0]);
        ImGui::Text("Larvae Test Runner");
        ImGui::PopFont();
        ImGui::SameLine(ImGui::GetWindowWidth() - kSubtitleRightMargin);
        ImGui::TextDisabled("HiveEngine Testing Framework");

        ImGui::Separator();

        // Main layout
        ImGui::Columns(2, "MainColumns", true);
        ImGui::SetColumnWidth(0, kLeftColumnWidth);

        // Left column: Suites and Playlists
        RenderSuitesPanel();
        ImGui::Spacing();
        RenderPlaylistsPanel();

        ImGui::NextColumn();

        // Right column: Controls, Results, Log
        RenderControlPanel();
        ImGui::Spacing();
        RenderResultsPanel();
        ImGui::Spacing();
        RenderLogPanel();

        ImGui::Columns(1);

        ImGui::End();
    }
} // namespace

int main(int argc, char** argv)
{
    brood::ProcessRuntime runtime{};

    // Initialize GLFW
    if (!glfwInit())
    {
        return -1;
    }

    // GL 3.3 + GLSL 330
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif

    // Load configuration
    g_state.m_config.Load();

    // Create window
    GLFWwindow* window = glfwCreateWindow(static_cast<int>(g_state.m_config.m_windowWidth),
                                          static_cast<int>(g_state.m_config.m_windowHeight),
                                          "Larvae Test Runner - HiveEngine", nullptr, nullptr);

    if (!window)
    {
        glfwTerminate();
        return -1;
    }

    glfwMakeContextCurrent(window);
    glfwSwapInterval(1); // VSync

    // Initialize ImGui
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    // Setup Platform/Renderer bindings
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 330");

    // Setup style
    SetupDarkTheme();

    // Discover tests
    DiscoverTests();

    // Main loop
    while (!glfwWindowShouldClose(window))
    {
        glfwPollEvents();

        // Start ImGui frame
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        // Render
        RenderMainWindow();

        // Rendering
        ImGui::Render();
        int displayW, displayH;
        glfwGetFramebufferSize(window, &displayW, &displayH);
        glViewport(0, 0, displayW, displayH);
        glClearColor(0.1f, 0.1f, 0.12f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(window);
    }

    // Save configuration before exit
    int width, height;
    glfwGetWindowSize(window, &width, &height);
    g_state.m_config.m_windowWidth = static_cast<float>(width);
    g_state.m_config.m_windowHeight = static_cast<float>(height);

    g_state.m_config.m_selectedSuites.Clear();
    for (const auto& [suite, selected] : g_state.m_suiteSelection)
    {
        if (selected)
        {
            g_state.m_config.m_selectedSuites.PushBack(suite);
        }
    }
    g_state.m_config.Save();

    // Cleanup
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glfwDestroyWindow(window);
    glfwTerminate();

    runtime.Finalize();
    return 0;
}

#ifdef _WIN32
#include <windows.h>

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
    (void)hInstance;
    (void)hPrevInstance;
    (void)lpCmdLine;
    (void)nCmdShow;
    return main(__argc, __argv);
}
#endif
