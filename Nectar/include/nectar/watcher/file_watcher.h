#pragma once

#include <wax/containers/vector.h>
#include <wax/containers/string.h>
#include <wax/containers/string_view.h>
#include <wax/containers/hash_map.h>
#include <comb/default_allocator.h>
#include <cstdint>

namespace nectar
{
    enum class FileChangeKind : uint8_t
    {
        Created,
        Modified,
        Deleted,
    };

    struct FileChange
    {
        wax::String<> path;
        FileChangeKind kind;
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
        int64_t mtime{0};
        int64_t size{0};
    };

    /// Polling-based file watcher using mtime + size checks.
    class PollingFileWatcher final : public IFileWatcher
    {
    public:
        PollingFileWatcher(comb::DefaultAllocator& alloc, uint32_t interval_ms = 500);

        void Watch(wax::StringView directory) override;
        void Poll(wax::Vector<FileChange>& changes) override;

        [[nodiscard]] size_t WatchedDirCount() const noexcept;

        /// Force a rescan regardless of interval (for testing).
        void ForcePoll(wax::Vector<FileChange>& changes);

    private:
        void ScanDirectories(wax::Vector<FileChange>& changes);
        void ScanDirectory(wax::StringView dir, wax::Vector<FileChange>& changes);

        comb::DefaultAllocator* alloc_;
        uint32_t interval_ms_;
        int64_t last_poll_time_{0};
        wax::Vector<wax::String<>> watched_dirs_;
        wax::HashMap<wax::String<>, FileSnapshot> known_files_;
    };
}
