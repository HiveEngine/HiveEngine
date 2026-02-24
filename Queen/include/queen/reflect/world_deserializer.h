#pragma once

#include <queen/reflect/json_deserializer.h>
#include <queen/reflect/component_registry.h>
#include <queen/world/world.h>
#include <queen/hierarchy/hierarchy.h>
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
        bool success = false;
        size_t entities_loaded = 0;
        size_t components_loaded = 0;
        size_t components_skipped = 0;
        const char* error = nullptr;
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
            const char* data;
            size_t pos = 0;

            explicit Parser(const char* json) noexcept : data{json} {}

            [[nodiscard]] bool HasMore() const noexcept { return data[pos] != '\0'; }
            [[nodiscard]] char Peek() const noexcept { return data[pos]; }
            void Advance() noexcept { if (data[pos]) ++pos; }

            void SkipWhitespace() noexcept
            {
                while (data[pos] == ' ' || data[pos] == '\t' ||
                       data[pos] == '\n' || data[pos] == '\r')
                {
                    ++pos;
                }
            }

            bool Expect(char c) noexcept
            {
                SkipWhitespace();
                if (data[pos] == c) { ++pos; return true; }
                return false;
            }

            bool ReadString(char* out, size_t out_size) noexcept
            {
                if (data[pos] != '"') return false;
                ++pos;

                size_t written = 0;
                while (data[pos] && data[pos] != '"')
                {
                    char c = data[pos++];
                    if (c == '\\' && data[pos])
                    {
                        char esc = data[pos++];
                        switch (esc)
                        {
                            case '"':  c = '"'; break;
                            case '\\': c = '\\'; break;
                            case 'n':  c = '\n'; break;
                            case 't':  c = '\t'; break;
                            case '/':  c = '/'; break;
                            default:   c = esc; break;
                        }
                    }
                    if (written < out_size - 1)
                    {
                        out[written++] = c;
                    }
                }
                out[written] = '\0';
                if (data[pos] == '"') { ++pos; return true; }
                return false;
            }

            bool ReadNumber(double& out) noexcept
            {
                const char* start = data + pos;
                char* end = nullptr;
                out = std::strtod(start, &end);
                if (end == start) return false;
                pos = static_cast<size_t>(end - data);
                return true;
            }

            bool SkipValue() noexcept
            {
                SkipWhitespace();
                char c = Peek();
                if (c == '"')
                {
                    char tmp[256];
                    return ReadString(tmp, sizeof(tmp));
                }
                if (c == '{') return SkipObject();
                if (c == '[') return SkipArray();
                if (c == 't' || c == 'f')
                {
                    bool tmp;
                    return ReadBool(tmp);
                }
                if (c == 'n' && std::strncmp(data + pos, "null", 4) == 0)
                {
                    pos += 4;
                    return true;
                }
                double tmp;
                return ReadNumber(tmp);
            }

            bool ReadBool(bool& out) noexcept
            {
                if (std::strncmp(data + pos, "true", 4) == 0)
                {
                    out = true; pos += 4; return true;
                }
                if (std::strncmp(data + pos, "false", 5) == 0)
                {
                    out = false; pos += 5; return true;
                }
                return false;
            }

            bool SkipObject() noexcept
            {
                if (data[pos] != '{') return false;
                ++pos;
                int depth = 1;
                while (data[pos] && depth > 0)
                {
                    if (data[pos] == '{') ++depth;
                    else if (data[pos] == '}') --depth;
                    else if (data[pos] == '"')
                    {
                        ++pos;
                        while (data[pos] && data[pos] != '"')
                        {
                            if (data[pos] == '\\') ++pos;
                            if (data[pos]) ++pos;
                        }
                    }
                    if (data[pos]) ++pos;
                }
                return depth == 0;
            }

            bool SkipArray() noexcept
            {
                if (data[pos] != '[') return false;
                ++pos;
                int depth = 1;
                while (data[pos] && depth > 0)
                {
                    if (data[pos] == '[') ++depth;
                    else if (data[pos] == ']') --depth;
                    else if (data[pos] == '"')
                    {
                        ++pos;
                        while (data[pos] && data[pos] != '"')
                        {
                            if (data[pos] == '\\') ++pos;
                            if (data[pos]) ++pos;
                        }
                    }
                    if (data[pos]) ++pos;
                }
                return depth == 0;
            }
        };

        struct RemapEntry
        {
            uint64_t serialized_id = 0;
            Entity live_entity;
        };

        struct ParentLink
        {
            size_t entity_index = 0;
            uint64_t parent_id = 0;
        };

        static Entity FindRemapped(const RemapEntry* table, size_t count, uint64_t serialized_id) noexcept
        {
            for (size_t i = 0; i < count; ++i)
            {
                if (table[i].serialized_id == serialized_id)
                {
                    return table[i].live_entity;
                }
            }
            return Entity::Invalid();
        }

        static void RemapFieldsRecursive(
            void* base,
            const FieldInfo* fields, size_t field_count,
            const RemapEntry* remap_table, size_t remap_count) noexcept
        {
            for (size_t i = 0; i < field_count; ++i)
            {
                const FieldInfo& field = fields[i];
                auto* field_ptr = static_cast<std::byte*>(base) + field.offset;

                if (field.type == FieldType::Entity)
                {
                    auto* entity_ref = reinterpret_cast<Entity*>(field_ptr);
                    if (!entity_ref->IsNull())
                    {
                        Entity remapped = FindRemapped(remap_table, remap_count, entity_ref->ToU64());
                        if (!remapped.IsNull())
                        {
                            *entity_ref = remapped;
                        }
                    }
                }
                else if (field.type == FieldType::Struct && field.nested_fields != nullptr)
                {
                    RemapFieldsRecursive(field_ptr, field.nested_fields, field.nested_field_count,
                                        remap_table, remap_count);
                }
                else if (field.type == FieldType::FixedArray && field.element_type == FieldType::Entity)
                {
                    size_t elem_size = (field.element_count > 0) ? (field.size / field.element_count) : 0;
                    for (size_t j = 0; j < field.element_count; ++j)
                    {
                        auto* elem = reinterpret_cast<Entity*>(field_ptr + j * elem_size);
                        if (!elem->IsNull())
                        {
                            Entity remapped = FindRemapped(remap_table, remap_count, elem->ToU64());
                            if (!remapped.IsNull())
                            {
                                *elem = remapped;
                            }
                        }
                    }
                }
            }
        }

        template<size_t MaxComponents>
        static void RemapEntityFields(
            World& world,
            const ComponentRegistry<MaxComponents>& registry,
            const RemapEntry* remap_table, size_t remap_count) noexcept
        {
            for (size_t e = 0; e < remap_count; ++e)
            {
                Entity live = remap_table[e].live_entity;

                for (size_t r = 0; r < registry.Count(); ++r)
                {
                    const auto& reg = registry[r];
                    if (!reg.HasReflection()) continue;

                    void* comp_data = world.GetComponentRaw(live, reg.meta.type_id);
                    if (comp_data == nullptr) continue;

                    RemapFieldsRecursive(
                        comp_data, reg.reflection.fields, reg.reflection.field_count,
                        remap_table, remap_count);
                }
            }
        }

        static WorldDeserializeResult Fail(WorldDeserializeResult& result, const char* error) noexcept
        {
            result.error = error;
            return result;
        }

    public:
        static constexpr size_t kMaxEntities = 4096;
        static constexpr size_t kMaxComponentSize = 512;

        template<size_t MaxComponents>
        static WorldDeserializeResult Deserialize(
            World& world,
            const ComponentRegistry<MaxComponents>& registry,
            const char* json) noexcept
        {
            WorldDeserializeResult result{};
            Parser p{json};

            RemapEntry remap_table[kMaxEntities]{};
            size_t remap_count = 0;

            ParentLink parent_links[kMaxEntities]{};
            size_t parent_count = 0;

            // Parse: {"version":1,"entities":[...]}
            p.SkipWhitespace();
            if (!p.Expect('{')) return Fail(result, "Expected '{'");

            p.SkipWhitespace();
            char key[64]{};
            if (!p.ReadString(key, sizeof(key))) return Fail(result, "Expected key");
            p.SkipWhitespace();
            if (!p.Expect(':')) return Fail(result, "Expected ':'");
            double version;
            if (!p.ReadNumber(version)) return Fail(result, "Expected version number");

            p.SkipWhitespace();
            if (!p.Expect(',')) return Fail(result, "Expected ','");

            p.SkipWhitespace();
            if (!p.ReadString(key, sizeof(key))) return Fail(result, "Expected 'entities' key");
            p.SkipWhitespace();
            if (!p.Expect(':')) return Fail(result, "Expected ':'");

            p.SkipWhitespace();
            if (!p.Expect('[')) return Fail(result, "Expected '['");

            p.SkipWhitespace();
            if (p.Peek() == ']')
            {
                p.Advance();
                p.SkipWhitespace();
                if (p.Peek() == '}') p.Advance();
                result.success = true;
                return result;
            }

            while (p.HasMore())
            {
                if (remap_count >= kMaxEntities)
                    return Fail(result, "Too many entities");

                p.SkipWhitespace();
                if (!p.Expect('{')) return Fail(result, "Expected entity '{'");

                uint64_t serialized_id = 0;
                uint64_t parent_id = 0;
                bool has_parent = false;

                auto builder = world.Spawn();

                while (p.HasMore())
                {
                    p.SkipWhitespace();
                    char field_name[64]{};
                    if (!p.ReadString(field_name, sizeof(field_name)))
                        return Fail(result, "Expected field name");

                    p.SkipWhitespace();
                    if (!p.Expect(':')) return Fail(result, "Expected ':'");
                    p.SkipWhitespace();

                    if (detail::StringsEqual(field_name, "id"))
                    {
                        double num;
                        if (!p.ReadNumber(num)) return Fail(result, "Expected entity id");
                        serialized_id = static_cast<uint64_t>(num);
                    }
                    else if (detail::StringsEqual(field_name, "parent"))
                    {
                        double num;
                        if (!p.ReadNumber(num)) return Fail(result, "Expected parent id");
                        parent_id = static_cast<uint64_t>(num);
                        has_parent = true;
                    }
                    else if (detail::StringsEqual(field_name, "components"))
                    {
                        if (!p.Expect('{')) return Fail(result, "Expected components '{'");

                        p.SkipWhitespace();
                        if (p.Peek() != '}')
                        {
                            while (p.HasMore())
                            {
                                p.SkipWhitespace();
                                char comp_name[64]{};
                                if (!p.ReadString(comp_name, sizeof(comp_name)))
                                    return Fail(result, "Expected component name");

                                p.SkipWhitespace();
                                if (!p.Expect(':')) return Fail(result, "Expected ':'");
                                p.SkipWhitespace();

                                const RegisteredComponent* reg = registry.FindByName(comp_name);
                                if (reg != nullptr && reg->HasReflection())
                                {
                                    if (reg->meta.size > kMaxComponentSize)
                                        return Fail(result, "Component too large");

                                    alignas(16) std::byte buffer[kMaxComponentSize]{};
                                    if (reg->meta.construct != nullptr)
                                    {
                                        reg->meta.construct(buffer);
                                    }

                                    auto comp_result = JsonDeserializer::DeserializeComponent(
                                        buffer, reg->reflection, p.data + p.pos);

                                    if (!comp_result.success)
                                    {
                                        if (reg->meta.destruct != nullptr)
                                            reg->meta.destruct(buffer);
                                        return Fail(result, "Failed to deserialize component");
                                    }

                                    // Advance parser past the component JSON object
                                    p.SkipValue();

                                    builder.WithRaw(reg->meta, buffer);

                                    if (reg->meta.destruct != nullptr)
                                    {
                                        reg->meta.destruct(buffer);
                                    }

                                    ++result.components_loaded;
                                }
                                else
                                {
                                    p.SkipValue();
                                    ++result.components_skipped;
                                }

                                p.SkipWhitespace();
                                if (p.Peek() == ',') { p.Advance(); }
                                else if (p.Peek() == '}') { p.Advance(); break; }
                                else return Fail(result, "Expected ',' or '}' in components");
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
                    if (p.Peek() == ',') { p.Advance(); }
                    else if (p.Peek() == '}') { p.Advance(); break; }
                    else return Fail(result, "Expected ',' or '}' in entity");
                }

                Entity live = builder.Build();

                size_t entity_idx = remap_count;
                remap_table[remap_count] = {serialized_id, live};
                ++remap_count;

                if (has_parent)
                {
                    parent_links[parent_count++] = {entity_idx, parent_id};
                }

                ++result.entities_loaded;

                p.SkipWhitespace();
                if (p.Peek() == ',') { p.Advance(); }
                else if (p.Peek() == ']') { p.Advance(); break; }
                else return Fail(result, "Expected ',' or ']' in entities array");
            }

            // --- Post-processing: Entity field remapping ---
            RemapEntityFields(world, registry, remap_table, remap_count);

            // --- Post-processing: Rebuild hierarchy ---
            for (size_t i = 0; i < parent_count; ++i)
            {
                Entity child = remap_table[parent_links[i].entity_index].live_entity;
                Entity parent = FindRemapped(remap_table, remap_count, parent_links[i].parent_id);
                if (!parent.IsNull() && world.IsAlive(parent) && world.IsAlive(child))
                {
                    world.SetParent(child, parent);
                }
            }

            p.SkipWhitespace();
            if (p.Peek() == '}') p.Advance();

            result.success = true;
            return result;
        }
    };
}
