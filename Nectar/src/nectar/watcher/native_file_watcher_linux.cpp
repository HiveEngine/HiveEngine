#include <hive/hive_config.h>

#include <nectar/watcher/file_watcher.h>

#if HIVE_PLATFORM_LINUX

#include <comb/new.h>
#include <hive/core/log.h>

#include <sys/inotify.h>
#include <unistd.h>

namespace nectar
{
    struct NativeFileWatcher::PlatformData
    {
        int inotifyFd{-1};
    };

    void NativeFileWatcher::PlatformInit()
    {
        m_platform = comb::New<PlatformData>(*m_alloc);

        m_platform->inotifyFd = inotify_init1(IN_NONBLOCK | IN_CLOEXEC);
        if (m_platform->inotifyFd < 0)
        {
            hive::LogWarning(hive::LOG_HIVE_ROOT, "NativeFileWatcher: inotify_init1 failed");
        }
    }

    void NativeFileWatcher::PlatformShutdown()
    {
        if (m_platform->inotifyFd >= 0)
        {
            close(m_platform->inotifyFd);
        }

        comb::Delete(*m_alloc, m_platform);
        m_platform = nullptr;
    }

    void NativeFileWatcher::PlatformAddWatch(wax::StringView directory)
    {
        if (m_platform->inotifyFd < 0)
        {
            return;
        }

        wax::String dirPath{*m_alloc, directory};
        constexpr uint32_t kMask = IN_CREATE | IN_DELETE | IN_MODIFY | IN_MOVED_FROM | IN_MOVED_TO;
        int wd = inotify_add_watch(m_platform->inotifyFd, dirPath.CStr(), kMask);
        if (wd < 0)
        {
            hive::LogWarning(hive::LOG_HIVE_ROOT, "NativeFileWatcher: inotify_add_watch failed for {}", directory.Data());
        }
    }

    void NativeFileWatcher::PlatformTick()
    {
        alignas(struct inotify_event) uint8_t buffer[4096]{};

        for (;;)
        {
            ssize_t len = read(m_platform->inotifyFd, buffer, sizeof(buffer));
            if (len <= 0)
                break;

            for (ssize_t offset = 0; offset < len;)
            {
                auto* event = reinterpret_cast<const struct inotify_event*>(buffer + offset);
                offset += static_cast<ssize_t>(sizeof(struct inotify_event) + event->len);

                if (event->len == 0)
                    continue;

                wax::StringView name{event->name};
                if (IsDotDirectory(name))
                    continue;

                FileChangeKind kind = FileChangeKind::MODIFIED;
                if (event->mask & (IN_CREATE | IN_MOVED_TO))
                    kind = FileChangeKind::CREATED;
                else if (event->mask & (IN_DELETE | IN_MOVED_FROM))
                    kind = FileChangeKind::DELETED;

                EnqueueChange(name, kind);
            }
        }
    }
} // namespace nectar

#endif // HIVE_PLATFORM_LINUX
