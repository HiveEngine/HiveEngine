#define _CRT_SECURE_NO_WARNINGS
#include <larvae/larvae.h>
#include <nectar/watcher/file_watcher.h>
#include <comb/default_allocator.h>
#include <cstdio>
#include <filesystem>
#include <thread>
#include <chrono>

namespace {

    auto& GetFwAlloc()
    {
        static comb::ModuleAllocator alloc{"TestFW", 4 * 1024 * 1024};
        return alloc.Get();
    }

    // Helper to create a temp directory for file watcher tests
    struct TempDir
    {
        std::filesystem::path path;

        TempDir()
        {
            path = std::filesystem::temp_directory_path() / "hive_fw_test";
            std::filesystem::remove_all(path);
            std::filesystem::create_directories(path);
        }

        ~TempDir()
        {
            std::error_code ec;
            std::filesystem::remove_all(path, ec);
        }

        void WriteFile(const char* name, const char* content)
        {
            auto file_path = path / name;
            FILE* f = std::fopen(file_path.string().c_str(), "wb");
            if (f)
            {
                std::fwrite(content, 1, std::strlen(content), f);
                std::fclose(f);
            }
        }

        void DeleteFile(const char* name)
        {
            auto file_path = path / name;
            std::filesystem::remove(file_path);
        }

        std::string PathStr() const
        {
            auto s = path.string();
            for (auto& c : s) if (c == '\\') c = '/';
            return s;
        }
    };

    // =========================================================================

    auto t1 = larvae::RegisterTest("NectarFileWatcher", "WatchAddsDirectory", []() {
        auto& alloc = GetFwAlloc();
        nectar::PollingFileWatcher watcher{alloc, 0};

        watcher.Watch("some/dir");
        watcher.Watch("other/dir");
        larvae::AssertEqual(watcher.WatchedDirCount(), size_t{2});
    });

    auto t2 = larvae::RegisterTest("NectarFileWatcher", "PollDetectsNewFile", []() {
        auto& alloc = GetFwAlloc();
        TempDir dir;
        auto path_str = dir.PathStr();

        nectar::PollingFileWatcher watcher{alloc, 0};
        watcher.Watch(wax::StringView{path_str.c_str(), path_str.size()});

        // Initial scan — no files yet
        wax::Vector<nectar::FileChange> changes{alloc};
        watcher.ForcePoll(changes);
        larvae::AssertEqual(changes.Size(), size_t{0});

        // Create a file
        dir.WriteFile("new_file.txt", "hello");

        // Rescan
        changes.Clear();
        watcher.ForcePoll(changes);
        larvae::AssertTrue(changes.Size() >= 1);

        bool found = false;
        for (size_t i = 0; i < changes.Size(); ++i)
        {
            if (static_cast<uint8_t>(changes[i].kind) ==
                static_cast<uint8_t>(nectar::FileChangeKind::Created))
            {
                found = true;
                break;
            }
        }
        larvae::AssertTrue(found);
    });

    auto t3 = larvae::RegisterTest("NectarFileWatcher", "PollDetectsModifiedFile", []() {
        auto& alloc = GetFwAlloc();
        TempDir dir;
        auto path_str = dir.PathStr();

        dir.WriteFile("mod_file.txt", "original");

        nectar::PollingFileWatcher watcher{alloc, 0};
        watcher.Watch(wax::StringView{path_str.c_str(), path_str.size()});

        // Initial scan
        wax::Vector<nectar::FileChange> changes{alloc};
        watcher.ForcePoll(changes);

        // Ensure mtime changes (filesystem resolution is at least 1 second on some OSes)
        std::this_thread::sleep_for(std::chrono::milliseconds(1100));
        dir.WriteFile("mod_file.txt", "modified content");

        changes.Clear();
        watcher.ForcePoll(changes);
        larvae::AssertTrue(changes.Size() >= 1);

        bool found_modified = false;
        for (size_t i = 0; i < changes.Size(); ++i)
        {
            if (static_cast<uint8_t>(changes[i].kind) ==
                static_cast<uint8_t>(nectar::FileChangeKind::Modified))
            {
                found_modified = true;
                break;
            }
        }
        larvae::AssertTrue(found_modified);
    });

    auto t4 = larvae::RegisterTest("NectarFileWatcher", "PollDetectsDeletedFile", []() {
        auto& alloc = GetFwAlloc();
        TempDir dir;
        auto path_str = dir.PathStr();

        dir.WriteFile("del_file.txt", "soon gone");

        nectar::PollingFileWatcher watcher{alloc, 0};
        watcher.Watch(wax::StringView{path_str.c_str(), path_str.size()});

        // Initial scan
        wax::Vector<nectar::FileChange> changes{alloc};
        watcher.ForcePoll(changes);
        // Should detect del_file.txt as Created
        larvae::AssertTrue(changes.Size() >= 1);

        // Delete file
        dir.DeleteFile("del_file.txt");

        changes.Clear();
        watcher.ForcePoll(changes);
        larvae::AssertTrue(changes.Size() >= 1);

        bool found_deleted = false;
        for (size_t i = 0; i < changes.Size(); ++i)
        {
            if (static_cast<uint8_t>(changes[i].kind) ==
                static_cast<uint8_t>(nectar::FileChangeKind::Deleted))
            {
                found_deleted = true;
                break;
            }
        }
        larvae::AssertTrue(found_deleted);
    });

    auto t5 = larvae::RegisterTest("NectarFileWatcher", "PollRespectsInterval", []() {
        auto& alloc = GetFwAlloc();
        TempDir dir;
        auto path_str = dir.PathStr();

        dir.WriteFile("interval.txt", "data");

        nectar::PollingFileWatcher watcher{alloc, 60000}; // 60 seconds
        watcher.Watch(wax::StringView{path_str.c_str(), path_str.size()});

        // First Poll: interval hasn't elapsed yet (we just created the watcher)
        // Actually, last_poll_time_ starts at 0, so first Poll should work.
        wax::Vector<nectar::FileChange> changes{alloc};
        watcher.Poll(changes);
        // First poll should succeed because last_poll_time_ = 0
        larvae::AssertTrue(changes.Size() >= 1);

        // Second Poll immediately — should NOT rescan (60s interval)
        dir.WriteFile("interval2.txt", "more");
        changes.Clear();
        watcher.Poll(changes);
        larvae::AssertEqual(changes.Size(), size_t{0});
    });

} // namespace
