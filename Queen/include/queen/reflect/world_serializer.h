#pragma once

#include <queen/hierarchy/hierarchy.h>
#include <queen/reflect/component_registry.h>
#include <queen/reflect/json_serializer.h>
#include <queen/world/world.h>

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
        bool m_success = false;
        size_t m_entitiesWritten = 0;
        size_t m_componentsWritten = 0;
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
    template <size_t BufSize = 65536> class WorldSerializer
    {
    public:
        WorldSerializer() noexcept = default;

        template <size_t MaxComponents>
        WorldSerializeResult Serialize(World& world, const ComponentRegistry<MaxComponents>& registry) noexcept {
            WorldSerializeResult result{};
            m_pos = 0;

            WriteRaw("{\"version\":1,\"entities\":[");

            bool firstEntity = true;

            world.ForEachArchetype([&](Archetype<ComponentAllocator>& archetype) {
                const auto& types = archetype.GetComponentTypes();

                for (uint32_t row = 0; row < archetype.EntityCount(); ++row)
                {
                    Entity entity = archetype.GetEntity(row);

                    if (!firstEntity)
                        Put(',');
                    firstEntity = false;

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

                    bool firstComp = true;
                    for (size_t c = 0; c < types.Size(); ++c)
                    {
                        TypeId typeId = types[c];

                        // Skip hierarchy components
                        if (typeId == TypeIdOf<Parent>() || typeId == TypeIdOf<Children>())
                            continue;

                        const RegisteredComponent* reg = registry.Find(typeId);
                        if (reg == nullptr || !reg->HasReflection())
                            continue;

                        if (!firstComp)
                            Put(',');
                        firstComp = false;

                        Put('"');
                        WriteRaw(reg->m_reflection.m_name);
                        Put('"');
                        Put(':');

                        const void* data = archetype.GetComponentRaw(row, typeId);
                        JsonSerializer<4096> json;
                        json.SerializeComponent(data, reg->m_reflection);
                        WriteRaw(json.CStr());

                        ++result.m_componentsWritten;
                    }

                    WriteRaw("}}");
                    ++result.m_entitiesWritten;
                }
            });

            WriteRaw("]}");
            Terminate();
            result.m_success = true;
            return result;
        }

        [[nodiscard]] const char* CStr() const noexcept { return m_buf; }
        [[nodiscard]] size_t Size() const noexcept { return m_pos; }

    private:
        void Put(char c) noexcept {
            if (m_pos < BufSize - 1)
                m_buf[m_pos++] = c;
        }

        void WriteRaw(const char* s) noexcept {
            if (s == nullptr)
                return;
            while (*s && m_pos < BufSize - 1)
            {
                m_buf[m_pos++] = *s++;
            }
        }

        void WriteUint64(uint64_t v) noexcept {
            char tmp[24];
            int n = std::snprintf(tmp, sizeof(tmp), "%llu", static_cast<unsigned long long>(v));
            for (int i = 0; i < n; ++i)
                Put(tmp[i]);
        }

        void Terminate() noexcept { m_buf[m_pos < BufSize ? m_pos : BufSize - 1] = '\0'; }

        char m_buf[BufSize]{};
        size_t m_pos = 0;
    };
} // namespace queen
