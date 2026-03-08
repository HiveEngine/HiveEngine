#pragma once

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace larvae
{
    struct TestPlaylist
    {
        std::string m_name;
        std::vector<std::string> m_testPatterns;
        bool m_enabled = true;
    };

    struct RunnerGuiConfig
    {
        int m_repeatCount = 1;
        bool m_shuffle = false;
        bool m_stopOnFailure = false;
        bool m_verbose = true;
        bool m_autoScrollLog = true;
        float m_windowWidth = 1280.0f;
        float m_windowHeight = 720.0f;

        std::vector<TestPlaylist> m_playlists;
        std::vector<std::string> m_selectedSuites;

        static std::filesystem::path GetConfigPath()
        {
#if defined(_WIN32)
            char* appdata = nullptr;
            size_t appdataSize = 0;
            if (_dupenv_s(&appdata, &appdataSize, "APPDATA") == 0 && appdata != nullptr)
            {
                auto path = std::filesystem::path{appdata} / "HiveEngine" / "larvae_runner_gui.cfg";
                std::free(appdata);
                std::filesystem::create_directories(path.parent_path());
                return path;
            }
            std::free(appdata);
#else
            if (const char* appdata = std::getenv("APPDATA"))
            {
                auto path = std::filesystem::path{appdata} / "HiveEngine" / "larvae_runner_gui.cfg";
                std::filesystem::create_directories(path.parent_path());
                return path;
            }
#endif
            return "larvae_runner_gui.cfg";
        }

        void Save() const
        {
            std::ofstream file{GetConfigPath()};
            if (!file.is_open())
                return;

            file << "repeat_count=" << m_repeatCount << "\n";
            file << "shuffle=" << (m_shuffle ? 1 : 0) << "\n";
            file << "stop_on_failure=" << (m_stopOnFailure ? 1 : 0) << "\n";
            file << "verbose=" << (m_verbose ? 1 : 0) << "\n";
            file << "auto_scroll_log=" << (m_autoScrollLog ? 1 : 0) << "\n";
            file << "window_width=" << m_windowWidth << "\n";
            file << "window_height=" << m_windowHeight << "\n";

            file << "selected_suites_count=" << m_selectedSuites.size() << "\n";
            for (const auto& suite : m_selectedSuites)
            {
                file << "selected_suite=" << suite << "\n";
            }

            file << "playlists_count=" << m_playlists.size() << "\n";
            for (const auto& playlist : m_playlists)
            {
                file << "playlist_name=" << playlist.m_name << "\n";
                file << "playlist_enabled=" << (playlist.m_enabled ? 1 : 0) << "\n";
                file << "playlist_patterns_count=" << playlist.m_testPatterns.size() << "\n";
                for (const auto& pattern : playlist.m_testPatterns)
                {
                    file << "playlist_pattern=" << pattern << "\n";
                }
            }
        }

        void Load()
        {
            std::ifstream file{GetConfigPath()};
            if (!file.is_open())
                return;

            std::string line;
            m_playlists.clear();
            m_selectedSuites.clear();

            TestPlaylist currentPlaylist;
            bool readingPlaylist = false;

            while (std::getline(file, line))
            {
                auto eqPos = line.find('=');
                if (eqPos == std::string::npos)
                    continue;

                auto key = line.substr(0, eqPos);
                auto value = line.substr(eqPos + 1);

                if (key == "repeat_count")
                    m_repeatCount = std::stoi(value);
                else if (key == "shuffle")
                    m_shuffle = (value == "1");
                else if (key == "stop_on_failure")
                    m_stopOnFailure = (value == "1");
                else if (key == "verbose")
                    m_verbose = (value == "1");
                else if (key == "auto_scroll_log")
                    m_autoScrollLog = (value == "1");
                else if (key == "window_width")
                    m_windowWidth = std::stof(value);
                else if (key == "window_height")
                    m_windowHeight = std::stof(value);
                else if (key == "selected_suites_count")
                    continue;
                else if (key == "selected_suite")
                    m_selectedSuites.push_back(value);
                else if (key == "playlists_count")
                    continue;
                else if (key == "playlist_name")
                {
                    if (readingPlaylist)
                    {
                        m_playlists.push_back(currentPlaylist);
                    }
                    currentPlaylist = TestPlaylist{};
                    currentPlaylist.m_name = value;
                    readingPlaylist = true;
                }
                else if (key == "playlist_enabled")
                    currentPlaylist.m_enabled = (value == "1");
                else if (key == "playlist_patterns_count")
                    continue;
                else if (key == "playlist_pattern")
                    currentPlaylist.m_testPatterns.push_back(value);
            }

            if (readingPlaylist)
            {
                m_playlists.push_back(currentPlaylist);
            }
        }
    };
} // namespace larvae
