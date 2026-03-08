#pragma once

#include <queen/hierarchy/hierarchy.h>
#include <queen/reflect/component_registry.h>
#include <queen/reflect/json_deserializer.h>
#include <queen/world/world.h>

#include <cstddef>
#include <cstdint>
#include <cstring>

namespace queen
{
    /**
     * Result of a world deserialization operation
     */
    struct WorldDeserializeResult
    {
        bool m_success = false;
        size_t m_entitiesLoaded = 0;
        size_t m_componentsLoaded = 0;
        size_t m_componentsSkipped = 0;
        const char* m_error = nullptr;
    };

    /**
     * Deserializes a World from JSON using reflection and a ComponentRegistry
     *
     * - Additive loading: existing entities are preserved
     * - Entity remapping: serialized IDs are mapped to newly spawned live entities
     * - Entity references in components are remapped automatically
     * - Hierarchy (Parent) is reconstructed via World::SetParent
     * - Unknown component types are skipped (forward-compatible)
     *
     * Limitations:
     * - Maximum 4096 entities per load
     * - Maximum component size: 512 bytes
     * - No Unicode escape sequences
     */
    class WorldDeserializer
    {
        // --- Private types (declared first so Deserialize can use them) ---

        struct Parser
        {
            const char* m_data;
            size_t m_pos = 0;

            explicit Parser(const char* json) noexcept
                : m_data{json} {}

            [[nodiscard]] bool HasMore() const noexcept { return m_data[m_pos] != '\0'; }
            [[nodiscard]] char Peek() const noexcept { return m_data[m_pos]; }
            void Advance() noexcept {
                if (m_data[m_pos])
                    ++m_pos;
            }

            void SkipWhitespace() noexcept {
                while (m_data[m_pos] == ' ' || m_data[m_pos] == '\t' || m_data[m_pos] == '\n' || m_data[m_pos] == '\r')
                {
                    ++m_pos;
                }
            }

            bool Expect(char c) noexcept {
                SkipWhitespace();
                if (m_data[m_pos] == c)
                {
                    ++m_pos;
                    return true;
                }
                return false;
            }

            bool ReadString(char* out, size_t outSize) noexcept {
                if (m_data[m_pos] != '"')
                    return false;
                ++m_pos;

                size_t written = 0;
                while (m_data[m_pos] && m_data[m_pos] != '"')
                {
                    char c = m_data[m_pos++];
                    if (c == '\\' && m_data[m_pos])
                    {
                        char esc = m_data[m_pos++];
                        switch (esc)
                        {
                            case '"':
                                c = '"';
                                break;
                            case '\\':
                                c = '\\';
                                break;
                            case 'n':
                                c = '\n';
                                break;
                            case 't':
                                c = '\t';
                                break;
                            case '/':
                                c = '/';
                                break;
                            default:
                                c = esc;
                                break;
                        }
                    }
                    if (written < outSize - 1)
                    {
                        out[written++] = c;
                    }
                }
                out[written] = '\0';
                if (m_data[m_pos] == '"')
                {
                    ++m_pos;
                    return true;
                }
                return false;
            }

            bool ReadNumber(double& out) noexcept {
                const char* start = m_data + m_pos;
                char* end = nullptr;
                out = std::strtod(start, &end);
                if (end == start)
                    return false;
                m_pos = static_cast<size_t>(end - m_data);
                return true;
            }

            bool SkipValue() noexcept {
                SkipWhitespace();
                char c = Peek();
                if (c == '"')
                {
                    char tmp[256];
                    return ReadString(tmp, sizeof(tmp));
                }
                if (c == '{')
                    return SkipObject();
                if (c == '[')
                    return SkipArray();
                if (c == 't' || c == 'f')
                {
                    bool tmp;
                    return ReadBool(tmp);
                }
                if (c == 'n' && std::strncmp(m_data + m_pos, "null", 4) == 0)
                {
                    m_pos += 4;
                    return true;
                }
                double tmp;
                return ReadNumber(tmp);
            }

            bool ReadBool(bool& out) noexcept {
                if (std::strncmp(m_data + m_pos, "true", 4) == 0)
                {
                    out = true;
                    m_pos += 4;
                    return true;
                }
                if (std::strncmp(m_data + m_pos, "false", 5) == 0)
                {
                    out = false;
                    m_pos += 5;
                    return true;
                }
                return false;
            }

            bool SkipObject() noexcept {
                if (m_data[m_pos] != '{')
                    return false;
                ++m_pos;
                int depth = 1;
                while (m_data[m_pos] && depth > 0)
                {
                    if (m_data[m_pos] == '{')
                        ++depth;
                    else if (m_data[m_pos] == '}')
                        --depth;
                    else if (m_data[m_pos] == '"')
                    {
                        ++m_pos;
                        while (m_data[m_pos] && m_data[m_pos] != '"')
                        {
                            if (m_data[m_pos] == '\\')
                                ++m_pos;
                            if (m_data[m_pos])
                                ++m_pos;
                        }
                    }
                    if (m_data[m_pos])
                        ++m_pos;
                }
                return depth == 0;
            }

            bool SkipArray() noexcept {
                if (m_data[m_pos] != '[')
                    return false;
                ++m_pos;
                int depth = 1;
                while (m_data[m_pos] && depth > 0)
                {
                    if (m_data[m_pos] == '[')
                        ++depth;
                    else if (m_data[m_pos] == ']')
                        --depth;
                    else if (m_data[m_pos] == '"')
                    {
                        ++m_pos;
                        while (m_data[m_pos] && m_data[m_pos] != '"')
                        {
                            if (m_data[m_pos] == '\\')
                                ++m_pos;
                            if (m_data[m_pos])
                                ++m_pos;
                        }
                    }
                    if (m_data[m_pos])
                        ++m_pos;
                }
                return depth == 0;
            }
        };

        struct RemapEntry
        {
            uint64_t m_serializedId = 0;
            Entity m_liveEntity;
        };

        struct ParentLink
        {
            size_t m_entityIndex = 0;
            uint64_t m_parentId = 0;
        };

        static Entity FindRemapped(const RemapEntry* table, size_t count, uint64_t serializedId) noexcept {
            for (size_t i = 0; i < count; ++i)
            {
                if (table[i].m_serializedId == serializedId)
                {
                    return table[i].m_liveEntity;
                }
            }
            return Entity::Invalid();
        }

        static void RemapFieldsRecursive(void* base, const FieldInfo* fields, size_t fieldCount,
                                         const RemapEntry* remapTable, size_t remapCount) noexcept {
            for (size_t i = 0; i < fieldCount; ++i)
            {
                const FieldInfo& field = fields[i];
                auto* fieldPtr = static_cast<std::byte*>(base) + field.m_offset;

                if (field.m_type == FieldType::ENTITY)
                {
                    auto* entityRef = reinterpret_cast<Entity*>(fieldPtr);
                    if (!entityRef->IsNull())
                    {
                        Entity remapped = FindRemapped(remapTable, remapCount, entityRef->ToU64());
                        if (!remapped.IsNull())
                        {
                            *entityRef = remapped;
                        }
                    }
                }
                else if (field.m_type == FieldType::STRUCT && field.m_nestedFields != nullptr)
                {
                    RemapFieldsRecursive(fieldPtr, field.m_nestedFields, field.m_nestedFieldCount, remapTable,
                                         remapCount);
                }
                else if (field.m_type == FieldType::FIXED_ARRAY && field.m_elementType == FieldType::ENTITY)
                {
                    size_t elemSize = (field.m_elementCount > 0) ? (field.m_size / field.m_elementCount) : 0;
                    for (size_t j = 0; j < field.m_elementCount; ++j)
                    {
                        auto* elem = reinterpret_cast<Entity*>(fieldPtr + j * elemSize);
                        if (!elem->IsNull())
                        {
                            Entity remapped = FindRemapped(remapTable, remapCount, elem->ToU64());
                            if (!remapped.IsNull())
                            {
                                *elem = remapped;
                            }
                        }
                    }
                }
            }
        }

        template <size_t MaxComponents>
        static void RemapEntityFields(World& world, const ComponentRegistry<MaxComponents>& registry,
                                      const RemapEntry* remapTable, size_t remapCount) noexcept {
            for (size_t e = 0; e < remapCount; ++e)
            {
                Entity live = remapTable[e].m_liveEntity;

                for (size_t r = 0; r < registry.Count(); ++r)
                {
                    const auto& reg = registry[r];
                    if (!reg.HasReflection())
                        continue;

                    void* compData = world.GetComponentRaw(live, reg.m_meta.m_typeId);
                    if (compData == nullptr)
                        continue;

                    RemapFieldsRecursive(compData, reg.m_reflection.m_fields, reg.m_reflection.m_fieldCount, remapTable,
                                         remapCount);
                }
            }
        }

        static WorldDeserializeResult Fail(WorldDeserializeResult& result, const char* error) noexcept {
            result.m_error = error;
            return result;
        }

    public:
        static constexpr size_t kMaxEntities = 4096;
        static constexpr size_t kMaxComponentSize = 512;

        template <size_t MaxComponents>
        static WorldDeserializeResult Deserialize(World& world, const ComponentRegistry<MaxComponents>& registry,
                                                  const char* json) noexcept {
            WorldDeserializeResult result{};
            Parser p{json};

            RemapEntry remapTable[kMaxEntities]{};
            size_t remapCount = 0;

            ParentLink parentLinks[kMaxEntities]{};
            size_t parentCount = 0;

            // Parse: {"version":1,"entities":[...]}
            p.SkipWhitespace();
            if (!p.Expect('{'))
                return Fail(result, "Expected '{'");

            p.SkipWhitespace();
            char key[64]{};
            if (!p.ReadString(key, sizeof(key)))
                return Fail(result, "Expected key");
            p.SkipWhitespace();
            if (!p.Expect(':'))
                return Fail(result, "Expected ':'");
            double version;
            if (!p.ReadNumber(version))
                return Fail(result, "Expected version number");

            p.SkipWhitespace();
            if (!p.Expect(','))
                return Fail(result, "Expected ','");

            p.SkipWhitespace();
            if (!p.ReadString(key, sizeof(key)))
                return Fail(result, "Expected 'entities' key");
            p.SkipWhitespace();
            if (!p.Expect(':'))
                return Fail(result, "Expected ':'");

            p.SkipWhitespace();
            if (!p.Expect('['))
                return Fail(result, "Expected '['");

            p.SkipWhitespace();
            if (p.Peek() == ']')
            {
                p.Advance();
                p.SkipWhitespace();
                if (p.Peek() == '}')
                    p.Advance();
                result.m_success = true;
                return result;
            }

            while (p.HasMore())
            {
                if (remapCount >= kMaxEntities)
                    return Fail(result, "Too many entities");

                p.SkipWhitespace();
                if (!p.Expect('{'))
                    return Fail(result, "Expected entity '{'");

                uint64_t serializedId = 0;
                uint64_t parentId = 0;
                bool hasParent = false;

                auto builder = world.Spawn();

                while (p.HasMore())
                {
                    p.SkipWhitespace();
                    char fieldName[64]{};
                    if (!p.ReadString(fieldName, sizeof(fieldName)))
                        return Fail(result, "Expected field name");

                    p.SkipWhitespace();
                    if (!p.Expect(':'))
                        return Fail(result, "Expected ':'");
                    p.SkipWhitespace();

                    if (detail::StringsEqual(fieldName, "id"))
                    {
                        double num;
                        if (!p.ReadNumber(num))
                            return Fail(result, "Expected entity id");
                        serializedId = static_cast<uint64_t>(num);
                    }
                    else if (detail::StringsEqual(fieldName, "parent"))
                    {
                        double num;
                        if (!p.ReadNumber(num))
                            return Fail(result, "Expected parent id");
                        parentId = static_cast<uint64_t>(num);
                        hasParent = true;
                    }
                    else if (detail::StringsEqual(fieldName, "components"))
                    {
                        if (!p.Expect('{'))
                            return Fail(result, "Expected components '{'");

                        p.SkipWhitespace();
                        if (p.Peek() != '}')
                        {
                            while (p.HasMore())
                            {
                                p.SkipWhitespace();
                                char compName[64]{};
                                if (!p.ReadString(compName, sizeof(compName)))
                                    return Fail(result, "Expected component name");

                                p.SkipWhitespace();
                                if (!p.Expect(':'))
                                    return Fail(result, "Expected ':'");
                                p.SkipWhitespace();

                                const RegisteredComponent* reg = registry.FindByName(compName);
                                if (reg != nullptr && reg->HasReflection())
                                {
                                    if (reg->m_meta.m_size > kMaxComponentSize)
                                        return Fail(result, "Component too large");

                                    alignas(16) std::byte buffer[kMaxComponentSize]{};
                                    if (reg->m_meta.m_construct != nullptr)
                                    {
                                        reg->m_meta.m_construct(buffer);
                                    }

                                    auto compResult = JsonDeserializer::DeserializeComponent(buffer, reg->m_reflection,
                                                                                             p.m_data + p.m_pos);

                                    if (!compResult.m_success)
                                    {
                                        if (reg->m_meta.m_destruct != nullptr)
                                            reg->m_meta.m_destruct(buffer);
                                        return Fail(result, "Failed to deserialize component");
                                    }

                                    // Advance parser past the component JSON object
                                    p.SkipValue();

                                    builder.WithRaw(reg->m_meta, buffer);

                                    if (reg->m_meta.m_destruct != nullptr)
                                    {
                                        reg->m_meta.m_destruct(buffer);
                                    }

                                    ++result.m_componentsLoaded;
                                }
                                else
                                {
                                    p.SkipValue();
                                    ++result.m_componentsSkipped;
                                }

                                p.SkipWhitespace();
                                if (p.Peek() == ',')
                                {
                                    p.Advance();
                                }
                                else if (p.Peek() == '}')
                                {
                                    p.Advance();
                                    break;
                                }
                                else
                                    return Fail(result, "Expected ',' or '}' in components");
                            }
                        }
                        else
                        {
                            p.Advance();
                        }
                    }
                    else
                    {
                        p.SkipValue();
                    }

                    p.SkipWhitespace();
                    if (p.Peek() == ',')
                    {
                        p.Advance();
                    }
                    else if (p.Peek() == '}')
                    {
                        p.Advance();
                        break;
                    }
                    else
                        return Fail(result, "Expected ',' or '}' in entity");
                }

                Entity live = builder.Build();

                size_t entityIdx = remapCount;
                remapTable[remapCount] = {serializedId, live};
                ++remapCount;

                if (hasParent)
                {
                    parentLinks[parentCount++] = {entityIdx, parentId};
                }

                ++result.m_entitiesLoaded;

                p.SkipWhitespace();
                if (p.Peek() == ',')
                {
                    p.Advance();
                }
                else if (p.Peek() == ']')
                {
                    p.Advance();
                    break;
                }
                else
                    return Fail(result, "Expected ',' or ']' in entities array");
            }

            // --- Post-processing: Entity field remapping ---
            RemapEntityFields(world, registry, remapTable, remapCount);

            // --- Post-processing: Rebuild hierarchy ---
            for (size_t i = 0; i < parentCount; ++i)
            {
                Entity child = remapTable[parentLinks[i].m_entityIndex].m_liveEntity;
                Entity parent = FindRemapped(remapTable, remapCount, parentLinks[i].m_parentId);
                if (!parent.IsNull() && world.IsAlive(parent) && world.IsAlive(child))
                {
                    world.SetParent(child, parent);
                }
            }

            p.SkipWhitespace();
            if (p.Peek() == '}')
                p.Advance();

            result.m_success = true;
            return result;
        }
    };
} // namespace queen
