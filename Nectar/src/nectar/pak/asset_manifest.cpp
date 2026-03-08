#include <nectar/pak/asset_manifest.h>

#include <cstring>

namespace nectar
{
    AssetManifest::AssetManifest(comb::DefaultAllocator& alloc)
        : m_alloc{&alloc}
        , m_entries{alloc, 64}
    {
    }

    void AssetManifest::Add(wax::StringView vfsPath, ContentHash hash)
    {
        wax::String key{*m_alloc};
        key.Append(vfsPath.Data(), vfsPath.Size());

        auto* existing = m_entries.Find(key);
        if (existing)
            *existing = hash;
        else
            m_entries.Insert(static_cast<wax::String&&>(key), hash);
    }

    const ContentHash* AssetManifest::Find(wax::StringView vfsPath) const
    {
        wax::String key{*m_alloc};
        key.Append(vfsPath.Data(), vfsPath.Size());
        return m_entries.Find(key);
    }

    size_t AssetManifest::Count() const noexcept
    {
        return m_entries.Count();
    }

    wax::ByteBuffer AssetManifest::Serialize(comb::DefaultAllocator& alloc) const
    {
        // Format: [count u32] [entry...]
        // Entry: [path_len u32] [path bytes] [hash_high u64] [hash_low u64]
        wax::ByteBuffer buf{alloc};

        uint32_t count = static_cast<uint32_t>(m_entries.Count());
        buf.Resize(sizeof(uint32_t));
        std::memcpy(buf.Data(), &count, sizeof(uint32_t));

        for (auto it = m_entries.Begin(); it != m_entries.End(); ++it)
        {
            const auto& path = it.Key();
            const auto& hash = it.Value();

            size_t oldSize = buf.Size();
            uint32_t pathLen = static_cast<uint32_t>(path.Size());
            size_t entrySize = sizeof(uint32_t) + pathLen + sizeof(uint64_t) * 2;
            buf.Resize(oldSize + entrySize);

            uint8_t* dst = buf.Data() + oldSize;
            std::memcpy(dst, &pathLen, sizeof(uint32_t));
            dst += sizeof(uint32_t);
            std::memcpy(dst, path.CStr(), pathLen);
            dst += pathLen;

            uint64_t hi = hash.High();
            uint64_t lo = hash.Low();
            std::memcpy(dst, &hi, sizeof(uint64_t));
            dst += sizeof(uint64_t);
            std::memcpy(dst, &lo, sizeof(uint64_t));
        }

        return buf;
    }

    AssetManifest AssetManifest::Deserialize(wax::ByteSpan data, comb::DefaultAllocator& alloc)
    {
        AssetManifest manifest{alloc};

        if (data.Size() < sizeof(uint32_t))
            return manifest;

        const uint8_t* ptr = data.Data();
        const uint8_t* end = ptr + data.Size();

        uint32_t count = 0;
        std::memcpy(&count, ptr, sizeof(uint32_t));
        ptr += sizeof(uint32_t);

        for (uint32_t i = 0; i < count && ptr + sizeof(uint32_t) <= end; ++i)
        {
            uint32_t pathLen = 0;
            std::memcpy(&pathLen, ptr, sizeof(uint32_t));
            ptr += sizeof(uint32_t);

            if (ptr + pathLen + sizeof(uint64_t) * 2 > end)
                break;

            wax::StringView path{reinterpret_cast<const char*>(ptr), pathLen};
            ptr += pathLen;

            uint64_t hi = 0, lo = 0;
            std::memcpy(&hi, ptr, sizeof(uint64_t));
            ptr += sizeof(uint64_t);
            std::memcpy(&lo, ptr, sizeof(uint64_t));
            ptr += sizeof(uint64_t);

            manifest.Add(path, ContentHash{hi, lo});
        }

        return manifest;
    }
} // namespace nectar
