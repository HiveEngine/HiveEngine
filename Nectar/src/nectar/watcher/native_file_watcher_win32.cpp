#include <hive/hive_config.h>

#include <nectar/watcher/file_watcher.h>

#if HIVE_PLATFORM_WINDOWS

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <comb/default_allocator.h>
#include <comb/new.h>

#include <windows.h>

namespace nectar
{
    struct NativeFileWatcher::PlatformData
    {
        struct DirWatch
        {
            HANDLE hDir{INVALID_HANDLE_VALUE};
            OVERLAPPED overlapped{};
            alignas(DWORD) uint8_t buffer[64 * 1024]{};
            wax::String dirPath;
        };

        wax::Vector<DirWatch*> watches;
        HANDLE hCompletionPort{INVALID_HANDLE_VALUE};
    };

    namespace
    {
        bool HasDotComponent(const char* path, size_t len)
        {
            size_t componentStart = 0;
            for (size_t i = 0; i <= len; ++i)
            {
                if (i == len || path[i] == '/' || path[i] == '\\')
                {
                    if (i > componentStart && path[componentStart] == '.')
                    {
                        return true;
                    }
                    componentStart = i + 1;
                }
            }
            return false;
        }

        FileChangeKind MapAction(DWORD action)
        {
            switch (action)
            {
                case FILE_ACTION_ADDED:
                case FILE_ACTION_RENAMED_NEW_NAME:
                    return FileChangeKind::CREATED;
                case FILE_ACTION_REMOVED:
                case FILE_ACTION_RENAMED_OLD_NAME:
                    return FileChangeKind::DELETED;
                case FILE_ACTION_MODIFIED:
                default:
                    return FileChangeKind::MODIFIED;
            }
        }

        bool IssueRead(HANDLE hDir, void* buffer, DWORD bufferSize, OVERLAPPED* overlapped)
        {
            constexpr DWORD kFilter = FILE_NOTIFY_CHANGE_FILE_NAME | FILE_NOTIFY_CHANGE_DIR_NAME |
                                      FILE_NOTIFY_CHANGE_SIZE | FILE_NOTIFY_CHANGE_LAST_WRITE;

            BOOL ok = ReadDirectoryChangesW(hDir, buffer, bufferSize, TRUE, kFilter, nullptr, overlapped, nullptr);
            return ok != FALSE;
        }
    } // namespace

    void NativeFileWatcher::PlatformInit()
    {
        m_platform = comb::New<PlatformData>(*m_alloc);
        m_platform->watches = wax::Vector<PlatformData::DirWatch*>{*m_alloc};
        m_platform->hCompletionPort = CreateIoCompletionPort(INVALID_HANDLE_VALUE, nullptr, 0, 1);
    }

    void NativeFileWatcher::PlatformShutdown()
    {
        for (size_t i = 0; i < m_platform->watches.Size(); ++i)
        {
            auto* w = m_platform->watches[i];
            CancelIo(w->hDir);
            CloseHandle(w->hDir);
            comb::Delete(*m_alloc, w);
        }
        m_platform->watches.Clear();

        if (m_platform->hCompletionPort != INVALID_HANDLE_VALUE)
        {
            CloseHandle(m_platform->hCompletionPort);
        }

        comb::Delete(*m_alloc, m_platform);
        m_platform = nullptr;
    }

    void NativeFileWatcher::PlatformAddWatch(wax::StringView directory)
    {
        wax::String dirUtf8{*m_alloc, directory};
        int wideLen = MultiByteToWideChar(CP_UTF8, 0, dirUtf8.CStr(), static_cast<int>(dirUtf8.Size()), nullptr, 0);
        if (wideLen <= 0)
        {
            return;
        }

        const size_t wideCount = static_cast<size_t>(wideLen) + 1;
        auto* wideBuf = comb::NewArray<wchar_t>(*m_alloc, wideCount);
        MultiByteToWideChar(CP_UTF8, 0, dirUtf8.CStr(), static_cast<int>(dirUtf8.Size()), wideBuf, wideLen);
        wideBuf[wideLen] = L'\0';

        HANDLE hDir = CreateFileW(wideBuf, FILE_LIST_DIRECTORY, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                                  nullptr, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OVERLAPPED, nullptr);
        comb::DeleteArray(*m_alloc, wideBuf, wideCount);

        if (hDir == INVALID_HANDLE_VALUE)
        {
            return;
        }

        auto* watch = comb::New<PlatformData::DirWatch>(*m_alloc);
        watch->hDir = hDir;
        watch->dirPath = wax::String{*m_alloc};
        watch->dirPath.Append(directory.Data(), directory.Size());

        if (watch->dirPath.Size() > 0)
        {
            char last = watch->dirPath[watch->dirPath.Size() - 1];
            if (last != '/' && last != '\\')
            {
                watch->dirPath.Append("/", 1);
            }
        }

        CreateIoCompletionPort(hDir, m_platform->hCompletionPort, reinterpret_cast<ULONG_PTR>(watch), 0);

        if (!IssueRead(watch->hDir, watch->buffer, sizeof(watch->buffer), &watch->overlapped))
        {
            CloseHandle(hDir);
            comb::Delete(*m_alloc, watch);
            return;
        }

        std::lock_guard<std::mutex> lock{m_watchMutex};
        m_platform->watches.PushBack(watch);
    }

    void NativeFileWatcher::PlatformTick()
    {
        for (;;)
        {
            DWORD bytesTransferred{};
            ULONG_PTR completionKey{};
            OVERLAPPED* pOverlapped{};

            BOOL result = GetQueuedCompletionStatus(m_platform->hCompletionPort, &bytesTransferred, &completionKey,
                                                    &pOverlapped, 0);

            if (!result || pOverlapped == nullptr)
                break;

            auto* watch = reinterpret_cast<PlatformData::DirWatch*>(completionKey);

            if (bytesTransferred == 0)
            {
                IssueRead(watch->hDir, watch->buffer, sizeof(watch->buffer), &watch->overlapped);
                continue;
            }

            auto* info = reinterpret_cast<FILE_NOTIFY_INFORMATION*>(watch->buffer);

            for (;;)
            {
                int utf8Len = WideCharToMultiByte(CP_UTF8, 0, info->FileName,
                                                  static_cast<int>(info->FileNameLength / sizeof(wchar_t)), nullptr, 0,
                                                  nullptr, nullptr);

                if (utf8Len > 0)
                {
                    wax::String relativePath{*m_alloc};
                    relativePath.Resize(static_cast<size_t>(utf8Len));
                    WideCharToMultiByte(CP_UTF8, 0, info->FileName,
                                        static_cast<int>(info->FileNameLength / sizeof(wchar_t)), relativePath.Data(),
                                        utf8Len, nullptr, nullptr);

                    if (!HasDotComponent(relativePath.Data(), relativePath.Size()))
                    {
                        wax::String fullPath{*m_alloc};
                        fullPath.Append(watch->dirPath.Data(), watch->dirPath.Size());
                        fullPath.Append(relativePath.Data(), relativePath.Size());

                        EnqueueChange(fullPath.View(), MapAction(info->Action));
                    }
                }

                if (info->NextEntryOffset == 0)
                {
                    break;
                }

                info = reinterpret_cast<FILE_NOTIFY_INFORMATION*>(reinterpret_cast<uint8_t*>(info) +
                                                                  info->NextEntryOffset);
            }

            IssueRead(watch->hDir, watch->buffer, sizeof(watch->buffer), &watch->overlapped);
        }
    }
} // namespace nectar

#endif // HIVE_PLATFORM_WINDOWS
