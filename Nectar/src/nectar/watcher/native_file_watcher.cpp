#include <hive/HiveConfig.h>

#include <nectar/watcher/file_watcher.h>

#include <chrono>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>

namespace nectar
{
    namespace
    {
        constexpr uint32_t kStateMagic = 0x46575354; // "FWST"
        constexpr uint32_t kStateVersion = 1;
    } // namespace

    NativeFileWatcher::NativeFileWatcher(comb::DefaultAllocator& alloc)
        : m_alloc{&alloc}
        , m_pendingChanges{alloc}
        , m_watchedDirs{alloc}
        , m_knownFiles{alloc, 256}
    {
        PlatformInit();
        m_running.store(true, std::memory_order_release);
        m_thread = std::thread{&NativeFileWatcher::ThreadMain, this};
    }

    NativeFileWatcher::~NativeFileWatcher()
    {
        Shutdown();
    }

    void NativeFileWatcher::Shutdown()
    {
        if (!m_running.exchange(false, std::memory_order_acq_rel))
        {
            return;
        }

        PlatformWakeThread();

        if (m_thread.joinable())
        {
            m_thread.join();
        }

        PlatformShutdown();
    }

    void NativeFileWatcher::Watch(wax::StringView directory)
    {
        {
            std::lock_guard<std::mutex> lock{m_watchMutex};
            wax::String dir{*m_alloc};
            dir.Append(directory.Data(), directory.Size());
            m_watchedDirs.PushBack(static_cast<wax::String&&>(dir));
        }
        PlatformAddWatch(directory);
    }

    void NativeFileWatcher::Poll(wax::Vector<FileChange>& changes)
    {
        std::lock_guard<std::mutex> lock{m_queueMutex};
        for (size_t i = 0; i < m_pendingChanges.Size(); ++i)
        {
            changes.PushBack(static_cast<FileChange&&>(m_pendingChanges[i]));
        }
        m_pendingChanges.Clear();
    }

    void NativeFileWatcher::EnqueueChange(wax::StringView path, FileChangeKind kind)
    {
        wax::String normalized{*m_alloc};
        normalized.Append(path.Data(), path.Size());

        for (size_t i = 0; i < normalized.Size(); ++i)
        {
            if (normalized[i] == '\\')
            {
                normalized[i] = '/';
            }
        }

        FileChange change{};
        change.m_path = static_cast<wax::String&&>(normalized);
        change.m_kind = kind;

        std::lock_guard<std::mutex> lock{m_queueMutex};
        m_pendingChanges.PushBack(static_cast<FileChange&&>(change));
    }

    bool NativeFileWatcher::IsDotDirectory(wax::StringView name)
    {
        return name.Size() > 0 && name.Data()[0] == '.';
    }

    bool NativeFileWatcher::SaveState(wax::StringView path) const
    {
        std::string filePath{path.Data(), path.Size()};
        std::ofstream file{filePath, std::ios::binary};
        if (!file)
        {
            return false;
        }

        auto writeU32 = [&file](uint32_t value) {
            file.write(reinterpret_cast<const char*>(&value), sizeof(value));
        };
        auto writeI64 = [&file](int64_t value) {
            file.write(reinterpret_cast<const char*>(&value), sizeof(value));
        };

        writeU32(kStateMagic);
        writeU32(kStateVersion);

        uint32_t entryCount = 0;
        for (auto it = m_knownFiles.Begin(); it != m_knownFiles.End(); ++it)
        {
            ++entryCount;
        }
        writeU32(entryCount);

        for (auto it = m_knownFiles.Begin(); it != m_knownFiles.End(); ++it)
        {
            const auto& key = it.Key();
            const auto& value = it.Value();

            const auto pathLen = static_cast<uint32_t>(key.Size());
            writeU32(pathLen);
            file.write(key.Data(), pathLen);
            writeI64(value.m_mtime);
            writeI64(value.m_size);
        }

        return file.good();
    }

    bool NativeFileWatcher::LoadState(wax::StringView path)
    {
        std::string filePath{path.Data(), path.Size()};
        std::ifstream file{filePath, std::ios::binary};
        if (!file)
        {
            return false;
        }

        auto readU32 = [&file](uint32_t& value) -> bool {
            file.read(reinterpret_cast<char*>(&value), sizeof(value));
            return file.good();
        };
        auto readI64 = [&file](int64_t& value) -> bool {
            file.read(reinterpret_cast<char*>(&value), sizeof(value));
            return file.good();
        };

        uint32_t magic = 0;
        if (!readU32(magic) || magic != kStateMagic)
        {
            return false;
        }

        uint32_t version = 0;
        if (!readU32(version) || version != kStateVersion)
        {
            return false;
        }

        uint32_t entryCount = 0;
        if (!readU32(entryCount))
        {
            return false;
        }

        m_knownFiles.Clear();

        for (uint32_t i = 0; i < entryCount; ++i)
        {
            uint32_t pathLen = 0;
            if (!readU32(pathLen))
            {
                return false;
            }

            wax::String entryPath{*m_alloc};
            entryPath.Resize(pathLen);
            file.read(entryPath.Data(), pathLen);
            if (!file.good())
            {
                return false;
            }

            FileSnapshot snapshot{};
            if (!readI64(snapshot.m_mtime) || !readI64(snapshot.m_size))
            {
                return false;
            }

            m_knownFiles.Insert(static_cast<wax::String&&>(entryPath), snapshot);
        }

        return true;
    }

    void NativeFileWatcher::DetectOfflineChanges(wax::Vector<FileChange>& changes)
    {
        wax::HashMap<wax::String, bool> seen{*m_alloc};

        for (size_t i = 0; i < m_watchedDirs.Size(); ++i)
        {
            std::error_code ec;
            auto it = std::filesystem::recursive_directory_iterator(
                std::filesystem::path{std::string{m_watchedDirs[i].Data(), m_watchedDirs[i].Size()}}, ec);
            if (ec)
                continue;

            for (auto dirIt = std::filesystem::begin(it); dirIt != std::filesystem::end(it); ++dirIt)
            {
                if (dirIt->is_directory())
                {
                    auto dirName = dirIt->path().filename().string();
                    if (!dirName.empty() && dirName[0] == '.')
                        dirIt.disable_recursion_pending();
                    continue;
                }

                if (!dirIt->is_regular_file())
                    continue;

                std::string pathStr = dirIt->path().string();
                for (char& c : pathStr)
                    if (c == '\\')
                        c = '/';

                wax::String key{*m_alloc};
                key.Append(pathStr.c_str(), pathStr.size());

                auto fileTime = std::filesystem::last_write_time(dirIt->path(), ec);
                int64_t mtime =
                    ec ? 0 : std::chrono::duration_cast<std::chrono::milliseconds>(fileTime.time_since_epoch()).count();
                auto fileSize = std::filesystem::file_size(dirIt->path(), ec);
                int64_t size = ec ? 0 : static_cast<int64_t>(fileSize);

                wax::String seenKey{*m_alloc};
                seenKey.Append(pathStr.c_str(), pathStr.size());
                seen.Insert(static_cast<wax::String&&>(seenKey), true);

                auto* prev = m_knownFiles.Find(key);
                if (prev == nullptr)
                {
                    FileChange change{};
                    change.m_kind = FileChangeKind::CREATED;
                    change.m_path = wax::String{*m_alloc};
                    change.m_path.Append(pathStr.c_str(), pathStr.size());
                    changes.PushBack(static_cast<FileChange&&>(change));

                    wax::String insertKey{*m_alloc};
                    insertKey.Append(pathStr.c_str(), pathStr.size());
                    m_knownFiles.Insert(static_cast<wax::String&&>(insertKey), FileSnapshot{mtime, size});
                }
                else if (prev->m_mtime != mtime || prev->m_size != size)
                {
                    FileChange change{};
                    change.m_kind = FileChangeKind::MODIFIED;
                    change.m_path = wax::String{*m_alloc};
                    change.m_path.Append(pathStr.c_str(), pathStr.size());
                    changes.PushBack(static_cast<FileChange&&>(change));

                    prev->m_mtime = mtime;
                    prev->m_size = size;
                }
            }
        }

        wax::Vector<wax::String> toRemove{*m_alloc};
        for (auto it = m_knownFiles.Begin(); it != m_knownFiles.End(); ++it)
        {
            if (seen.Find(it.Key()) == nullptr)
            {
                FileChange change{};
                change.m_kind = FileChangeKind::DELETED;
                change.m_path = wax::String{*m_alloc};
                change.m_path.Append(it.Key().Data(), it.Key().Size());
                changes.PushBack(static_cast<FileChange&&>(change));

                wax::String rmKey{*m_alloc};
                rmKey.Append(it.Key().Data(), it.Key().Size());
                toRemove.PushBack(static_cast<wax::String&&>(rmKey));
            }
        }

        for (size_t i = 0; i < toRemove.Size(); ++i)
            m_knownFiles.Remove(toRemove[i]);
    }

    IFileWatcher* CreateFileWatcher(comb::DefaultAllocator& alloc)
    {
#if HIVE_PLATFORM_WINDOWS || HIVE_PLATFORM_LINUX
        return new NativeFileWatcher{alloc};
#else
        return new PollingFileWatcher{alloc};
#endif
    }
} // namespace nectar
