#include <nectar/cas/cas_store.h>

#include <hive/profiling/profiler.h>

#include <wax/containers/string.h>

#include <cstdio>
#include <filesystem>

namespace nectar
{
    CasStore::CasStore(comb::DefaultAllocator& alloc, wax::StringView rootDir)
        : m_alloc{&alloc}
        , m_rootDir{alloc, rootDir}
    {
    }

    ContentHash CasStore::Store(wax::ByteSpan data)
    {
        HIVE_PROFILE_SCOPE_N("CasStore::Store");
        ContentHash hash = ContentHash::FromData(data);

        // Already exists — dedup
        if (Contains(hash))
            return hash;

        wax::String path{*m_alloc};
        BuildBlobPath(hash, path);

        auto parent = std::filesystem::path{path.CStr()}.parent_path();
        EnsureDirectoryExists(wax::StringView{parent.string().c_str()});

        FILE* file = std::fopen(path.CStr(), "wb");
        if (!file)
            return ContentHash::Invalid();

        if (data.Size() > 0)
            std::fwrite(data.Data(), 1, data.Size(), file);

        std::fclose(file);
        return hash;
    }

    wax::ByteBuffer CasStore::Load(ContentHash hash)
    {
        HIVE_PROFILE_SCOPE_N("CasStore::Load");
        wax::ByteBuffer buffer{*m_alloc};

        wax::String path{*m_alloc};
        BuildBlobPath(hash, path);

        FILE* file = std::fopen(path.CStr(), "rb");
        if (!file)
            return buffer;

        std::fseek(file, 0, SEEK_END);
        long fileSize = std::ftell(file);
        std::fseek(file, 0, SEEK_SET);

        if (fileSize > 0)
        {
            buffer.Resize(static_cast<size_t>(fileSize));
            std::fread(buffer.Data(), 1, static_cast<size_t>(fileSize), file);
        }

        std::fclose(file);
        return buffer;
    }

    bool CasStore::Contains(ContentHash hash) const
    {
        wax::String path{*m_alloc};
        BuildBlobPath(hash, path);
        return std::filesystem::exists(path.CStr());
    }

    bool CasStore::Remove(ContentHash hash)
    {
        wax::String path{*m_alloc};
        BuildBlobPath(hash, path);

        std::error_code ec;
        return std::filesystem::remove(path.CStr(), ec);
    }

    wax::StringView CasStore::RootDir() const noexcept
    {
        return m_rootDir.View();
    }

    void CasStore::BuildBlobPath(ContentHash hash, wax::String& out) const
    {
        auto hex = hash.ToString();
        // root/ab/cd/abcdef0123456789...
        out.Append(m_rootDir.View().Data(), m_rootDir.View().Size());
        out.Append('/');
        out.Append(hex.CStr(), 2); // first 2 hex chars
        out.Append('/');
        out.Append(hex.CStr() + 2, 2); // next 2 hex chars
        out.Append('/');
        out.Append(hex.CStr(), hex.Size()); // full hash
    }

    void CasStore::EnsureDirectoryExists(wax::StringView dirPath) const
    {
        std::error_code ec;
        wax::String dir{dirPath};
        std::filesystem::create_directories(dir.CStr(), ec);
    }
} // namespace nectar
