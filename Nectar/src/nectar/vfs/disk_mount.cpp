#include <nectar/vfs/disk_mount.h>

#include <cstdio>
#include <filesystem>
#include <system_error>

namespace nectar
{
    DiskMountSource::DiskMountSource(wax::StringView rootDir, comb::DefaultAllocator& alloc)
        : m_alloc{&alloc}
        , m_rootDir{alloc} {
        m_rootDir.Append(rootDir.Data(), rootDir.Size());
    }

    wax::StringView DiskMountSource::RootDir() const noexcept {
        return m_rootDir.View();
    }

    wax::String DiskMountSource::BuildFullPath(wax::StringView relative, comb::DefaultAllocator& alloc) const {
        wax::String full{alloc};
        if (m_rootDir.Size() > 0)
        {
            full.Append(m_rootDir.View().Data(), m_rootDir.View().Size());
            full.Append('/');
        }
        full.Append(relative.Data(), relative.Size());
        return full;
    }

    wax::ByteBuffer DiskMountSource::ReadFile(wax::StringView path, comb::DefaultAllocator& alloc) {
        wax::ByteBuffer buffer{alloc};
        const wax::String full = BuildFullPath(path, alloc);

        FILE* file = std::fopen(full.CStr(), "rb");
        if (file == nullptr)
        {
            return buffer;
        }

        std::fseek(file, 0, SEEK_END);
        const long fileSize = std::ftell(file);
        std::fseek(file, 0, SEEK_SET);

        if (fileSize <= 0)
        {
            std::fclose(file);
            return buffer;
        }

        buffer.Resize(static_cast<size_t>(fileSize));
        const size_t read = std::fread(buffer.Data(), 1, static_cast<size_t>(fileSize), file);
        std::fclose(file);

        if (read != static_cast<size_t>(fileSize))
        {
            buffer.Clear();
        }

        return buffer;
    }

    bool DiskMountSource::Exists(wax::StringView path) const {
        const wax::String full = BuildFullPath(path, *m_alloc);
        FILE* file = std::fopen(full.CStr(), "rb");
        if (file == nullptr)
        {
            return false;
        }

        std::fclose(file);
        return true;
    }

    FileInfo DiskMountSource::Stat(wax::StringView path) const {
        const wax::String full = BuildFullPath(path, *m_alloc);
        FILE* file = std::fopen(full.CStr(), "rb");
        if (file == nullptr)
        {
            return FileInfo{0, false};
        }

        std::fseek(file, 0, SEEK_END);
        const long size = std::ftell(file);
        std::fclose(file);

        return FileInfo{size > 0 ? static_cast<size_t>(size) : 0, true};
    }

    void DiskMountSource::ListDirectory(wax::StringView path, wax::Vector<DirectoryEntry>& out,
                                        comb::DefaultAllocator& alloc) const {
        const wax::String full = BuildFullPath(path, alloc);

        std::error_code error;
        const auto it = std::filesystem::directory_iterator(full.CStr(), error);
        if (error)
        {
            return;
        }

        for (const auto& entry : it)
        {
            const auto filename = entry.path().filename().string();
            DirectoryEntry dirEntry{};
            dirEntry.m_name = wax::String{alloc};
            dirEntry.m_name.Append(filename.c_str(), filename.size());
            dirEntry.m_isDirectory = entry.is_directory();
            out.PushBack(static_cast<DirectoryEntry&&>(dirEntry));
        }
    }
} // namespace nectar
