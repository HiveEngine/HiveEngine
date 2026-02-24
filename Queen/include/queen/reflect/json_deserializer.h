#pragma once

#include <queen/reflect/component_reflector.h>
#include <queen/reflect/enum_reflection.h>
#include <queen/core/entity.h>
#include <hive/core/assert.h>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>

namespace queen
{
    /**
     * Result of a JSON deserialization operation
     */
    struct JsonDeserializeResult
    {
        bool success = false;
        size_t fields_read = 0;
        size_t fields_skipped = 0;
        const char* error = nullptr;
    };

    /**
     * Minimal JSON deserializer driven by reflection data
     *
     * Reads a JSON object string and populates component fields.
     * Unknown fields are skipped (forward-compatible).
     * Missing fields keep their existing value.
     *
     * Limitations:
     * - No support for JSON arrays at top level (only objects)
     * - No Unicode escape sequences (\uXXXX)
     * - Whitespace tolerant
     */
    class JsonDeserializer
    {
    public:
        /**
         * Deserialize a JSON object into a component
         *
         * @param component Pre-constructed component (fields keep defaults if missing in JSON)
         * @param reflection Reflection data for the component
         * @param json JSON string to parse
         * @return Result with success flag and field counts
         */
        static JsonDeserializeResult DeserializeComponent(
            void* component,
            const ComponentReflection& reflection,
            const char* json) noexcept
        {
            JsonDeserializeResult result{};
            Parser p{json};

            p.SkipWhitespace();
            if (!p.Expect('{'))
            {
                result.error = "Expected '{'";
                return result;
            }

            p.SkipWhitespace();
            if (p.Peek() == '}')
            {
                p.Advance();
                result.success = true;
                return result;
            }

            while (p.HasMore())
            {
                p.SkipWhitespace();

                // Read field name
                char field_name[64]{};
                if (!p.ReadString(field_name, sizeof(field_name)))
                {
                    result.error = "Expected field name string";
                    return result;
                }

                p.SkipWhitespace();
                if (!p.Expect(':'))
                {
                    result.error = "Expected ':'";
                    return result;
                }

                p.SkipWhitespace();

                // Find field in reflection
                const FieldInfo* field = reflection.FindField(field_name);
                if (field != nullptr)
                {
                    if (!DeserializeValue(
                        static_cast<std::byte*>(component) + field->offset,
                        *field, p))
                    {
                        result.error = "Failed to parse field value";
                        return result;
                    }
                    ++result.fields_read;
                }
                else
                {
                    // Skip unknown field value
                    if (!p.SkipValue())
                    {
                        result.error = "Failed to skip unknown field";
                        return result;
                    }
                    ++result.fields_skipped;
                }

                p.SkipWhitespace();
                if (p.Peek() == ',')
                {
                    p.Advance();
                }
                else if (p.Peek() == '}')
                {
                    p.Advance();
                    result.success = true;
                    return result;
                }
                else
                {
                    result.error = "Expected ',' or '}'";
                    return result;
                }
            }

            result.error = "Unexpected end of input";
            return result;
        }

    private:
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

            bool ReadBool(bool& out) noexcept
            {
                if (std::strncmp(data + pos, "true", 4) == 0)
                {
                    out = true;
                    pos += 4;
                    return true;
                }
                if (std::strncmp(data + pos, "false", 5) == 0)
                {
                    out = false;
                    pos += 5;
                    return true;
                }
                return false;
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
                if (c == '{')
                {
                    return SkipObject();
                }
                if (c == '[')
                {
                    return SkipArray();
                }
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
                // Number
                double tmp;
                return ReadNumber(tmp);
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

        static bool DeserializeValue(void* ptr, const FieldInfo& field, Parser& p) noexcept
        {
            switch (field.type)
            {
                case FieldType::Int8:
                case FieldType::Int16:
                case FieldType::Int32:
                case FieldType::Int64:
                case FieldType::Uint8:
                case FieldType::Uint16:
                case FieldType::Uint32:
                case FieldType::Uint64:
                {
                    double num;
                    if (!p.ReadNumber(num)) return false;
                    WriteInteger(ptr, field, num);
                    return true;
                }
                case FieldType::Float32:
                {
                    double num;
                    if (!p.ReadNumber(num)) return false;
                    *static_cast<float*>(ptr) = static_cast<float>(num);
                    return true;
                }
                case FieldType::Float64:
                {
                    double num;
                    if (!p.ReadNumber(num)) return false;
                    *static_cast<double*>(ptr) = num;
                    return true;
                }
                case FieldType::Bool:
                {
                    bool val;
                    if (!p.ReadBool(val)) return false;
                    *static_cast<bool*>(ptr) = val;
                    return true;
                }
                case FieldType::Entity:
                {
                    double num;
                    if (!p.ReadNumber(num)) return false;
                    *static_cast<Entity*>(ptr) = Entity::FromU64(static_cast<uint64_t>(num));
                    return true;
                }
                case FieldType::Struct:
                {
                    if (field.nested_fields == nullptr)
                    {
                        return p.SkipValue();
                    }
                    return DeserializeObject(ptr, field.nested_fields, field.nested_field_count, p);
                }
                case FieldType::Enum:
                {
                    // Enum can be string name or integer
                    p.SkipWhitespace();
                    if (p.Peek() == '"')
                    {
                        char name[64]{};
                        if (!p.ReadString(name, sizeof(name))) return false;

                        if (field.enum_info != nullptr)
                        {
                            int64_t val;
                            if (field.enum_info->ValueOf(name, val))
                            {
                                WriteEnumValue(ptr, field, val);
                                return true;
                            }
                        }
                        return false;
                    }
                    else
                    {
                        double num;
                        if (!p.ReadNumber(num)) return false;
                        WriteEnumValue(ptr, field, static_cast<int64_t>(num));
                        return true;
                    }
                }
                case FieldType::String:
                {
                    // FixedString: char buffer, uint8_t size at end
                    char tmp[64]{};
                    if (!p.ReadString(tmp, sizeof(tmp))) return false;
                    size_t len = std::strlen(tmp);
                    size_t max_len = field.size - 2;  // buffer is field.size - 1 (size byte), minus null term
                    if (len > max_len) len = max_len;
                    std::memcpy(ptr, tmp, len);
                    static_cast<char*>(ptr)[len] = '\0';
                    *reinterpret_cast<uint8_t*>(
                        static_cast<std::byte*>(ptr) + (field.size - 1)) = static_cast<uint8_t>(len);
                    return true;
                }
                case FieldType::FixedArray:
                {
                    p.SkipWhitespace();
                    if (p.Peek() != '[') return false;
                    p.Advance();

                    size_t elem_size = (field.element_count > 0) ? (field.size / field.element_count) : 0;
                    for (size_t i = 0; i < field.element_count; ++i)
                    {
                        p.SkipWhitespace();
                        if (i > 0)
                        {
                            if (p.Peek() != ',') return false;
                            p.Advance();
                            p.SkipWhitespace();
                        }
                        auto* elem_ptr = static_cast<std::byte*>(ptr) + (i * elem_size);
                        FieldInfo elem{};
                        elem.type = field.element_type;
                        elem.size = elem_size;
                        elem.offset = 0;
                        if (!DeserializeValue(elem_ptr, elem, p)) return false;
                    }

                    p.SkipWhitespace();
                    if (p.Peek() != ']') return false;
                    p.Advance();
                    return true;
                }
                case FieldType::Invalid:
                    return p.SkipValue();
            }
            return false;
        }

        static bool DeserializeObject(void* base,
                                       const FieldInfo* fields, size_t field_count,
                                       Parser& p) noexcept
        {
            p.SkipWhitespace();
            if (p.Peek() != '{') return false;
            p.Advance();
            p.SkipWhitespace();

            if (p.Peek() == '}') { p.Advance(); return true; }

            while (p.HasMore())
            {
                p.SkipWhitespace();
                char name[64]{};
                if (!p.ReadString(name, sizeof(name))) return false;

                p.SkipWhitespace();
                if (p.Peek() != ':') return false;
                p.Advance();
                p.SkipWhitespace();

                // Find field
                const FieldInfo* found = nullptr;
                for (size_t i = 0; i < field_count; ++i)
                {
                    if (detail::StringsEqual(fields[i].name, name))
                    {
                        found = &fields[i];
                        break;
                    }
                }

                if (found != nullptr)
                {
                    auto* field_ptr = static_cast<std::byte*>(base) + found->offset;
                    if (!DeserializeValue(field_ptr, *found, p)) return false;
                }
                else
                {
                    if (!p.SkipValue()) return false;
                }

                p.SkipWhitespace();
                if (p.Peek() == ',') { p.Advance(); }
                else if (p.Peek() == '}') { p.Advance(); return true; }
                else return false;
            }
            return false;
        }

        static void WriteInteger(void* ptr, const FieldInfo& field, double num) noexcept
        {
            switch (field.type)
            {
                case FieldType::Int8:   *static_cast<int8_t*>(ptr) = static_cast<int8_t>(num); break;
                case FieldType::Int16:  *static_cast<int16_t*>(ptr) = static_cast<int16_t>(num); break;
                case FieldType::Int32:  *static_cast<int32_t*>(ptr) = static_cast<int32_t>(num); break;
                case FieldType::Int64:  *static_cast<int64_t*>(ptr) = static_cast<int64_t>(num); break;
                case FieldType::Uint8:  *static_cast<uint8_t*>(ptr) = static_cast<uint8_t>(num); break;
                case FieldType::Uint16: *static_cast<uint16_t*>(ptr) = static_cast<uint16_t>(num); break;
                case FieldType::Uint32: *static_cast<uint32_t*>(ptr) = static_cast<uint32_t>(num); break;
                case FieldType::Uint64: *static_cast<uint64_t*>(ptr) = static_cast<uint64_t>(num); break;
                default: break;
            }
        }

        static void WriteEnumValue(void* ptr, const FieldInfo& field, int64_t val) noexcept
        {
            size_t sz = field.enum_info ? field.enum_info->underlying_size : field.size;
            switch (sz)
            {
                case 1: *static_cast<uint8_t*>(ptr) = static_cast<uint8_t>(val); break;
                case 2: *static_cast<uint16_t*>(ptr) = static_cast<uint16_t>(val); break;
                case 4: *static_cast<uint32_t*>(ptr) = static_cast<uint32_t>(val); break;
                case 8: *static_cast<uint64_t*>(ptr) = static_cast<uint64_t>(val); break;
                default: break;
            }
        }
    };
}
