#include <hive/profiling/profiler.h>

#include <wax/containers/hash_set.h>

#include <nectar/vfs/path.h>
#include <nectar/vfs/virtual_filesystem.h>

namespace nectar
{
    VirtualFilesystem::VirtualFilesystem(comb::DefaultAllocator& alloc)
        : m_alloc{&alloc}
        , m_mounts{alloc}
    {
    }

    void VirtualFilesystem::Mount(wax::StringView mountPoint, MountSource* source, int priority)
    {
        if (source == nullptr)
        {
            return;
        }

        wax::String normalized = NormalizePath(mountPoint, *m_alloc);

        MountEntry entry{};
        entry.m_prefix = static_cast<wax::String&&>(normalized);
        entry.m_source = source;
        entry.m_priority = priority;

        size_t insertAt = m_mounts.Size();
        for (size_t i = 0; i < m_mounts.Size(); ++i)
        {
            if (priority > m_mounts[i].m_priority)
            {
                insertAt = i;
                break;
            }
        }

        m_mounts.PushBack(static_cast<MountEntry&&>(entry));
        for (size_t i = m_mounts.Size() - 1; i > insertAt; --i)
        {
            MountEntry tmp = static_cast<MountEntry&&>(m_mounts[i - 1]);
            m_mounts[i - 1] = static_cast<MountEntry&&>(m_mounts[i]);
            m_mounts[i] = static_cast<MountEntry&&>(tmp);
        }
    }

    void VirtualFilesystem::Unmount(wax::StringView mountPoint, MountSource* source)
    {
        const wax::String normalized = NormalizePath(mountPoint, *m_alloc);

        for (size_t i = 0; i < m_mounts.Size(); ++i)
        {
            if (m_mounts[i].m_source == source && m_mounts[i].m_prefix.View().Equals(normalized.View()))
            {
                if (i < m_mounts.Size() - 1)
                {
                    m_mounts[i] = static_cast<MountEntry&&>(m_mounts[m_mounts.Size() - 1]);
                }
                m_mounts.PopBack();

                for (size_t a = 0; a < m_mounts.Size(); ++a)
                {
                    for (size_t b = a + 1; b < m_mounts.Size(); ++b)
                    {
                        if (m_mounts[b].m_priority > m_mounts[a].m_priority)
                        {
                            MountEntry tmp = static_cast<MountEntry&&>(m_mounts[a]);
                            m_mounts[a] = static_cast<MountEntry&&>(m_mounts[b]);
                            m_mounts[b] = static_cast<MountEntry&&>(tmp);
                        }
                    }
                }
                return;
            }
        }
    }

    MountSource* VirtualFilesystem::Resolve(wax::StringView normalizedPath, wax::String& outRelative) const
    {
        for (size_t i = 0; i < m_mounts.Size(); ++i)
        {
            const MountEntry& mount = m_mounts[i];
            const wax::StringView prefix = mount.m_prefix.View();

            wax::StringView relative;
            if (prefix.Size() == 0)
            {
                relative = normalizedPath;
            }
            else if (normalizedPath.StartsWith(prefix))
            {
                if (normalizedPath.Size() == prefix.Size())
                {
                    relative = wax::StringView{};
                }
                else if (normalizedPath[prefix.Size()] == '/')
                {
                    relative = normalizedPath.Substr(prefix.Size() + 1);
                }
                else
                {
                    continue;
                }
            }
            else
            {
                continue;
            }

            if (mount.m_source->Exists(relative))
            {
                outRelative = wax::String{*m_alloc};
                outRelative.Append(relative.Data(), relative.Size());
                return mount.m_source;
            }
        }

        return nullptr;
    }

    wax::ByteBuffer VirtualFilesystem::ReadSync(wax::StringView path)
    {
        HIVE_PROFILE_SCOPE_N("VFS::ReadSync");

        const wax::String normalized = NormalizePath(path, *m_alloc);
        wax::String relative{*m_alloc};
        MountSource* source = Resolve(normalized.View(), relative);
        if (source == nullptr)
        {
            return wax::ByteBuffer{*m_alloc};
        }

        return source->ReadFile(relative.View(), *m_alloc);
    }

    bool VirtualFilesystem::Exists(wax::StringView path) const
    {
        const wax::String normalized = NormalizePath(path, *m_alloc);
        wax::String relative{*m_alloc};
        return Resolve(normalized.View(), relative) != nullptr;
    }

    FileInfo VirtualFilesystem::Stat(wax::StringView path) const
    {
        const wax::String normalized = NormalizePath(path, *m_alloc);
        wax::String relative{*m_alloc};
        MountSource* source = Resolve(normalized.View(), relative);
        if (source == nullptr)
        {
            return FileInfo{0, false};
        }

        return source->Stat(relative.View());
    }

    void VirtualFilesystem::ListDirectory(wax::StringView path, wax::Vector<DirectoryEntry>& out) const
    {
        const wax::String normalized = NormalizePath(path, *m_alloc);
        wax::HashSet<wax::String> seen{*m_alloc};

        for (size_t i = 0; i < m_mounts.Size(); ++i)
        {
            const MountEntry& mount = m_mounts[i];
            const wax::StringView prefix = mount.m_prefix.View();

            wax::StringView relative;
            if (prefix.Size() == 0)
            {
                relative = normalized.View();
            }
            else if (normalized.View().StartsWith(prefix))
            {
                if (normalized.View().Size() == prefix.Size())
                {
                    relative = wax::StringView{};
                }
                else if (normalized.View()[prefix.Size()] == '/')
                {
                    relative = normalized.View().Substr(prefix.Size() + 1);
                }
                else
                {
                    continue;
                }
            }
            else
            {
                continue;
            }

            wax::Vector<DirectoryEntry> entries{*m_alloc};
            mount.m_source->ListDirectory(relative, entries, *m_alloc);

            for (size_t j = 0; j < entries.Size(); ++j)
            {
                if (!seen.Contains(entries[j].m_name))
                {
                    seen.Insert(wax::String{*m_alloc, entries[j].m_name.View()});
                    out.PushBack(static_cast<DirectoryEntry&&>(entries[j]));
                }
            }
        }
    }

    size_t VirtualFilesystem::MountCount() const noexcept
    {
        return m_mounts.Size();
    }
} // namespace nectar
