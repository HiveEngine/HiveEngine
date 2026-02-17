#define _CRT_SECURE_NO_WARNINGS
#include <nectar/watcher/file_watcher.h>
#include <hive/profiling/profiler.h>
#include <filesystem>
#include <chrono>

namespace nectar
{
    PollingFileWatcher::PollingFileWatcher(comb::DefaultAllocator& alloc, uint32_t interval_ms)
        : alloc_{&alloc}
        , interval_ms_{interval_ms}
        , watched_dirs_{alloc}
        , known_files_{alloc, 256}
    {}

    void PollingFileWatcher::Watch(wax::StringView directory)
    {
        wax::String<> dir{*alloc_};
        dir.Append(directory.Data(), directory.Size());
        watched_dirs_.PushBack(static_cast<wax::String<>&&>(dir));
    }

    size_t PollingFileWatcher::WatchedDirCount() const noexcept
    {
        return watched_dirs_.Size();
    }

    static int64_t GetCurrentTimeMs()
    {
        auto now = std::chrono::steady_clock::now();
        return std::chrono::duration_cast<std::chrono::milliseconds>(
            now.time_since_epoch()).count();
    }

    static int64_t GetFileMtime(const std::filesystem::path& p)
    {
        std::error_code ec;
        auto ftime = std::filesystem::last_write_time(p, ec);
        if (ec) return 0;
        auto dur = ftime.time_since_epoch();
        return std::chrono::duration_cast<std::chrono::milliseconds>(dur).count();
    }

    void PollingFileWatcher::Poll(wax::Vector<FileChange>& changes)
    {
        int64_t now = GetCurrentTimeMs();
        if (now - last_poll_time_ < static_cast<int64_t>(interval_ms_))
            return;

        last_poll_time_ = now;
        ScanDirectories(changes);
    }

    void PollingFileWatcher::ForcePoll(wax::Vector<FileChange>& changes)
    {
        last_poll_time_ = GetCurrentTimeMs();
        ScanDirectories(changes);
    }

    void PollingFileWatcher::ScanDirectories(wax::Vector<FileChange>& changes)
    {
        HIVE_PROFILE_SCOPE_N("FileWatcher::ScanDirectories");
        // Mark all known files as "not seen" by using a temp set
        // We'll track which files we've seen this scan
        wax::HashMap<wax::String<>, bool> seen{*alloc_, 256};

        for (size_t i = 0; i < watched_dirs_.Size(); ++i)
            ScanDirectory(watched_dirs_[i].View(), changes);

        // For files we've scanned, mark them as seen.
        // But ScanDirectory already handles created/modified.
        // For deletions, check if known files still exist.
        wax::Vector<wax::String<>> to_remove{*alloc_};
        for (auto it = known_files_.begin(); it != known_files_.end(); ++it)
        {
            std::error_code ec;
            if (!std::filesystem::exists(
                    std::filesystem::path{it.Key().CStr()}, ec))
            {
                FileChange change;
                change.path = wax::String<>{*alloc_};
                change.path.Append(it.Key().CStr(), it.Key().Size());
                change.kind = FileChangeKind::Deleted;
                changes.PushBack(static_cast<FileChange&&>(change));

                wax::String<> key{*alloc_};
                key.Append(it.Key().CStr(), it.Key().Size());
                to_remove.PushBack(static_cast<wax::String<>&&>(key));
            }
        }

        for (size_t i = 0; i < to_remove.Size(); ++i)
            known_files_.Remove(to_remove[i]);
    }

    void PollingFileWatcher::ScanDirectory(wax::StringView dir,
                                            wax::Vector<FileChange>& changes)
    {
        std::error_code ec;
        auto it = std::filesystem::recursive_directory_iterator(
            std::filesystem::path{std::string{dir.Data(), dir.Size()}}, ec);
        if (ec) return;

        for (const auto& entry : it)
        {
            if (!entry.is_regular_file()) continue;

            auto path_str = entry.path().string();
            // Normalize to forward slashes
            for (auto& c : path_str)
                if (c == '\\') c = '/';

            int64_t mtime = GetFileMtime(entry.path());

            wax::String<> key{*alloc_};
            key.Append(path_str.c_str(), path_str.size());

            auto* existing = known_files_.Find(key);
            if (!existing)
            {
                // New file
                wax::String<> insert_key{*alloc_};
                insert_key.Append(path_str.c_str(), path_str.size());
                known_files_.Insert(static_cast<wax::String<>&&>(insert_key), mtime);

                FileChange change;
                change.path = wax::String<>{*alloc_};
                change.path.Append(path_str.c_str(), path_str.size());
                change.kind = FileChangeKind::Created;
                changes.PushBack(static_cast<FileChange&&>(change));
            }
            else if (*existing != mtime)
            {
                // Modified
                *existing = mtime;

                FileChange change;
                change.path = wax::String<>{*alloc_};
                change.path.Append(path_str.c_str(), path_str.size());
                change.kind = FileChangeKind::Modified;
                changes.PushBack(static_cast<FileChange&&>(change));
            }
        }
    }
}
