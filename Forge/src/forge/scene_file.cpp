#include <forge/scene_file.h>

#include <queen/world/world.h>
#include <queen/reflect/world_serializer.h>
#include <queen/reflect/world_deserializer.h>

#include <hive/core/log.h>

#include <cstdio>
#include <vector>

static const hive::LogCategory LogForge{"Forge"};

namespace forge
{
    bool SaveScene(queen::World& world, const queen::ComponentRegistry<256>& registry,
                   const char* path)
    {
        queen::WorldSerializer<1024 * 1024> serializer;
        auto result = serializer.Serialize(world, registry);
        if (!result.success)
        {
            hive::LogError(LogForge, "Failed to serialize scene");
            return false;
        }

        FILE* f = nullptr;
#ifdef _MSC_VER
        fopen_s(&f, path, "w");
#else
        f = fopen(path, "w");
#endif
        if (!f)
        {
            hive::LogError(LogForge, "Failed to open file for writing: {}", path);
            return false;
        }

        fwrite(serializer.CStr(), 1, serializer.Size(), f);
        fclose(f);

        hive::LogInfo(LogForge, "Scene saved: {} ({} entities, {} components)",
                      path, result.entities_written, result.components_written);
        return true;
    }

    bool LoadScene(queen::World& world, const queen::ComponentRegistry<256>& registry,
                   const char* path)
    {
        FILE* f = nullptr;
#ifdef _MSC_VER
        fopen_s(&f, path, "r");
#else
        f = fopen(path, "r");
#endif
        if (!f)
        {
            hive::LogError(LogForge, "Failed to open scene file: {}", path);
            return false;
        }

        fseek(f, 0, SEEK_END);
        long size = ftell(f);
        fseek(f, 0, SEEK_SET);

        std::vector<char> buffer(static_cast<size_t>(size) + 1, '\0');
        fread(buffer.data(), 1, static_cast<size_t>(size), f);
        fclose(f);

        auto result = queen::WorldDeserializer::Deserialize(world, registry, buffer.data());
        if (!result.success)
        {
            hive::LogError(LogForge, "Failed to deserialize scene: {}",
                           result.error ? result.error : "unknown");
            return false;
        }

        hive::LogInfo(LogForge, "Scene loaded: {} ({} entities, {} components, {} skipped)",
                      path, result.entities_loaded, result.components_loaded,
                      result.components_skipped);
        return true;
    }
}
