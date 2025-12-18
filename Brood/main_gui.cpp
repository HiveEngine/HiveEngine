#include <larvae/larvae.h>
#include <hive/core/log.h>

#include "larvae_runner_config.h"

#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"

#include <GLFW/glfw3.h>

#include <vector>
#include <string>
#include <set>
#include <map>
#include <thread>
#include <mutex>
#include <atomic>
#include <chrono>
#include <sstream>
#include <algorithm>
#include <random>

namespace
{
    // Application state
    struct TestEntry
    {
        std::string suite_name;
        std::string test_name;
        std::string full_name;
        bool selected = true;
    };

    struct TestResultEntry
    {
        std::string suite_name;
        std::string test_name;
        larvae::TestStatus status;
        std::string error_message;
        double duration_ms;
        int iteration;
    };

    struct TestStatistics
    {
        int total_runs = 0;
        int passed = 0;
        int failed = 0;
        double min_duration_ms = 0.0;
        double max_duration_ms = 0.0;
        double avg_duration_ms = 0.0;
        double total_duration_ms = 0.0;
        std::string last_error;
    };

    struct RunHistoryEntry
    {
        std::string timestamp;
        int total_tests = 0;
        int passed = 0;
        int failed = 0;
        double duration_ms = 0.0;
        int iterations = 0;
    };

    struct RunnerState
    {
        std::vector<TestEntry> all_tests;
        std::map<std::string, bool> suite_selection;
        std::map<std::string, bool> test_selection;
        std::vector<TestResultEntry> results;
        std::vector<std::string> log_lines;

        std::map<std::string, TestStatistics> test_statistics;
        std::vector<RunHistoryEntry> run_history;

        std::atomic<bool> is_running{false};
        std::atomic<bool> should_stop{false};
        std::atomic<int> current_iteration{0};
        std::atomic<int> total_iterations{1};
        std::atomic<int> tests_completed{0};
        std::atomic<int> tests_total{0};
        std::atomic<int> tests_passed{0};
        std::atomic<int> tests_failed{0};

        std::mutex results_mutex;
        std::mutex log_mutex;

        std::thread runner_thread;

        larvae::RunnerGuiConfig config;

        char test_filter[256] = "";
        bool show_individual_tests = false;
        std::chrono::steady_clock::time_point run_start_time;
    };

    RunnerState g_state;

    void AddLogLine(const std::string& line)
    {
        std::lock_guard<std::mutex> lock{g_state.log_mutex};
        g_state.log_lines.push_back(line);
    }

    void ClearLog()
    {
        std::lock_guard<std::mutex> lock{g_state.log_mutex};
        g_state.log_lines.clear();
    }

    void DiscoverTests()
    {
        g_state.all_tests.clear();
        g_state.suite_selection.clear();
        g_state.test_selection.clear();

        const auto& tests = larvae::TestRegistry::GetInstance().GetTests();

        for (const auto& test : tests)
        {
            TestEntry entry;
            entry.suite_name = test.suite_name;
            entry.test_name = test.test_name;
            entry.full_name = test.GetFullName();

            // Check if this suite should be selected based on saved config
            bool suite_enabled = g_state.config.selected_suites.empty();
            if (!suite_enabled)
            {
                for (const auto& s : g_state.config.selected_suites)
                {
                    if (s == test.suite_name)
                    {
                        suite_enabled = true;
                        break;
                    }
                }
            }
            entry.selected = suite_enabled;
            g_state.all_tests.push_back(entry);
            g_state.test_selection[entry.full_name] = suite_enabled;

            if (g_state.suite_selection.find(test.suite_name) == g_state.suite_selection.end())
            {
                g_state.suite_selection[test.suite_name] = suite_enabled;
            }

            // Initialize statistics if not present
            if (g_state.test_statistics.find(entry.full_name) == g_state.test_statistics.end())
            {
                g_state.test_statistics[entry.full_name] = TestStatistics{};
            }
        }
    }

    void UpdateTestStatistics(const TestResultEntry& result)
    {
        std::string full_name = result.suite_name + "." + result.test_name;
        auto& stats = g_state.test_statistics[full_name];

        stats.total_runs++;
        stats.total_duration_ms += result.duration_ms;

        if (result.status == larvae::TestStatus::Passed)
        {
            stats.passed++;
        }
        else if (result.status == larvae::TestStatus::Failed)
        {
            stats.failed++;
            stats.last_error = result.error_message;
        }

        if (stats.total_runs == 1)
        {
            stats.min_duration_ms = result.duration_ms;
            stats.max_duration_ms = result.duration_ms;
        }
        else
        {
            stats.min_duration_ms = std::min(stats.min_duration_ms, result.duration_ms);
            stats.max_duration_ms = std::max(stats.max_duration_ms, result.duration_ms);
        }
        stats.avg_duration_ms = stats.total_duration_ms / stats.total_runs;
    }

    void AddRunToHistory()
    {
        RunHistoryEntry entry;

        // Get current timestamp
        auto now = std::chrono::system_clock::now();
        auto time = std::chrono::system_clock::to_time_t(now);
        char buffer[64];
        std::strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", std::localtime(&time));
        entry.timestamp = buffer;

        entry.total_tests = g_state.tests_completed.load();
        entry.passed = g_state.tests_passed.load();
        entry.failed = g_state.tests_failed.load();
        entry.iterations = g_state.config.repeat_count;

        auto end_time = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
            end_time - g_state.run_start_time);
        entry.duration_ms = static_cast<double>(duration.count());

        g_state.run_history.push_back(entry);

        // Keep only last 50 entries
        if (g_state.run_history.size() > 50)
        {
            g_state.run_history.erase(g_state.run_history.begin());
        }
    }

    std::string GetCurrentTimestamp()
    {
        auto now = std::chrono::system_clock::now();
        auto time = std::chrono::system_clock::to_time_t(now);
        char buffer[64];
        std::strftime(buffer, sizeof(buffer), "%H:%M:%S", std::localtime(&time));
        return buffer;
    }

    void RunSelectedTests()
    {
        if (g_state.is_running) return;

        g_state.is_running = true;
        g_state.should_stop = false;
        g_state.run_start_time = std::chrono::steady_clock::now();

        // Collect selected tests
        std::vector<larvae::TestInfo> selected_tests;
        const auto& all_registered = larvae::TestRegistry::GetInstance().GetTests();

        for (const auto& test_info : all_registered)
        {
            std::string full_name = test_info.GetFullName();
            auto test_it = g_state.test_selection.find(full_name);
            if (test_it != g_state.test_selection.end() && test_it->second)
            {
                selected_tests.push_back(test_info);
            }
        }

        // Reset counters
        {
            std::lock_guard<std::mutex> lock{g_state.results_mutex};
            g_state.results.clear();
        }
        ClearLog();

        g_state.tests_completed = 0;
        g_state.tests_total = static_cast<int>(selected_tests.size() * g_state.config.repeat_count);
        g_state.tests_passed = 0;
        g_state.tests_failed = 0;
        g_state.current_iteration = 0;
        g_state.total_iterations = g_state.config.repeat_count;

        // Start runner thread
        g_state.runner_thread = std::thread([selected_tests]() {
            AddLogLine("[==========] Running " + std::to_string(selected_tests.size()) +
                       " test(s), " + std::to_string(g_state.config.repeat_count) + " iteration(s)");

            std::vector<larvae::TestInfo> tests_to_run = selected_tests;

            for (int iter = 0; iter < g_state.config.repeat_count; ++iter)
            {
                if (g_state.should_stop) break;

                g_state.current_iteration = iter + 1;

                if (g_state.config.repeat_count > 1)
                {
                    AddLogLine("\n[----------] Iteration " + std::to_string(iter + 1) +
                               " of " + std::to_string(g_state.config.repeat_count));
                }

                if (g_state.config.shuffle)
                {
                    std::random_device rd;
                    std::mt19937 g{rd()};
                    std::ranges::shuffle(tests_to_run, g);
                }

                std::string current_suite;

                for (const auto& test : tests_to_run)
                {
                    if (g_state.should_stop) break;

                    if (test.suite_name != current_suite)
                    {
                        current_suite = test.suite_name;
                        AddLogLine("\n[----------] Running tests from " + current_suite);
                    }

                    AddLogLine("[   RUN    ] " + test.GetFullName());

                    auto start = std::chrono::high_resolution_clock::now();

                    // Run the test
                    std::string error_message;
                    bool test_failed = false;

                    static thread_local std::string* s_current_error = nullptr;
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

                    test.func();

                    larvae::SetAssertionFailureHandler(nullptr);
                    s_current_error = nullptr;
                    s_current_failed = nullptr;

                    auto end = std::chrono::high_resolution_clock::now();
                    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
                    double duration_ms = static_cast<double>(duration.count()) / 1000.0;

                    TestResultEntry result;
                    result.suite_name = test.suite_name;
                    result.test_name = test.test_name;
                    result.duration_ms = duration_ms;
                    result.iteration = iter + 1;

                    if (test_failed)
                    {
                        result.status = larvae::TestStatus::Failed;
                        result.error_message = error_message;
                        g_state.tests_failed++;

                        AddLogLine("[  FAILED  ] " + test.GetFullName() +
                                   " (" + std::to_string(duration_ms) + " ms)");

                        if (g_state.config.verbose && !error_message.empty())
                        {
                            AddLogLine("    " + error_message);
                        }

                        if (g_state.config.stop_on_failure)
                        {
                            AddLogLine("\n[==========] Stopped due to failure");
                            g_state.should_stop = true;
                        }
                    }
                    else
                    {
                        result.status = larvae::TestStatus::Passed;
                        g_state.tests_passed++;

                        AddLogLine("[    OK    ] " + test.GetFullName() +
                                   " (" + std::to_string(duration_ms) + " ms)");
                    }

                    {
                        std::lock_guard<std::mutex> lock{g_state.results_mutex};
                        g_state.results.push_back(result);
                        UpdateTestStatistics(result);
                    }

                    g_state.tests_completed++;
                }
            }

            AddLogLine("\n[==========] " + std::to_string(g_state.tests_completed.load()) + " test(s) completed");
            AddLogLine("[  PASSED  ] " + std::to_string(g_state.tests_passed.load()) + " test(s)");
            if (g_state.tests_failed > 0)
            {
                AddLogLine("[  FAILED  ] " + std::to_string(g_state.tests_failed.load()) + " test(s)");
            }

            AddRunToHistory();
            g_state.is_running = false;
        });

        g_state.runner_thread.detach();
    }

    void RunFailedTestsOnly()
    {
        if (g_state.is_running) return;

        // Deselect all, then select only failed
        for (auto& [name, selected] : g_state.test_selection)
        {
            selected = false;
        }

        {
            std::lock_guard<std::mutex> lock{g_state.results_mutex};
            for (const auto& result : g_state.results)
            {
                if (result.status == larvae::TestStatus::Failed)
                {
                    std::string full_name = result.suite_name + "." + result.test_name;
                    g_state.test_selection[full_name] = true;
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
        float available_height = ImGui::GetContentRegionAvail().y;
        ImGui::BeginChild("SuitesPanel", ImVec2(0, available_height * 0.65f), true);

        ImGui::Text("Test Suites");
        ImGui::Separator();

        if (ImGui::Button("Select All", ImVec2(115, 0)))
        {
            for (auto& [suite, selected] : g_state.suite_selection)
            {
                selected = true;
            }
            for (auto& [test, selected] : g_state.test_selection)
            {
                selected = true;
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("Deselect All", ImVec2(115, 0)))
        {
            for (auto& [suite, selected] : g_state.suite_selection)
            {
                selected = false;
            }
            for (auto& [test, selected] : g_state.test_selection)
            {
                selected = false;
            }
        }

        ImGui::Spacing();

        ImGui::SetNextItemWidth(-1);
        ImGui::InputTextWithHint("##TestFilter", "Filter tests...", g_state.test_filter, sizeof(g_state.test_filter));

        ImGui::Spacing();
        ImGui::Checkbox("Show individual tests", &g_state.show_individual_tests);
        ImGui::Spacing();

        for (auto& [suite, suite_selected] : g_state.suite_selection)
        {
            int test_count = 0;
            int selected_count = 0;
            for (const auto& test : g_state.all_tests)
            {
                if (test.suite_name == suite)
                {
                    test_count++;
                    auto it = g_state.test_selection.find(test.full_name);
                    if (it != g_state.test_selection.end() && it->second)
                    {
                        selected_count++;
                    }
                }
            }

            ImGui::PushID(suite.c_str());

            if (g_state.show_individual_tests)
            {
                bool prev_selected = suite_selected;
                ImGui::Checkbox("##suite", &suite_selected);
                ImGui::SameLine();

                if (suite_selected != prev_selected)
                {
                    for (auto& [test_name, test_selected] : g_state.test_selection)
                    {
                        for (const auto& test : g_state.all_tests)
                        {
                            if (test.full_name == test_name && test.suite_name == suite)
                            {
                                test_selected = suite_selected;
                                break;
                            }
                        }
                    }
                }

                std::string suite_label = suite + " (" + std::to_string(selected_count) + "/" + std::to_string(test_count) + ")";
                if (ImGui::TreeNode(suite_label.c_str()))
                {
                    for (const auto& test : g_state.all_tests)
                    {
                        if (test.suite_name != suite) continue;

                        if (strlen(g_state.test_filter) > 0)
                        {
                            if (test.full_name.find(g_state.test_filter) == std::string::npos &&
                                test.test_name.find(g_state.test_filter) == std::string::npos)
                            {
                                continue;
                            }
                        }

                        auto it = g_state.test_selection.find(test.full_name);
                        if (it != g_state.test_selection.end())
                        {
                            bool test_selected = it->second;
                            if (ImGui::Checkbox(test.test_name.c_str(), &test_selected))
                            {
                                it->second = test_selected;
                            }

                            if (ImGui::IsItemHovered())
                            {
                                auto stats_it = g_state.test_statistics.find(test.full_name);
                                if (stats_it != g_state.test_statistics.end() && stats_it->second.total_runs > 0)
                                {
                                    const auto& stats = stats_it->second;
                                    ImGui::BeginTooltip();
                                    ImGui::Text("Runs: %d (Pass: %d, Fail: %d)", stats.total_runs, stats.passed, stats.failed);
                                    ImGui::Text("Duration: %.2f ms (avg), %.2f-%.2f ms (range)",
                                                stats.avg_duration_ms, stats.min_duration_ms, stats.max_duration_ms);
                                    if (!stats.last_error.empty())
                                    {
                                        ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "Last error: %s", stats.last_error.c_str());
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
                std::string label = suite + " (" + std::to_string(test_count) + ")";
                bool prev_selected = suite_selected;
                if (ImGui::Checkbox(label.c_str(), &suite_selected))
                {
                    for (auto& [test_name, test_selected] : g_state.test_selection)
                    {
                        for (const auto& test : g_state.all_tests)
                        {
                            if (test.full_name == test_name && test.suite_name == suite)
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

        static char new_playlist_name[64] = "";

        ImGui::SetNextItemWidth(170);
        ImGui::InputText("##NewPlaylist", new_playlist_name, sizeof(new_playlist_name));
        ImGui::SameLine();
        if (ImGui::Button("+", ImVec2(25, 0)))
        {
            if (strlen(new_playlist_name) > 0)
            {
                larvae::TestPlaylist playlist;
                playlist.name = new_playlist_name;
                playlist.enabled = true;

                for (const auto& [suite, selected] : g_state.suite_selection)
                {
                    if (selected)
                    {
                        playlist.test_patterns.push_back(suite + ".*");
                    }
                }

                g_state.config.playlists.push_back(playlist);
                new_playlist_name[0] = '\0';
            }
        }
        if (ImGui::IsItemHovered())
        {
            ImGui::SetTooltip("Create playlist from current selection");
        }

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        int playlist_to_remove = -1;
        for (size_t i = 0; i < g_state.config.playlists.size(); ++i)
        {
            auto& playlist = g_state.config.playlists[i];

            ImGui::PushID(static_cast<int>(i));

            ImGui::Checkbox("##enabled", &playlist.enabled);
            ImGui::SameLine();

            if (ImGui::TreeNode(playlist.name.c_str()))
            {
                ImGui::Text("Patterns:");
                for (const auto& pattern : playlist.test_patterns)
                {
                    ImGui::BulletText("%s", pattern.c_str());
                }

                if (ImGui::Button("Load"))
                {
                    for (auto& [suite, selected] : g_state.suite_selection)
                    {
                        selected = false;
                    }

                    for (const auto& pattern : playlist.test_patterns)
                    {
                        if (pattern.ends_with(".*"))
                        {
                            std::string suite_name = pattern.substr(0, pattern.length() - 2);
                            auto it = g_state.suite_selection.find(suite_name);
                            if (it != g_state.suite_selection.end())
                            {
                                it->second = true;
                            }
                        }
                    }
                }

                ImGui::SameLine();
                if (ImGui::Button("Delete"))
                {
                    playlist_to_remove = static_cast<int>(i);
                }

                ImGui::TreePop();
            }

            ImGui::PopID();
        }

        if (playlist_to_remove >= 0)
        {
            g_state.config.playlists.erase(g_state.config.playlists.begin() + playlist_to_remove);
        }

        ImGui::EndChild();
    }

    void RenderControlPanel()
    {
        ImGui::BeginChild("ControlPanel", ImVec2(0, 120), true);

        ImGui::Text("Run Configuration");
        ImGui::Separator();

        // Repeat count
        ImGui::SetNextItemWidth(100);
        ImGui::InputInt("Iterations", &g_state.config.repeat_count);
        if (g_state.config.repeat_count < 1) g_state.config.repeat_count = 1;
        if (g_state.config.repeat_count > 1000) g_state.config.repeat_count = 1000;

        ImGui::SameLine(200);
        ImGui::Checkbox("Shuffle", &g_state.config.shuffle);

        ImGui::SameLine(300);
        ImGui::Checkbox("Stop on Failure", &g_state.config.stop_on_failure);

        ImGui::SameLine(450);
        ImGui::Checkbox("Verbose", &g_state.config.verbose);

        ImGui::SameLine(550);
        ImGui::Checkbox("Auto-scroll Log", &g_state.config.auto_scroll_log);

        ImGui::Spacing();

        // Run/Stop buttons
        bool is_running = g_state.is_running;

        if (!is_running)
        {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.6f, 0.2f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.7f, 0.3f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.15f, 0.5f, 0.15f, 1.0f));

            if (ImGui::Button("Run Tests", ImVec2(120, 40)))
            {
                RunSelectedTests();
            }

            ImGui::PopStyleColor(3);

            ImGui::SameLine();

            // Rerun Failed button (only if there are failed tests)
            bool has_failed = false;
            {
                std::lock_guard<std::mutex> lock{g_state.results_mutex};
                for (const auto& result : g_state.results)
                {
                    if (result.status == larvae::TestStatus::Failed)
                    {
                        has_failed = true;
                        break;
                    }
                }
            }

            if (has_failed)
            {
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.7f, 0.5f, 0.2f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.8f, 0.6f, 0.3f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.6f, 0.4f, 0.15f, 1.0f));

                if (ImGui::Button("Rerun Failed", ImVec2(120, 40)))
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

            if (ImGui::Button("Stop", ImVec2(120, 40)))
            {
                g_state.should_stop = true;
            }

            ImGui::PopStyleColor(3);
        }

        ImGui::SameLine();

        // Progress info
        if (is_running)
        {
            ImGui::Text("Iteration %d/%d | Tests: %d/%d | Passed: %d | Failed: %d",
                        g_state.current_iteration.load(),
                        g_state.total_iterations.load(),
                        g_state.tests_completed.load(),
                        g_state.tests_total.load(),
                        g_state.tests_passed.load(),
                        g_state.tests_failed.load());

            // Progress bar
            float progress = g_state.tests_total > 0
                ? static_cast<float>(g_state.tests_completed) / static_cast<float>(g_state.tests_total)
                : 0.0f;

            ImGui::SameLine(500);
            ImGui::SetNextItemWidth(300);
            ImGui::ProgressBar(progress, ImVec2(0, 20));
        }
        else if (!g_state.results.empty())
        {
            ImGui::Text("Completed | Passed: %d | Failed: %d",
                        g_state.tests_passed.load(),
                        g_state.tests_failed.load());
        }

        ImGui::EndChild();
    }

    void RenderResultsPanel()
    {
        ImGui::BeginChild("ResultsPanel", ImVec2(0, 250), true);

        if (ImGui::BeginTabBar("ResultsTabs"))
        {
            if (ImGui::BeginTabItem("Results"))
            {
                if (ImGui::BeginTable("ResultsTable", 5,
                    ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                    ImGuiTableFlags_Resizable | ImGuiTableFlags_ScrollY))
                {
                    ImGui::TableSetupColumn("Iter", ImGuiTableColumnFlags_WidthFixed, 40);
                    ImGui::TableSetupColumn("Status", ImGuiTableColumnFlags_WidthFixed, 60);
                    ImGui::TableSetupColumn("Suite", ImGuiTableColumnFlags_WidthStretch);
                    ImGui::TableSetupColumn("Test", ImGuiTableColumnFlags_WidthStretch);
                    ImGui::TableSetupColumn("Duration", ImGuiTableColumnFlags_WidthFixed, 80);
                    ImGui::TableHeadersRow();

                    std::lock_guard<std::mutex> lock{g_state.results_mutex};

                    for (const auto& result : g_state.results)
                    {
                        ImGui::TableNextRow();

                        ImGui::TableNextColumn();
                        ImGui::Text("%d", result.iteration);

                        ImGui::TableNextColumn();
                        if (result.status == larvae::TestStatus::Passed)
                        {
                            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.4f, 0.9f, 0.4f, 1.0f));
                            ImGui::Text("PASS");
                            ImGui::PopStyleColor();
                        }
                        else if (result.status == larvae::TestStatus::Failed)
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
                        ImGui::Text("%s", result.suite_name.c_str());

                        ImGui::TableNextColumn();
                        ImGui::Text("%s", result.test_name.c_str());

                        ImGui::TableNextColumn();
                        ImGui::Text("%.2f ms", result.duration_ms);
                    }

                    ImGui::EndTable();
                }
                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("Failed Only"))
            {
                if (ImGui::BeginTable("FailedTable", 5,
                    ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                    ImGuiTableFlags_Resizable | ImGuiTableFlags_ScrollY))
                {
                    ImGui::TableSetupColumn("Iter", ImGuiTableColumnFlags_WidthFixed, 40);
                    ImGui::TableSetupColumn("Suite", ImGuiTableColumnFlags_WidthStretch);
                    ImGui::TableSetupColumn("Test", ImGuiTableColumnFlags_WidthStretch);
                    ImGui::TableSetupColumn("Duration", ImGuiTableColumnFlags_WidthFixed, 80);
                    ImGui::TableSetupColumn("Error", ImGuiTableColumnFlags_WidthStretch);
                    ImGui::TableHeadersRow();

                    std::lock_guard<std::mutex> lock{g_state.results_mutex};

                    for (const auto& result : g_state.results)
                    {
                        if (result.status != larvae::TestStatus::Failed) continue;

                        ImGui::TableNextRow();

                        ImGui::TableNextColumn();
                        ImGui::Text("%d", result.iteration);

                        ImGui::TableNextColumn();
                        ImGui::Text("%s", result.suite_name.c_str());

                        ImGui::TableNextColumn();
                        ImGui::Text("%s", result.test_name.c_str());

                        ImGui::TableNextColumn();
                        ImGui::Text("%.2f ms", result.duration_ms);

                        ImGui::TableNextColumn();
                        ImGui::TextWrapped("%s", result.error_message.c_str());
                    }

                    ImGui::EndTable();
                }
                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("Statistics"))
            {
                if (ImGui::BeginTable("StatsTable", 7,
                    ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                    ImGuiTableFlags_Resizable | ImGuiTableFlags_ScrollY |
                    ImGuiTableFlags_Sortable))
                {
                    ImGui::TableSetupColumn("Test", ImGuiTableColumnFlags_WidthStretch);
                    ImGui::TableSetupColumn("Runs", ImGuiTableColumnFlags_WidthFixed, 50);
                    ImGui::TableSetupColumn("Pass", ImGuiTableColumnFlags_WidthFixed, 50);
                    ImGui::TableSetupColumn("Fail", ImGuiTableColumnFlags_WidthFixed, 50);
                    ImGui::TableSetupColumn("Avg (ms)", ImGuiTableColumnFlags_WidthFixed, 70);
                    ImGui::TableSetupColumn("Min (ms)", ImGuiTableColumnFlags_WidthFixed, 70);
                    ImGui::TableSetupColumn("Max (ms)", ImGuiTableColumnFlags_WidthFixed, 70);
                    ImGui::TableHeadersRow();

                    for (const auto& [test_name, stats] : g_state.test_statistics)
                    {
                        if (stats.total_runs == 0) continue;

                        ImGui::TableNextRow();

                        ImGui::TableNextColumn();
                        ImGui::Text("%s", test_name.c_str());

                        ImGui::TableNextColumn();
                        ImGui::Text("%d", stats.total_runs);

                        ImGui::TableNextColumn();
                        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.4f, 0.9f, 0.4f, 1.0f));
                        ImGui::Text("%d", stats.passed);
                        ImGui::PopStyleColor();

                        ImGui::TableNextColumn();
                        if (stats.failed > 0)
                        {
                            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.9f, 0.4f, 0.4f, 1.0f));
                            ImGui::Text("%d", stats.failed);
                            ImGui::PopStyleColor();
                        }
                        else
                        {
                            ImGui::Text("0");
                        }

                        ImGui::TableNextColumn();
                        ImGui::Text("%.2f", stats.avg_duration_ms);

                        ImGui::TableNextColumn();
                        ImGui::Text("%.2f", stats.min_duration_ms);

                        ImGui::TableNextColumn();
                        ImGui::Text("%.2f", stats.max_duration_ms);
                    }

                    ImGui::EndTable();
                }

                ImGui::Spacing();
                if (ImGui::Button("Clear Statistics"))
                {
                    for (auto& [name, stats] : g_state.test_statistics)
                    {
                        stats = TestStatistics{};
                    }
                }

                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("History"))
            {
                if (ImGui::BeginTable("HistoryTable", 6,
                    ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                    ImGuiTableFlags_Resizable | ImGuiTableFlags_ScrollY))
                {
                    ImGui::TableSetupColumn("Timestamp", ImGuiTableColumnFlags_WidthFixed, 140);
                    ImGui::TableSetupColumn("Iterations", ImGuiTableColumnFlags_WidthFixed, 70);
                    ImGui::TableSetupColumn("Total", ImGuiTableColumnFlags_WidthFixed, 60);
                    ImGui::TableSetupColumn("Passed", ImGuiTableColumnFlags_WidthFixed, 60);
                    ImGui::TableSetupColumn("Failed", ImGuiTableColumnFlags_WidthFixed, 60);
                    ImGui::TableSetupColumn("Duration", ImGuiTableColumnFlags_WidthFixed, 100);
                    ImGui::TableHeadersRow();

                    // Show most recent first
                    for (auto it = g_state.run_history.rbegin(); it != g_state.run_history.rend(); ++it)
                    {
                        const auto& entry = *it;
                        ImGui::TableNextRow();

                        ImGui::TableNextColumn();
                        ImGui::Text("%s", entry.timestamp.c_str());

                        ImGui::TableNextColumn();
                        ImGui::Text("%d", entry.iterations);

                        ImGui::TableNextColumn();
                        ImGui::Text("%d", entry.total_tests);

                        ImGui::TableNextColumn();
                        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.4f, 0.9f, 0.4f, 1.0f));
                        ImGui::Text("%d", entry.passed);
                        ImGui::PopStyleColor();

                        ImGui::TableNextColumn();
                        if (entry.failed > 0)
                        {
                            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.9f, 0.4f, 0.4f, 1.0f));
                            ImGui::Text("%d", entry.failed);
                            ImGui::PopStyleColor();
                        }
                        else
                        {
                            ImGui::Text("0");
                        }

                        ImGui::TableNextColumn();
                        ImGui::Text("%.0f ms", entry.duration_ms);
                    }

                    ImGui::EndTable();
                }

                ImGui::Spacing();
                if (ImGui::Button("Clear History"))
                {
                    g_state.run_history.clear();
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
        ImGui::SameLine(ImGui::GetWindowWidth() - 80);
        if (ImGui::Button("Clear"))
        {
            ClearLog();
        }

        ImGui::Separator();

        ImGui::BeginChild("LogContent", ImVec2(0, 0), false, ImGuiWindowFlags_HorizontalScrollbar);

        {
            std::lock_guard<std::mutex> lock{g_state.log_mutex};

            for (const auto& line : g_state.log_lines)
            {
                // Color coding
                if (line.find("[  FAILED  ]") != std::string::npos)
                {
                    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.9f, 0.4f, 0.4f, 1.0f));
                    ImGui::TextUnformatted(line.c_str());
                    ImGui::PopStyleColor();
                }
                else if (line.find("[    OK    ]") != std::string::npos)
                {
                    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.4f, 0.9f, 0.4f, 1.0f));
                    ImGui::TextUnformatted(line.c_str());
                    ImGui::PopStyleColor();
                }
                else if (line.find("[   RUN    ]") != std::string::npos)
                {
                    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.4f, 0.7f, 0.9f, 1.0f));
                    ImGui::TextUnformatted(line.c_str());
                    ImGui::PopStyleColor();
                }
                else if (line.find("[==========]") != std::string::npos ||
                         line.find("[----------]") != std::string::npos)
                {
                    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.7f, 0.7f, 0.7f, 1.0f));
                    ImGui::TextUnformatted(line.c_str());
                    ImGui::PopStyleColor();
                }
                else if (line.find("[  PASSED  ]") != std::string::npos)
                {
                    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.4f, 0.9f, 0.4f, 1.0f));
                    ImGui::TextUnformatted(line.c_str());
                    ImGui::PopStyleColor();
                }
                else
                {
                    ImGui::TextUnformatted(line.c_str());
                }
            }

            if (g_state.config.auto_scroll_log && ImGui::GetScrollY() >= ImGui::GetScrollMaxY())
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

        ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoTitleBar |
                                        ImGuiWindowFlags_NoResize |
                                        ImGuiWindowFlags_NoMove |
                                        ImGuiWindowFlags_NoCollapse |
                                        ImGuiWindowFlags_NoBringToFrontOnFocus |
                                        ImGuiWindowFlags_NoNavFocus |
                                        ImGuiWindowFlags_MenuBar;

        ImGui::Begin("Larvae Test Runner", nullptr, window_flags);

        // Menu bar
        if (ImGui::BeginMenuBar())
        {
            if (ImGui::BeginMenu("File"))
            {
                if (ImGui::MenuItem("Save Config"))
                {
                    // Update selected suites before saving
                    g_state.config.selected_suites.clear();
                    for (const auto& [suite, selected] : g_state.suite_selection)
                    {
                        if (selected)
                        {
                            g_state.config.selected_suites.push_back(suite);
                        }
                    }
                    g_state.config.Save();
                }

                if (ImGui::MenuItem("Reload Config"))
                {
                    g_state.config.Load();
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
                ImGui::MenuItem("Auto-scroll Log", nullptr, &g_state.config.auto_scroll_log);
                ImGui::MenuItem("Verbose Output", nullptr, &g_state.config.verbose);
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
        ImGui::SameLine(ImGui::GetWindowWidth() - 200);
        ImGui::TextDisabled("HiveEngine Testing Framework");

        ImGui::Separator();

        // Main layout
        ImGui::Columns(2, "MainColumns", true);
        ImGui::SetColumnWidth(0, 520);

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
}

int main(int argc, char** argv)
{
    // Initialize logging
    hive::LogManager logManager;
    hive::ConsoleLogger logger{hive::LogManager::GetInstance()};

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
    g_state.config.Load();

    // Create window
    GLFWwindow* window = glfwCreateWindow(
        static_cast<int>(g_state.config.window_width),
        static_cast<int>(g_state.config.window_height),
        "Larvae Test Runner - HiveEngine",
        nullptr,
        nullptr);

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
        int display_w, display_h;
        glfwGetFramebufferSize(window, &display_w, &display_h);
        glViewport(0, 0, display_w, display_h);
        glClearColor(0.1f, 0.1f, 0.12f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(window);
    }

    // Save configuration before exit
    int width, height;
    glfwGetWindowSize(window, &width, &height);
    g_state.config.window_width = static_cast<float>(width);
    g_state.config.window_height = static_cast<float>(height);

    g_state.config.selected_suites.clear();
    for (const auto& [suite, selected] : g_state.suite_selection)
    {
        if (selected)
        {
            g_state.config.selected_suites.push_back(suite);
        }
    }
    g_state.config.Save();

    // Cleanup
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glfwDestroyWindow(window);
    glfwTerminate();

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
