#include <wax/containers/hash_set.h>

#include <nectar/vfs/memory_mount.h>

namespace nectar
{
    MemoryMountSource::MemoryMountSource(comb::DefaultAllocator& alloc)
        : m_alloc{&alloc}
        , m_files{alloc, 64}
    {
    }

    void MemoryMountSource::AddFile(wax::StringView path, wax::ByteSpan data)
    {
        wax::String key{*m_alloc};
        key.Append(path.Data(), path.Size());

        if (m_files.Contains(key.View()))
        {
            m_files.Remove(key.View());
        }

        wax::Vector<uint8_t> bytes{*m_alloc};
        if (data.Size() > 0)
        {
            bytes.Reserve(data.Size());
            for (size_t i = 0; i < data.Size(); ++i)
            {
                bytes.PushBack(data[i]);
            }
        }

        m_files.Insert(static_cast<wax::String&&>(key), static_cast<wax::Vector<uint8_t>&&>(bytes));
    }

    bool MemoryMountSource::RemoveFile(wax::StringView path)
    {
        return m_files.Remove(path);
    }

    size_t MemoryMountSource::FileCount() const noexcept
    {
        return m_files.Count();
    }

    wax::ByteBuffer MemoryMountSource::ReadFile(wax::StringView path, comb::DefaultAllocator& alloc)
    {
        wax::ByteBuffer buffer{alloc};
        auto* data = m_files.Find(path);
        if (data == nullptr)
        {
            return buffer;
        }

        if (data->Size() > 0)
        {
            buffer.Resize(data->Size());
            for (size_t i = 0; i < data->Size(); ++i)
            {
                buffer.Data()[i] = (*data)[i];
            }
        }

        return buffer;
    }

    bool MemoryMountSource::Exists(wax::StringView path) const
    {
        return m_files.Contains(path);
    }

    FileInfo MemoryMountSource::Stat(wax::StringView path) const
    {
        auto* data = m_files.Find(path);
        if (data == nullptr)
        {
            return FileInfo{0, false};
        }

        return FileInfo{data->Size(), true};
    }

    void MemoryMountSource::ListDirectory(wax::StringView path, wax::Vector<DirectoryEntry>& out,
                                          comb::DefaultAllocator& alloc) const
    {
        wax::String prefix{alloc};
        if (path.Size() > 0)
        {
            prefix.Append(path.Data(), path.Size());
            prefix.Append('/');
        }

        wax::HashSet<wax::String> seen{alloc};

        for (auto it = m_files.Begin(); it != m_files.End(); ++it)
        {
            const wax::StringView key = it.Key().View();
            if (prefix.Size() > 0 && !key.StartsWith(prefix.View()))
            {
                continue;
            }

            const wax::StringView remainder = key.Substr(prefix.Size());
            if (remainder.Size() == 0)
            {
                continue;
            }

            const size_t slash = remainder.Find('/');
            const bool isDir = slash != wax::StringView::npos;
            const wax::StringView name = isDir ? remainder.Substr(0, slash) : remainder;

            wax::String nameStr{alloc};
            nameStr.Append(name.Data(), name.Size());

            if (seen.Contains(nameStr))
            {
                continue;
            }

            seen.Insert(wax::String{alloc, nameStr.View()});

            DirectoryEntry entry{};
            entry.m_name = static_cast<wax::String&&>(nameStr);
            entry.m_isDirectory = isDir;
            out.PushBack(static_cast<DirectoryEntry&&>(entry));
        }
    }
} // namespace nectar
