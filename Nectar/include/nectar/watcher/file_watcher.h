#pragma once

#include <comb/default_allocator.h>

#include <wax/containers/hash_map.h>
#include <wax/containers/string.h>
#include <wax/containers/string_view.h>
#include <wax/containers/vector.h>

#include <atomic>
#include <cstdint>
#include <mutex>
#include <thread>

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

        void Baseline();

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

    /// Native OS file watcher using platform notification APIs.
    class NativeFileWatcher final : public IFileWatcher
    {
    public:
        explicit NativeFileWatcher(comb::DefaultAllocator& alloc);
        ~NativeFileWatcher() override;

        NativeFileWatcher(const NativeFileWatcher&) = delete;
        NativeFileWatcher& operator=(const NativeFileWatcher&) = delete;

        void Watch(wax::StringView directory) override;
        void Poll(wax::Vector<FileChange>& changes) override;

        bool SaveState(wax::StringView path) const;
        bool LoadState(wax::StringView path);

        // TODO: for 100k+ assets, run this on a background thread to avoid blocking editor startup
        void DetectOfflineChanges(wax::Vector<FileChange>& changes);

        void Shutdown();
        void EnqueueChange(wax::StringView path, FileChangeKind kind);

    private:
        void ThreadMain();
        void PlatformInit();
        void PlatformWakeThread();
        void PlatformShutdown();
        void PlatformAddWatch(wax::StringView directory);
        static bool IsDotDirectory(wax::StringView name);

        comb::DefaultAllocator* m_alloc;
        std::thread m_thread;
        std::atomic<bool> m_running{false};
        std::mutex m_queueMutex;
        wax::Vector<FileChange> m_pendingChanges;
        std::mutex m_watchMutex;
        wax::Vector<wax::String> m_watchedDirs;
        wax::HashMap<wax::String, FileSnapshot> m_knownFiles;
        struct PlatformData;
        PlatformData* m_platform{nullptr};
    };

    IFileWatcher* CreateFileWatcher(comb::DefaultAllocator& alloc);
} // namespace nectar
