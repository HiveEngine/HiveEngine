#include <nectar/vfs/memory_mount.h>
#include <nectar/vfs/path.h>
#include <wax/containers/hash_set.h>

namespace nectar
{
    MemoryMountSource::MemoryMountSource(comb::DefaultAllocator& alloc)
        : alloc_{&alloc}
        , files_{alloc, 64}
    {}

    void MemoryMountSource::AddFile(wax::StringView path, wax::ByteSpan data)
    {
        wax::String<> key{*alloc_};
        key.Append(path.Data(), path.Size());

        // Check if already exists — remove first to overwrite
        if (files_.Contains(key.View()))
            files_.Remove(key.View());

        wax::Vector<uint8_t> bytes{*alloc_};
        if (data.Size() > 0)
        {
            bytes.Reserve(data.Size());
            for (size_t i = 0; i < data.Size(); ++i)
                bytes.PushBack(data[i]);
        }

        files_.Insert(static_cast<wax::String<>&&>(key), static_cast<wax::Vector<uint8_t>&&>(bytes));
    }

    bool MemoryMountSource::RemoveFile(wax::StringView path)
    {
        return files_.Remove(path);
    }

    size_t MemoryMountSource::FileCount() const noexcept
    {
        return files_.Count();
    }

    wax::ByteBuffer<> MemoryMountSource::ReadFile(wax::StringView path, comb::DefaultAllocator& alloc)
    {
        wax::ByteBuffer<> buffer{alloc};
        auto* data = files_.Find(path);
        if (!data) return buffer;

        if (data->Size() > 0)
        {
            buffer.Resize(data->Size());
            for (size_t i = 0; i < data->Size(); ++i)
                buffer.Data()[i] = (*data)[i];
        }
        return buffer;
    }

    bool MemoryMountSource::Exists(wax::StringView path) const
    {
        return files_.Contains(path);
    }

    FileInfo MemoryMountSource::Stat(wax::StringView path) const
    {
        auto* data = files_.Find(path);
        if (!data)
            return FileInfo{0, false};
        return FileInfo{data->Size(), true};
    }

    void MemoryMountSource::ListDirectory(wax::StringView path,
                                           wax::Vector<DirectoryEntry>& out,
                                           comb::DefaultAllocator& alloc) const
    {
        // Build prefix to match: "dir/" or "" for root
        wax::String<> prefix{alloc};
        if (path.Size() > 0)
        {
            prefix.Append(path.Data(), path.Size());
            prefix.Append('/');
        }

        // Collect unique direct children
        wax::HashSet<wax::String<>> seen{alloc};

        for (auto it = files_.begin(); it != files_.end(); ++it)
        {
            auto key = it.Key().View();

            // Must start with prefix
            if (prefix.Size() > 0 && !key.StartsWith(prefix.View()))
                continue;
            if (prefix.Size() == 0)
            {
                // Root listing — everything is a match
            }

            // Get the part after the prefix
            auto remainder = key.Substr(prefix.Size());
            if (remainder.Size() == 0) continue;

            // Find first slash in remainder — if present, this is a subdirectory
            size_t slash = remainder.Find('/');
            bool is_dir = (slash != wax::StringView::npos);
            auto name = is_dir ? remainder.Substr(0, slash) : remainder;

            wax::String<> name_str{alloc};
            name_str.Append(name.Data(), name.Size());

            if (seen.Contains(name_str))
                continue;
            seen.Insert(static_cast<wax::String<>&&>(wax::String<>{alloc, name_str.View()}));

            DirectoryEntry entry;
            entry.name = static_cast<wax::String<>&&>(name_str);
            entry.is_directory = is_dir;
            out.PushBack(static_cast<DirectoryEntry&&>(entry));
        }
    }
}
