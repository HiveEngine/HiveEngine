#pragma once

#include <queen/reflect/json_serializer.h>
#include <queen/reflect/component_registry.h>
#include <queen/world/world.h>
#include <queen/hierarchy/hierarchy.h>
#include <cstddef>
#include <cstdint>
#include <cstdio>

namespace queen
{
    /**
     * Result of a world serialization operation
     */
    struct WorldSerializeResult
    {
        bool success = false;
        size_t entities_written = 0;
        size_t components_written = 0;
    };

    /**
     * Serializes a World to JSON using reflection and a ComponentRegistry
     *
     * Format:
     * {
     *   "version": 1,
     *   "entities": [
     *     {
     *       "id": <uint64>,
     *       "parent": <uint64>,          // only if entity has a parent
     *       "components": {
     *         "TypeName": { ... },
     *         ...
     *       }
     *     }
     *   ]
     * }
     *
     * Parent/Children hierarchy components are NOT serialized as regular
     * components. Instead, the parent relationship is stored as a separate
     * "parent" field on each entity and reconstructed via SetParent on load.
     *
     * Components not registered in the registry are silently skipped.
     *
     * @tparam BufSize Maximum output buffer size in bytes
     */
    template<size_t BufSize = 65536>
    class WorldSerializer
    {
    public:
        WorldSerializer() noexcept = default;

        template<size_t MaxComponents>
        WorldSerializeResult Serialize(World& world,
                                       const ComponentRegistry<MaxComponents>& registry) noexcept
        {
            WorldSerializeResult result{};
            pos_ = 0;

            WriteRaw("{\"version\":1,\"entities\":[");

            bool first_entity = true;

            world.ForEachArchetype([&](Archetype<ComponentAllocator>& archetype) {
                const auto& types = archetype.GetComponentTypes();
                const auto& metas = archetype.GetComponentMetas();

                for (uint32_t row = 0; row < archetype.EntityCount(); ++row)
                {
                    Entity entity = archetype.GetEntity(row);

                    if (!first_entity) Put(',');
                    first_entity = false;

                    WriteRaw("{\"id\":");
                    WriteUint64(entity.ToU64());

                    // Parent (special handling — not a regular component)
                    Entity parent = world.GetParent(entity);
                    if (!parent.IsNull())
                    {
                        WriteRaw(",\"parent\":");
                        WriteUint64(parent.ToU64());
                    }

                    WriteRaw(",\"components\":{");

                    bool first_comp = true;
                    for (size_t c = 0; c < types.Size(); ++c)
                    {
                        TypeId type_id = types[c];

                        // Skip hierarchy components
                        if (type_id == TypeIdOf<Parent>() || type_id == TypeIdOf<Children>())
                            continue;

                        const RegisteredComponent* reg = registry.Find(type_id);
                        if (reg == nullptr || !reg->HasReflection())
                            continue;

                        if (!first_comp) Put(',');
                        first_comp = false;

                        Put('"');
                        WriteRaw(reg->reflection.name);
                        Put('"');
                        Put(':');

                        const void* data = archetype.GetComponentRaw(row, type_id);
                        JsonSerializer<4096> json;
                        json.SerializeComponent(data, reg->reflection);
                        WriteRaw(json.CStr());

                        ++result.components_written;
                    }

                    WriteRaw("}}");
                    ++result.entities_written;
                }
            });

            WriteRaw("]}");
            Terminate();
            result.success = true;
            return result;
        }

        [[nodiscard]] const char* CStr() const noexcept { return buf_; }
        [[nodiscard]] size_t Size() const noexcept { return pos_; }

    private:
        void Put(char c) noexcept
        {
            if (pos_ < BufSize - 1) buf_[pos_++] = c;
        }

        void WriteRaw(const char* s) noexcept
        {
            if (s == nullptr) return;
            while (*s && pos_ < BufSize - 1)
            {
                buf_[pos_++] = *s++;
            }
        }

        void WriteUint64(uint64_t v) noexcept
        {
            char tmp[24];
            int n = std::snprintf(tmp, sizeof(tmp), "%llu", static_cast<unsigned long long>(v));
            for (int i = 0; i < n; ++i) Put(tmp[i]);
        }

        void Terminate() noexcept
        {
            buf_[pos_ < BufSize ? pos_ : BufSize - 1] = '\0';
        }

        char buf_[BufSize]{};
        size_t pos_ = 0;
    };
}
