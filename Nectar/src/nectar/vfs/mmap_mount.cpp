#define _CRT_SECURE_NO_WARNINGS
#include <nectar/vfs/mmap_mount.h>
#include <nectar/io/mapped_file.h>
#include <cstdio>
#include <filesystem>

namespace nectar
{
    MmapMountSource::MmapMountSource(wax::StringView root_dir, comb::DefaultAllocator& alloc)
        : alloc_{&alloc}
        , root_dir_{alloc}
    {
        root_dir_.Append(root_dir.Data(), root_dir.Size());
    }

    wax::String<> MmapMountSource::BuildFullPath(wax::StringView relative,
                                                   comb::DefaultAllocator& alloc) const
    {
        wax::String<> full{alloc};
        if (root_dir_.Size() > 0)
        {
            full.Append(root_dir_.View().Data(), root_dir_.View().Size());
            full.Append('/');
        }
        full.Append(relative.Data(), relative.Size());
        return full;
    }

    wax::ByteBuffer<> MmapMountSource::ReadFile(wax::StringView path,
                                                  comb::DefaultAllocator& alloc)
    {
        wax::ByteBuffer<> buffer{alloc};
        auto full = BuildFullPath(path, alloc);

        auto mapped = MappedFile::Open(full.View());
        if (!mapped.IsValid()) return buffer;

        buffer.Append(mapped.Data(), mapped.Size());
        return buffer;
    }

    bool MmapMountSource::Exists(wax::StringView path) const
    {
        auto full = BuildFullPath(path, *alloc_);
        std::FILE* file = std::fopen(full.CStr(), "rb");
        if (!file) return false;
        std::fclose(file);
        return true;
    }

    FileInfo MmapMountSource::Stat(wax::StringView path) const
    {
        auto full = BuildFullPath(path, *alloc_);
        std::FILE* file = std::fopen(full.CStr(), "rb");
        if (!file) return FileInfo{0, false};

        std::fseek(file, 0, SEEK_END);
        long size = std::ftell(file);
        std::fclose(file);

        return FileInfo{size > 0 ? static_cast<size_t>(size) : 0, true};
    }

    void MmapMountSource::ListDirectory(wax::StringView path,
                                          wax::Vector<DirectoryEntry>& out,
                                          comb::DefaultAllocator& alloc) const
    {
        auto full = BuildFullPath(path, alloc);

        std::error_code ec;
        auto it = std::filesystem::directory_iterator(full.CStr(), ec);
        if (ec) return;

        for (const auto& entry : it)
        {
            auto filename = entry.path().filename().string();
            DirectoryEntry de;
            de.name = wax::String<>{alloc};
            de.name.Append(filename.c_str(), filename.size());
            de.is_directory = entry.is_directory();
            out.PushBack(static_cast<DirectoryEntry&&>(de));
        }
    }
}
