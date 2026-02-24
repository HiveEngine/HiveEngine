#pragma once

#include <nectar/database/asset_database.h>
#include <nectar/core/content_hash.h>
#include <nectar/core/asset_id.h>
#include <wax/serialization/binary_writer.h>
#include <wax/serialization/binary_reader.h>
#include <wax/serialization/byte_buffer.h>
#include <comb/default_allocator.h>
#include <cstring>
#include <fstream>
#include <filesystem>

namespace nectar
{

inline constexpr uint32_t kImportCacheMagic = 0x4244494E; // "NIDB"
inline constexpr uint16_t kImportCacheVersion = 1;

inline AssetId AssetIdFromPath(const char* path)
{
    auto hash = ContentHash::FromData(path, std::strlen(path));
    return AssetId{hash.High(), hash.Low()};
}

inline bool LoadImportCache(const char* path, AssetDatabase& db,
                            comb::DefaultAllocator& alloc)
{
    std::ifstream file{path, std::ios::binary | std::ios::ate};
    if (!file) return false;

    auto file_size = file.tellg();
    if (file_size < 12) return false;

    file.seekg(0);
    wax::ByteBuffer<> buf{alloc, static_cast<size_t>(file_size)};
    buf.Resize(static_cast<size_t>(file_size));
    file.read(reinterpret_cast<char*>(buf.Data()), file_size);
    if (!file) return false;

    wax::BinaryReader reader{buf.View()};

    uint32_t magic{};
    if (!reader.TryRead(magic) || magic != kImportCacheMagic) return false;

    uint16_t version{};
    if (!reader.TryRead(version) || version != kImportCacheVersion) return false;

    reader.Skip(2);

    uint32_t count = reader.Read<uint32_t>();

    for (uint32_t i = 0; i < count; ++i)
    {
        if (reader.Remaining() < 16) break;

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
        record.uuid = AssetId{id_high, id_low};
        record.path = wax::String<>{alloc};
        record.path.Append(reinterpret_cast<const char*>(path_span.Data()), path_span.Size());
        record.type = wax::String<>{alloc};
        record.type.Append(reinterpret_cast<const char*>(type_span.Data()), type_span.Size());
        record.name = wax::String<>{alloc};
        record.name.Append(reinterpret_cast<const char*>(name_span.Data()), name_span.Size());
        record.content_hash = ContentHash{ch_high, ch_low};
        record.intermediate_hash = ContentHash{ih_high, ih_low};
        record.import_version = imp_version;
        record.labels = wax::Vector<wax::String<>>{alloc};

        db.Insert(static_cast<AssetRecord&&>(record));
    }

    return true;
}

inline bool SaveImportCache(const char* path, const AssetDatabase& db,
                            comb::DefaultAllocator& alloc)
{
    std::error_code ec;
    std::filesystem::create_directories(
        std::filesystem::path{path}.parent_path(), ec);

    wax::BinaryWriter<comb::DefaultAllocator> writer{alloc, 4096};

    writer.Write<uint32_t>(kImportCacheMagic);
    writer.Write<uint16_t>(kImportCacheVersion);
    writer.Write<uint16_t>(0);
    writer.Write<uint32_t>(static_cast<uint32_t>(db.Count()));

    db.ForEach([&](AssetId id, const AssetRecord& r) {
        writer.Write<uint64_t>(id.High());
        writer.Write<uint64_t>(id.Low());
        writer.WriteString(r.path.CStr(), r.path.Size());
        writer.WriteString(r.type.CStr(), r.type.Size());
        writer.WriteString(r.name.CStr(), r.name.Size());
        writer.Write<uint64_t>(r.content_hash.High());
        writer.Write<uint64_t>(r.content_hash.Low());
        writer.Write<uint64_t>(r.intermediate_hash.High());
        writer.Write<uint64_t>(r.intermediate_hash.Low());
        writer.Write<uint32_t>(r.import_version);
        writer.Write<uint32_t>(0);
    });

    auto span = writer.View();
    std::ofstream file{path, std::ios::binary};
    if (!file) return false;

    file.write(reinterpret_cast<const char*>(span.Data()),
               static_cast<std::streamsize>(span.Size()));
    return file.good();
}

} // namespace nectar
