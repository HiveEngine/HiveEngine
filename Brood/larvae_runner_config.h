#pragma once

#include <string>
#include <vector>
#include <fstream>
#include <filesystem>

namespace larvae
{
    struct TestPlaylist
    {
        std::string name;
        std::vector<std::string> test_patterns;
        bool enabled = true;
    };

    struct RunnerGuiConfig
    {
        int repeat_count = 1;
        bool shuffle = false;
        bool stop_on_failure = false;
        bool verbose = true;
        bool auto_scroll_log = true;
        float window_width = 1280.0f;
        float window_height = 720.0f;

        std::vector<TestPlaylist> playlists;
        std::vector<std::string> selected_suites;

        static std::filesystem::path GetConfigPath()
        {
            const char* appdata = std::getenv("APPDATA");
            if (appdata)
            {
                auto path = std::filesystem::path{appdata} / "HiveEngine" / "larvae_runner_gui.cfg";
                std::filesystem::create_directories(path.parent_path());
                return path;
            }
            return "larvae_runner_gui.cfg";
        }

        void Save() const
        {
            std::ofstream file{GetConfigPath()};
            if (!file.is_open()) return;

            file << "repeat_count=" << repeat_count << "\n";
            file << "shuffle=" << (shuffle ? 1 : 0) << "\n";
            file << "stop_on_failure=" << (stop_on_failure ? 1 : 0) << "\n";
            file << "verbose=" << (verbose ? 1 : 0) << "\n";
            file << "auto_scroll_log=" << (auto_scroll_log ? 1 : 0) << "\n";
            file << "window_width=" << window_width << "\n";
            file << "window_height=" << window_height << "\n";

            file << "selected_suites_count=" << selected_suites.size() << "\n";
            for (const auto& suite : selected_suites)
            {
                file << "selected_suite=" << suite << "\n";
            }

            file << "playlists_count=" << playlists.size() << "\n";
            for (const auto& playlist : playlists)
            {
                file << "playlist_name=" << playlist.name << "\n";
                file << "playlist_enabled=" << (playlist.enabled ? 1 : 0) << "\n";
                file << "playlist_patterns_count=" << playlist.test_patterns.size() << "\n";
                for (const auto& pattern : playlist.test_patterns)
                {
                    file << "playlist_pattern=" << pattern << "\n";
                }
            }
        }

        void Load()
        {
            std::ifstream file{GetConfigPath()};
            if (!file.is_open()) return;

            std::string line;
            playlists.clear();
            selected_suites.clear();

            size_t suites_count = 0;
            size_t playlists_count = 0;
            TestPlaylist current_playlist;
            size_t patterns_count = 0;
            bool reading_playlist = false;

            while (std::getline(file, line))
            {
                auto eq_pos = line.find('=');
                if (eq_pos == std::string::npos) continue;

                auto key = line.substr(0, eq_pos);
                auto value = line.substr(eq_pos + 1);

                if (key == "repeat_count") repeat_count = std::stoi(value);
                else if (key == "shuffle") shuffle = (value == "1");
                else if (key == "stop_on_failure") stop_on_failure = (value == "1");
                else if (key == "verbose") verbose = (value == "1");
                else if (key == "auto_scroll_log") auto_scroll_log = (value == "1");
                else if (key == "window_width") window_width = std::stof(value);
                else if (key == "window_height") window_height = std::stof(value);
                else if (key == "selected_suites_count") suites_count = std::stoul(value);
                else if (key == "selected_suite") selected_suites.push_back(value);
                else if (key == "playlists_count") playlists_count = std::stoul(value);
                else if (key == "playlist_name")
                {
                    if (reading_playlist)
                    {
                        playlists.push_back(current_playlist);
                    }
                    current_playlist = TestPlaylist{};
                    current_playlist.name = value;
                    reading_playlist = true;
                }
                else if (key == "playlist_enabled") current_playlist.enabled = (value == "1");
                else if (key == "playlist_patterns_count") patterns_count = std::stoul(value);
                else if (key == "playlist_pattern") current_playlist.test_patterns.push_back(value);
            }

            if (reading_playlist)
            {
                playlists.push_back(current_playlist);
            }
        }
    };
}
