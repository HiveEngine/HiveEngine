#include <nectar/vfs/pak_mount.h>

#include <cstring>

namespace nectar
{
    PakMountSource::PakMountSource(PakReader* reader, comb::DefaultAllocator& alloc)
        : m_alloc{&alloc}
        , m_reader{reader}
    {
    }

    PakMountSource::~PakMountSource()
    {
        if (m_reader != nullptr)
        {
            m_reader->~PakReader();
            m_alloc->Deallocate(m_reader);
        }
    }

    wax::ByteBuffer PakMountSource::ReadFile(wax::StringView path, comb::DefaultAllocator& alloc)
    {
        const auto* manifest = m_reader->GetManifest();
        if (manifest == nullptr)
        {
            return wax::ByteBuffer{alloc};
        }

        const auto* hash = manifest->Find(path);
        if (hash == nullptr)
        {
            return wax::ByteBuffer{alloc};
        }

        return m_reader->Read(*hash, alloc);
    }

    bool PakMountSource::Exists(wax::StringView path) const
    {
        const auto* manifest = m_reader->GetManifest();
        if (manifest == nullptr)
        {
            return false;
        }

        return manifest->Find(path) != nullptr;
    }

    FileInfo PakMountSource::Stat(wax::StringView path) const
    {
        const auto* manifest = m_reader->GetManifest();
        if (manifest == nullptr)
        {
            return FileInfo{0, false};
        }

        const auto* hash = manifest->Find(path);
        if (hash == nullptr)
        {
            return FileInfo{0, false};
        }

        const size_t size = m_reader->GetAssetSize(*hash);
        return FileInfo{size, true};
    }

    void PakMountSource::ListDirectory(wax::StringView path, wax::Vector<DirectoryEntry>& out,
                                       comb::DefaultAllocator& alloc) const
    {
        const auto* manifest = m_reader->GetManifest();
        if (manifest == nullptr)
        {
            return;
        }

        wax::String prefix{alloc};
        if (path.Size() > 0)
        {
            prefix.Append(path.Data(), path.Size());
            if (path.Data()[path.Size() - 1] != '/')
            {
                prefix.Append('/');
            }
        }

        manifest->ForEach([&](wax::StringView entryPath, ContentHash) {
            if (prefix.Size() > 0)
            {
                if (entryPath.Size() <= prefix.Size())
                {
                    return;
                }

                bool match = true;
                for (size_t i = 0; i < prefix.Size(); ++i)
                {
                    if (entryPath.Data()[i] != prefix.CStr()[i])
                    {
                        match = false;
                        break;
                    }
                }

                if (!match)
                {
                    return;
                }
            }

            const char* rest = entryPath.Data() + prefix.Size();
            const size_t restLen = entryPath.Size() - prefix.Size();

            size_t slashPos = restLen;
            for (size_t i = 0; i < restLen; ++i)
            {
                if (rest[i] == '/')
                {
                    slashPos = i;
                    break;
                }
            }

            const wax::StringView component{rest, slashPos};
            const bool isDir = slashPos < restLen;

            for (size_t i = 0; i < out.Size(); ++i)
            {
                if (out[i].m_name.Size() == component.Size() &&
                    std::memcmp(out[i].m_name.CStr(), component.Data(), component.Size()) == 0)
                {
                    return;
                }
            }

            DirectoryEntry entry{};
            entry.m_name = wax::String{alloc};
            entry.m_name.Append(component.Data(), component.Size());
            entry.m_isDirectory = isDir;
            out.PushBack(static_cast<DirectoryEntry&&>(entry));
        });
    }
} // namespace nectar
