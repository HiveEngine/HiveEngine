#pragma once

#include <hive/hive_config.h>

#include <comb/default_allocator.h>

#include <wax/containers/hash_map.h>
#include <wax/containers/string.h>
#include <wax/containers/string_view.h>
#include <wax/containers/vector.h>

#include <drone/service_thread.h>

#include <atomic>
#include <cstdint>
#include <mutex>

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
    class HIVE_API PollingFileWatcher final : public IFileWatcher
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

    class HIVE_API NativeFileWatcher final : public IFileWatcher, public drone::IService
    {
    public:
        explicit NativeFileWatcher(comb::DefaultAllocator& alloc);
        ~NativeFileWatcher() override;

        NativeFileWatcher(const NativeFileWatcher&) = delete;
        NativeFileWatcher& operator=(const NativeFileWatcher&) = delete;

        void Watch(wax::StringView directory) override;
        void Poll(wax::Vector<FileChange>& changes) override;
        void Tick() override;

        bool SaveState(wax::StringView path) const;
        bool LoadState(wax::StringView path);

        void DetectOfflineChanges(wax::Vector<FileChange>& changes);

        void Shutdown();
        void EnqueueChange(wax::StringView path, FileChangeKind kind);

    private:
        void PlatformInit();
        void PlatformShutdown();
        void PlatformAddWatch(wax::StringView directory);
        void PlatformTick();
        static bool IsDotDirectory(wax::StringView name);

        comb::DefaultAllocator* m_alloc;
        std::atomic<bool> m_running{false};
        std::mutex m_queueMutex;
        wax::Vector<FileChange> m_pendingChanges;
        std::mutex m_watchMutex;
        wax::Vector<wax::String> m_watchedDirs;
        wax::HashMap<wax::String, FileSnapshot> m_knownFiles;
        struct PlatformData;
        PlatformData* m_platform{nullptr};
    };

    HIVE_API IFileWatcher* CreateFileWatcher(comb::DefaultAllocator& alloc);
} // namespace nectar
