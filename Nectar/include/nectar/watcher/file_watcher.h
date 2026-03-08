#pragma once

#include <comb/default_allocator.h>

#include <wax/containers/hash_map.h>
#include <wax/containers/string.h>
#include <wax/containers/string_view.h>
#include <wax/containers/vector.h>

#include <cstdint>

namespace nectar
{
    enum class FileChangeKind : uint8_t
    {
        CREATED,
        MODIFIED,
        DELETED,
    };

    struct FileChange
    {
        wax::String m_path;
        FileChangeKind m_kind;
    };

    /// Abstract file watcher interface.
    class IFileWatcher
    {
    public:
        virtual ~IFileWatcher() = default;
        virtual void Watch(wax::StringView directory) = 0;
        virtual void Poll(wax::Vector<FileChange>& changes) = 0;
    };

    struct FileSnapshot
    {
        int64_t m_mtime{0};
        int64_t m_size{0};
    };

    /// Polling-based file watcher using mtime + size checks.
    class PollingFileWatcher final : public IFileWatcher
    {
    public:
        PollingFileWatcher(comb::DefaultAllocator& alloc, uint32_t intervalMs = 500);

        void Watch(wax::StringView directory) override;
        void Poll(wax::Vector<FileChange>& changes) override;

        [[nodiscard]] size_t WatchedDirCount() const noexcept;

        /// Force a rescan regardless of interval (for testing).
        void ForcePoll(wax::Vector<FileChange>& changes);

    private:
        void ScanDirectories(wax::Vector<FileChange>& changes);
        void ScanDirectory(wax::StringView dir, wax::Vector<FileChange>& changes);

        comb::DefaultAllocator* m_alloc;
        uint32_t m_intervalMs;
        int64_t m_lastPollTime{0};
        wax::Vector<wax::String> m_watchedDirs;
        wax::HashMap<wax::String, FileSnapshot> m_knownFiles;
    };
} // namespace nectar
