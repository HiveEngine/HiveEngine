#include <nectar/vfs/pak_mount.h>
#include <nectar/pak/asset_manifest.h>

namespace nectar
{
    PakMountSource::PakMountSource(PakReader* reader, comb::DefaultAllocator& alloc)
        : alloc_{&alloc}
        , reader_{reader}
    {}

    PakMountSource::~PakMountSource()
    {
        if (reader_)
        {
            reader_->~PakReader();
            alloc_->Deallocate(reader_);
        }
    }

    wax::ByteBuffer<> PakMountSource::ReadFile(wax::StringView path,
                                                 comb::DefaultAllocator& alloc)
    {
        const auto* manifest = reader_->GetManifest();
        if (!manifest)
            return wax::ByteBuffer<>{alloc};

        const auto* hash = manifest->Find(path);
        if (!hash)
            return wax::ByteBuffer<>{alloc};

        return reader_->Read(*hash, alloc);
    }

    bool PakMountSource::Exists(wax::StringView path) const
    {
        const auto* manifest = reader_->GetManifest();
        if (!manifest) return false;
        return manifest->Find(path) != nullptr;
    }

    FileInfo PakMountSource::Stat(wax::StringView path) const
    {
        const auto* manifest = reader_->GetManifest();
        if (!manifest) return FileInfo{0, false};

        const auto* hash = manifest->Find(path);
        if (!hash) return FileInfo{0, false};

        size_t size = reader_->GetAssetSize(*hash);
        return FileInfo{size, true};
    }

    void PakMountSource::ListDirectory(wax::StringView path,
                                        wax::Vector<DirectoryEntry>& out,
                                        comb::DefaultAllocator& alloc) const
    {
        const auto* manifest = reader_->GetManifest();
        if (!manifest) return;

        // Build prefix to match against
        // If path is "textures", we look for entries starting with "textures/"
        // If path is empty, we look for top-level entries
        wax::String<> prefix{alloc};
        if (path.Size() > 0)
        {
            prefix.Append(path.Data(), path.Size());
            // Add trailing slash if not present
            if (path.Data()[path.Size() - 1] != '/')
                prefix.Append('/');
        }

        manifest->ForEach([&](wax::StringView entry_path, ContentHash) {
            // Check if entry starts with our prefix
            if (prefix.Size() > 0)
            {
                if (entry_path.Size() <= prefix.Size())
                    return;
                bool match = true;
                for (size_t i = 0; i < prefix.Size(); ++i)
                {
                    if (entry_path.Data()[i] != prefix.CStr()[i])
                    {
                        match = false;
                        break;
                    }
                }
                if (!match) return;
            }

            // Extract the next path component after the prefix
            const char* rest = entry_path.Data() + prefix.Size();
            size_t rest_len = entry_path.Size() - prefix.Size();

            // Find next slash — if present, it's a directory component
            size_t slash_pos = rest_len;
            for (size_t i = 0; i < rest_len; ++i)
            {
                if (rest[i] == '/')
                {
                    slash_pos = i;
                    break;
                }
            }

            wax::StringView component{rest, slash_pos};
            bool is_dir = (slash_pos < rest_len);

            // Deduplicate
            for (size_t i = 0; i < out.Size(); ++i)
            {
                if (out[i].name.Size() == component.Size() &&
                    std::memcmp(out[i].name.CStr(), component.Data(), component.Size()) == 0)
                    return;
            }

            DirectoryEntry de;
            de.name = wax::String<>{alloc};
            de.name.Append(component.Data(), component.Size());
            de.is_directory = is_dir;
            out.PushBack(static_cast<DirectoryEntry&&>(de));
        });
    }
}
