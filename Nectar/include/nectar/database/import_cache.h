#pragma once

#include <comb/default_allocator.h>

#include <wax/serialization/binary_reader.h>
#include <wax/serialization/binary_writer.h>
#include <wax/serialization/byte_buffer.h>

#include <nectar/core/asset_id.h>
#include <nectar/core/content_hash.h>
#include <nectar/database/asset_database.h>

#include <cstring>
#include <filesystem>
#include <fstream>

namespace nectar
{

    inline constexpr uint32_t kImportCacheMagic = 0x4244494E; // "NIDB"
    inline constexpr uint16_t kImportCacheVersion = 1;

    inline AssetId AssetIdFromPath(const char* path)
    {
        auto hash = ContentHash::FromData(path, std::strlen(path));
        return AssetId{hash.High(), hash.Low()};
    }

    inline bool LoadImportCache(const char* path, AssetDatabase& db, comb::DefaultAllocator& alloc)
    {
        std::ifstream file{path, std::ios::binary | std::ios::ate};
        if (!file)
            return false;

        auto file_size = file.tellg();
        if (file_size < 12)
            return false;

        file.seekg(0);
        wax::ByteBuffer buf{alloc, static_cast<size_t>(file_size)};
        buf.Resize(static_cast<size_t>(file_size));
        file.read(reinterpret_cast<char*>(buf.Data()), file_size);
        if (!file)
            return false;

        wax::BinaryReader reader{buf.View()};

        uint32_t magic{};
        if (!reader.TryRead(magic) || magic != kImportCacheMagic)
            return false;

        uint16_t version{};
        if (!reader.TryRead(version) || version != kImportCacheVersion)
            return false;

        reader.Skip(2);

        uint32_t count = reader.Read<uint32_t>();

        for (uint32_t i = 0; i < count; ++i)
        {
            if (reader.Remaining() < 16)
                break;

            uint64_t id_high = reader.Read<uint64_t>();
            uint64_t id_low = reader.Read<uint64_t>();

            auto path_span = reader.ReadString();
            auto type_span = reader.ReadString();
            auto name_span = reader.ReadString();

            uint64_t ch_high = reader.Read<uint64_t>();
            uint64_t ch_low = reader.Read<uint64_t>();
            uint64_t ih_high = reader.Read<uint64_t>();
            uint64_t ih_low = reader.Read<uint64_t>();

            uint32_t imp_version = reader.Read<uint32_t>();

            uint32_t label_count = reader.Read<uint32_t>();
            for (uint32_t j = 0; j < label_count; ++j)
                (void)reader.ReadString();

            AssetRecord record{};
            record.m_uuid = AssetId{id_high, id_low};
            record.m_path = wax::String{alloc};
            record.m_path.Append(reinterpret_cast<const char*>(path_span.Data()), path_span.Size());
            record.m_type = wax::String{alloc};
            record.m_type.Append(reinterpret_cast<const char*>(type_span.Data()), type_span.Size());
            record.m_name = wax::String{alloc};
            record.m_name.Append(reinterpret_cast<const char*>(name_span.Data()), name_span.Size());
            record.m_contentHash = ContentHash{ch_high, ch_low};
            record.m_intermediateHash = ContentHash{ih_high, ih_low};
            record.m_importVersion = imp_version;
            record.m_labels = wax::Vector<wax::String>{alloc};

            db.Insert(static_cast<AssetRecord&&>(record));
        }

        return true;
    }

    inline bool SaveImportCache(const char* path, const AssetDatabase& db, comb::DefaultAllocator& alloc)
    {
        std::error_code ec;
        std::filesystem::create_directories(std::filesystem::path{path}.parent_path(), ec);

        wax::BinaryWriter writer{alloc, 4096};

        writer.Write<uint32_t>(kImportCacheMagic);
        writer.Write<uint16_t>(kImportCacheVersion);
        writer.Write<uint16_t>(0);
        writer.Write<uint32_t>(static_cast<uint32_t>(db.Count()));

        db.ForEach([&](AssetId id, const AssetRecord& r) {
            writer.Write<uint64_t>(id.High());
            writer.Write<uint64_t>(id.Low());
            writer.WriteString(r.m_path.CStr(), r.m_path.Size());
            writer.WriteString(r.m_type.CStr(), r.m_type.Size());
            writer.WriteString(r.m_name.CStr(), r.m_name.Size());
            writer.Write<uint64_t>(r.m_contentHash.High());
            writer.Write<uint64_t>(r.m_contentHash.Low());
            writer.Write<uint64_t>(r.m_intermediateHash.High());
            writer.Write<uint64_t>(r.m_intermediateHash.Low());
            writer.Write<uint32_t>(r.m_importVersion);
            writer.Write<uint32_t>(0);
        });

        auto span = writer.View();
        std::ofstream file{path, std::ios::binary};
        if (!file)
            return false;

        file.write(reinterpret_cast<const char*>(span.Data()), static_cast<std::streamsize>(span.Size()));
        return file.good();
    }

} // namespace nectar
