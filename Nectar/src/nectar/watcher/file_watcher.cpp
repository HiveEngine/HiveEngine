#include <hive/profiling/profiler.h>

#include <nectar/watcher/file_watcher.h>

#include <chrono>
#include <filesystem>
#include <string>
#include <system_error>

namespace nectar
{
    namespace
    {
        int64_t GetCurrentTimeMs() {
            const auto now = std::chrono::steady_clock::now();
            return std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
        }

        FileSnapshot GetFileSnapshot(const std::filesystem::path& path) {
            std::error_code error;
            const auto fileTime = std::filesystem::last_write_time(path, error);
            if (error)
            {
                return {};
            }

            const int64_t mtime =
                std::chrono::duration_cast<std::chrono::milliseconds>(fileTime.time_since_epoch()).count();
            const auto fileSize = std::filesystem::file_size(path, error);
            if (error)
            {
                return FileSnapshot{mtime, 0};
            }

            return FileSnapshot{mtime, static_cast<int64_t>(fileSize)};
        }
    } // namespace

    PollingFileWatcher::PollingFileWatcher(comb::DefaultAllocator& alloc, uint32_t intervalMs)
        : m_alloc{&alloc}
        , m_intervalMs{intervalMs}
        , m_watchedDirs{alloc}
        , m_knownFiles{alloc, 256} {}

    void PollingFileWatcher::Watch(wax::StringView directory) {
        wax::String dir{*m_alloc};
        dir.Append(directory.Data(), directory.Size());
        m_watchedDirs.PushBack(static_cast<wax::String&&>(dir));
    }

    void PollingFileWatcher::Poll(wax::Vector<FileChange>& changes) {
        const int64_t now = GetCurrentTimeMs();
        if (m_lastPollTime != 0 && now - m_lastPollTime < static_cast<int64_t>(m_intervalMs))
        {
            return;
        }

        m_lastPollTime = now;
        ScanDirectories(changes);
    }

    size_t PollingFileWatcher::WatchedDirCount() const noexcept {
        return m_watchedDirs.Size();
    }

    void PollingFileWatcher::ForcePoll(wax::Vector<FileChange>& changes) {
        m_lastPollTime = GetCurrentTimeMs();
        ScanDirectories(changes);
    }

    void PollingFileWatcher::ScanDirectories(wax::Vector<FileChange>& changes) {
        HIVE_PROFILE_SCOPE_N("FileWatcher::ScanDirectories");

        for (size_t i = 0; i < m_watchedDirs.Size(); ++i)
        {
            ScanDirectory(m_watchedDirs[i].View(), changes);
        }

        wax::Vector<wax::String> toRemove{*m_alloc};
        for (auto it = m_knownFiles.Begin(); it != m_knownFiles.End(); ++it)
        {
            std::error_code error;
            if (!std::filesystem::exists(std::filesystem::path{it.Key().CStr()}, error))
            {
                FileChange change{};
                change.m_path = wax::String{*m_alloc};
                change.m_path.Append(it.Key().CStr(), it.Key().Size());
                change.m_kind = FileChangeKind::DELETED;
                changes.PushBack(static_cast<FileChange&&>(change));

                wax::String key{*m_alloc};
                key.Append(it.Key().CStr(), it.Key().Size());
                toRemove.PushBack(static_cast<wax::String&&>(key));
            }
        }

        for (size_t i = 0; i < toRemove.Size(); ++i)
        {
            m_knownFiles.Remove(toRemove[i]);
        }
    }

    void PollingFileWatcher::ScanDirectory(wax::StringView dir, wax::Vector<FileChange>& changes) {
        std::error_code error;
        const auto it = std::filesystem::recursive_directory_iterator(
            std::filesystem::path{std::string{dir.Data(), dir.Size()}}, error);
        if (error)
        {
            return;
        }

        for (const auto& entry : it)
        {
            if (!entry.is_regular_file())
            {
                continue;
            }

            std::string pathStr = entry.path().string();
            for (char& c : pathStr)
            {
                if (c == '\\')
                {
                    c = '/';
                }
            }

            const FileSnapshot snapshot = GetFileSnapshot(entry.path());

            wax::String key{*m_alloc};
            key.Append(pathStr.c_str(), pathStr.size());

            auto* existing = m_knownFiles.Find(key);
            if (existing == nullptr)
            {
                wax::String insertKey{*m_alloc};
                insertKey.Append(pathStr.c_str(), pathStr.size());
                m_knownFiles.Insert(static_cast<wax::String&&>(insertKey), snapshot);

                FileChange change{};
                change.m_path = wax::String{*m_alloc};
                change.m_path.Append(pathStr.c_str(), pathStr.size());
                change.m_kind = FileChangeKind::CREATED;
                changes.PushBack(static_cast<FileChange&&>(change));
            }
            else if (existing->m_mtime != snapshot.m_mtime || existing->m_size != snapshot.m_size)
            {
                *existing = snapshot;

                FileChange change{};
                change.m_path = wax::String{*m_alloc};
                change.m_path.Append(pathStr.c_str(), pathStr.size());
                change.m_kind = FileChangeKind::MODIFIED;
                changes.PushBack(static_cast<FileChange&&>(change));
            }
        }
    }
} // namespace nectar
