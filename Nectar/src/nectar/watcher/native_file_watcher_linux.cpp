#include <hive/hive_config.h>

#include <nectar/watcher/file_watcher.h>

#if HIVE_PLATFORM_LINUX

#include <comb/new.h>
#include <hive/core/log.h>

#include <poll.h>
#include <sys/inotify.h>
#include <unistd.h>

namespace nectar
{
    struct NativeFileWatcher::PlatformData
    {
        int inotifyFd{-1};
        int wakeReadFd{-1};
        int wakeWriteFd{-1};
    };

    void NativeFileWatcher::PlatformInit()
    {
        m_platform = comb::New<PlatformData>(*m_alloc);

        m_platform->inotifyFd = inotify_init1(IN_NONBLOCK | IN_CLOEXEC);
        if (m_platform->inotifyFd < 0)
        {
            hive::LogWarning(hive::LOG_HIVE_ROOT, "NativeFileWatcher: inotify_init1 failed");
        }

        int pipeFds[2]{-1, -1};
        if (pipe(pipeFds) == 0)
        {
            m_platform->wakeReadFd = pipeFds[0];
            m_platform->wakeWriteFd = pipeFds[1];
        }
    }

    void NativeFileWatcher::PlatformWakeThread()
    {
        if (m_platform != nullptr && m_platform->wakeWriteFd >= 0)
        {
            char c = 'w';
            [[maybe_unused]] auto r = write(m_platform->wakeWriteFd, &c, 1);
        }
    }

    void NativeFileWatcher::PlatformShutdown()
    {
        if (m_platform->inotifyFd >= 0)
        {
            close(m_platform->inotifyFd);
        }
        if (m_platform->wakeReadFd >= 0)
        {
            close(m_platform->wakeReadFd);
        }
        if (m_platform->wakeWriteFd >= 0)
        {
            close(m_platform->wakeWriteFd);
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

    void NativeFileWatcher::ThreadMain()
    {
        alignas(struct inotify_event) uint8_t buffer[4096]{};

        while (m_running.load(std::memory_order_acquire))
        {
            struct pollfd fds[2]{};
            fds[0].fd = m_platform->inotifyFd;
            fds[0].events = POLLIN;
            fds[1].fd = m_platform->wakeReadFd;
            fds[1].events = POLLIN;

            int ret = poll(fds, 2, 500);
            if (!m_running.load(std::memory_order_acquire))
            {
                break;
            }

            if (ret <= 0)
            {
                continue;
            }

            // Drain wake pipe
            if (fds[1].revents & POLLIN)
            {
                char drain[64];
                while (read(m_platform->wakeReadFd, drain, sizeof(drain)) > 0)
                {
                }
            }

            if (!(fds[0].revents & POLLIN))
            {
                continue;
            }

            ssize_t len = read(m_platform->inotifyFd, buffer, sizeof(buffer));
            if (len <= 0)
            {
                continue;
            }

            for (ssize_t offset = 0; offset < len;)
            {
                auto* event = reinterpret_cast<const struct inotify_event*>(buffer + offset);
                offset += static_cast<ssize_t>(sizeof(struct inotify_event) + event->len);

                if (event->len == 0)
                {
                    continue;
                }

                wax::StringView name{event->name};
                if (IsDotDirectory(name))
                {
                    continue;
                }

                FileChangeKind kind = FileChangeKind::MODIFIED;
                if (event->mask & (IN_CREATE | IN_MOVED_TO))
                {
                    kind = FileChangeKind::CREATED;
                }
                else if (event->mask & (IN_DELETE | IN_MOVED_FROM))
                {
                    kind = FileChangeKind::DELETED;
                }

                EnqueueChange(name, kind);
            }
        }
    }
} // namespace nectar

#endif // HIVE_PLATFORM_LINUX
