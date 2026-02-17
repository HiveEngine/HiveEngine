#include <nectar/pak/asset_manifest.h>
#include <cstring>

namespace nectar
{
    AssetManifest::AssetManifest(comb::DefaultAllocator& alloc)
        : alloc_{&alloc}
        , entries_{alloc, 64}
    {}

    void AssetManifest::Add(wax::StringView vfs_path, ContentHash hash)
    {
        wax::String<> key{*alloc_};
        key.Append(vfs_path.Data(), vfs_path.Size());

        auto* existing = entries_.Find(key);
        if (existing)
            *existing = hash;
        else
            entries_.Insert(static_cast<wax::String<>&&>(key), hash);
    }

    const ContentHash* AssetManifest::Find(wax::StringView vfs_path) const
    {
        wax::String<> key{*alloc_};
        key.Append(vfs_path.Data(), vfs_path.Size());
        return entries_.Find(key);
    }

    size_t AssetManifest::Count() const noexcept
    {
        return entries_.Count();
    }

    wax::ByteBuffer<> AssetManifest::Serialize(comb::DefaultAllocator& alloc) const
    {
        // Format: [count u32] [entry...]
        // Entry: [path_len u32] [path bytes] [hash_high u64] [hash_low u64]
        wax::ByteBuffer<> buf{alloc};

        uint32_t count = static_cast<uint32_t>(entries_.Count());
        buf.Resize(sizeof(uint32_t));
        std::memcpy(buf.Data(), &count, sizeof(uint32_t));

        for (auto it = entries_.begin(); it != entries_.end(); ++it)
        {
            const auto& path = it.Key();
            const auto& hash = it.Value();

            size_t old_size = buf.Size();
            uint32_t path_len = static_cast<uint32_t>(path.Size());
            size_t entry_size = sizeof(uint32_t) + path_len + sizeof(uint64_t) * 2;
            buf.Resize(old_size + entry_size);

            uint8_t* dst = buf.Data() + old_size;
            std::memcpy(dst, &path_len, sizeof(uint32_t));
            dst += sizeof(uint32_t);
            std::memcpy(dst, path.CStr(), path_len);
            dst += path_len;

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
            uint32_t path_len = 0;
            std::memcpy(&path_len, ptr, sizeof(uint32_t));
            ptr += sizeof(uint32_t);

            if (ptr + path_len + sizeof(uint64_t) * 2 > end)
                break;

            wax::StringView path{reinterpret_cast<const char*>(ptr), path_len};
            ptr += path_len;

            uint64_t hi = 0, lo = 0;
            std::memcpy(&hi, ptr, sizeof(uint64_t));
            ptr += sizeof(uint64_t);
            std::memcpy(&lo, ptr, sizeof(uint64_t));
            ptr += sizeof(uint64_t);

            manifest.Add(path, ContentHash{hi, lo});
        }

        return manifest;
    }
}
