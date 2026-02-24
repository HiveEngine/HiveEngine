#include <nectar/vfs/virtual_filesystem.h>
#include <nectar/vfs/path.h>
#include <hive/profiling/profiler.h>
#include <wax/containers/hash_set.h>

namespace nectar
{
    VirtualFilesystem::VirtualFilesystem(comb::DefaultAllocator& alloc)
        : alloc_{&alloc}
        , mounts_{alloc}
    {}

    void VirtualFilesystem::Mount(wax::StringView mount_point, MountSource* source, int priority)
    {
        if (!source) return;

        auto normalized = NormalizePath(mount_point, *alloc_);

        MountEntry entry;
        entry.prefix = static_cast<wax::String<>&&>(normalized);
        entry.source = source;
        entry.priority = priority;

        // Insert sorted by priority descending (highest first)
        size_t insert_at = mounts_.Size();
        for (size_t i = 0; i < mounts_.Size(); ++i)
        {
            if (priority > mounts_[i].priority)
            {
                insert_at = i;
                break;
            }
        }

        // No Vector::Insert — push back and bubble into position
        mounts_.PushBack(static_cast<MountEntry&&>(entry));
        for (size_t i = mounts_.Size() - 1; i > insert_at; --i)
        {
            // Swap mounts_[i] and mounts_[i-1]
            MountEntry tmp = static_cast<MountEntry&&>(mounts_[i - 1]);
            mounts_[i - 1] = static_cast<MountEntry&&>(mounts_[i]);
            mounts_[i] = static_cast<MountEntry&&>(tmp);
        }
    }

    void VirtualFilesystem::Unmount(wax::StringView mount_point, MountSource* source)
    {
        auto normalized = NormalizePath(mount_point, *alloc_);

        for (size_t i = 0; i < mounts_.Size(); ++i)
        {
            if (mounts_[i].source == source && mounts_[i].prefix.View().Equals(normalized.View()))
            {
                // Swap-remove
                if (i < mounts_.Size() - 1)
                    mounts_[i] = static_cast<MountEntry&&>(mounts_[mounts_.Size() - 1]);
                mounts_.PopBack();

                // Re-sort since swap-remove breaks ordering
                // Simple bubble sort — mounts list is tiny
                for (size_t a = 0; a < mounts_.Size(); ++a)
                {
                    for (size_t b = a + 1; b < mounts_.Size(); ++b)
                    {
                        if (mounts_[b].priority > mounts_[a].priority)
                        {
                            MountEntry tmp = static_cast<MountEntry&&>(mounts_[a]);
                            mounts_[a] = static_cast<MountEntry&&>(mounts_[b]);
                            mounts_[b] = static_cast<MountEntry&&>(tmp);
                        }
                    }
                }
                return;
            }
        }
    }

    MountSource* VirtualFilesystem::Resolve(wax::StringView normalized_path,
                                             wax::String<>& out_relative) const
    {
        for (size_t i = 0; i < mounts_.Size(); ++i)
        {
            const auto& mount = mounts_[i];
            auto prefix = mount.prefix.View();

            wax::StringView relative;
            if (prefix.Size() == 0)
            {
                // Root mount — matches everything
                relative = normalized_path;
            }
            else if (normalized_path.StartsWith(prefix))
            {
                if (normalized_path.Size() == prefix.Size())
                {
                    relative = wax::StringView{};
                }
                else if (normalized_path[prefix.Size()] == '/')
                {
                    relative = normalized_path.Substr(prefix.Size() + 1);
                }
                else
                {
                    continue;  // partial match, e.g. "assets" doesn't match "assets2/..."
                }
            }
            else
            {
                continue;
            }

            if (mount.source->Exists(relative))
            {
                out_relative = wax::String<>{*alloc_};
                out_relative.Append(relative.Data(), relative.Size());
                return mount.source;
            }
        }
        return nullptr;
    }

    wax::ByteBuffer<> VirtualFilesystem::ReadSync(wax::StringView path)
    {
        HIVE_PROFILE_SCOPE_N("VFS::ReadSync");
        auto normalized = NormalizePath(path, *alloc_);
        wax::String<> relative{*alloc_};
        auto* source = Resolve(normalized.View(), relative);
        if (!source)
            return wax::ByteBuffer<>{*alloc_};
        return source->ReadFile(relative.View(), *alloc_);
    }

    bool VirtualFilesystem::Exists(wax::StringView path) const
    {
        auto normalized = NormalizePath(path, *alloc_);
        wax::String<> relative{*alloc_};
        return Resolve(normalized.View(), relative) != nullptr;
    }

    FileInfo VirtualFilesystem::Stat(wax::StringView path) const
    {
        auto normalized = NormalizePath(path, *alloc_);
        wax::String<> relative{*alloc_};
        auto* source = Resolve(normalized.View(), relative);
        if (!source)
            return FileInfo{0, false};
        return source->Stat(relative.View());
    }

    void VirtualFilesystem::ListDirectory(wax::StringView path,
                                           wax::Vector<DirectoryEntry>& out) const
    {
        auto normalized = NormalizePath(path, *alloc_);
        wax::HashSet<wax::String<>> seen{*alloc_};

        for (size_t i = 0; i < mounts_.Size(); ++i)
        {
            const auto& mount = mounts_[i];
            auto prefix = mount.prefix.View();

            wax::StringView relative;
            if (prefix.Size() == 0)
            {
                relative = normalized.View();
            }
            else if (normalized.View().StartsWith(prefix))
            {
                if (normalized.View().Size() == prefix.Size())
                    relative = wax::StringView{};
                else if (normalized.View()[prefix.Size()] == '/')
                    relative = normalized.View().Substr(prefix.Size() + 1);
                else
                    continue;
            }
            else
            {
                continue;
            }

            wax::Vector<DirectoryEntry> entries{*alloc_};
            mount.source->ListDirectory(relative, entries, *alloc_);

            for (size_t j = 0; j < entries.Size(); ++j)
            {
                if (!seen.Contains(entries[j].name))
                {
                    seen.Insert(wax::String<>{*alloc_, entries[j].name.View()});
                    out.PushBack(static_cast<DirectoryEntry&&>(entries[j]));
                }
            }
        }
    }

    size_t VirtualFilesystem::MountCount() const noexcept
    {
        return mounts_.Size();
    }
}
