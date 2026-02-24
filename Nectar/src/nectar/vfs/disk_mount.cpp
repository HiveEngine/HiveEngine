#define _CRT_SECURE_NO_WARNINGS
#include <nectar/vfs/disk_mount.h>
#include <cstdio>
#include <filesystem>

namespace nectar
{
    DiskMountSource::DiskMountSource(wax::StringView root_dir, comb::DefaultAllocator& alloc)
        : alloc_{&alloc}
        , root_dir_{alloc}
    {
        root_dir_.Append(root_dir.Data(), root_dir.Size());
    }

    wax::StringView DiskMountSource::RootDir() const noexcept
    {
        return root_dir_.View();
    }

    wax::String<> DiskMountSource::BuildFullPath(wax::StringView relative,
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

    wax::ByteBuffer<> DiskMountSource::ReadFile(wax::StringView path,
                                                  comb::DefaultAllocator& alloc)
    {
        wax::ByteBuffer<> buffer{alloc};
        auto full = BuildFullPath(path, alloc);

        FILE* file = std::fopen(full.CStr(), "rb");
        if (!file) return buffer;

        std::fseek(file, 0, SEEK_END);
        long file_size = std::ftell(file);
        std::fseek(file, 0, SEEK_SET);

        if (file_size <= 0)
        {
            std::fclose(file);
            return buffer;
        }

        buffer.Resize(static_cast<size_t>(file_size));
        size_t read = std::fread(buffer.Data(), 1, static_cast<size_t>(file_size), file);
        std::fclose(file);

        if (read != static_cast<size_t>(file_size))
            buffer.Clear();

        return buffer;
    }

    bool DiskMountSource::Exists(wax::StringView path) const
    {
        auto full = BuildFullPath(path, *alloc_);
        FILE* file = std::fopen(full.CStr(), "rb");
        if (!file) return false;
        std::fclose(file);
        return true;
    }

    FileInfo DiskMountSource::Stat(wax::StringView path) const
    {
        auto full = BuildFullPath(path, *alloc_);
        FILE* file = std::fopen(full.CStr(), "rb");
        if (!file) return FileInfo{0, false};

        std::fseek(file, 0, SEEK_END);
        long size = std::ftell(file);
        std::fclose(file);

        return FileInfo{size > 0 ? static_cast<size_t>(size) : 0, true};
    }

    void DiskMountSource::ListDirectory(wax::StringView path,
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
