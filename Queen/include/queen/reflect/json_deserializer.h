#pragma once

#include <hive/core/assert.h>

#include <queen/core/entity.h>
#include <queen/reflect/component_reflector.h>
#include <queen/reflect/enum_reflection.h>

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>

namespace queen
{
    struct JsonDeserializeResult
    {
        bool m_success = false;
        size_t m_fieldsRead = 0;
        size_t m_fieldsSkipped = 0;
        const char* m_error = nullptr;
    };

    class JsonDeserializer
    {
    public:
        static JsonDeserializeResult DeserializeComponent(void* component, const ComponentReflection& reflection,
                                                          const char* json) noexcept
        {
            JsonDeserializeResult result{};
            Parser parser{json};

            parser.SkipWhitespace();
            if (!parser.Expect('{'))
            {
                result.m_error = "Expected '{'";
                return result;
            }

            parser.SkipWhitespace();
            if (parser.Peek() == '}')
            {
                parser.Advance();
                result.m_success = true;
                return result;
            }

            while (parser.HasMore())
            {
                parser.SkipWhitespace();

                char fieldName[64]{};
                if (!parser.ReadString(fieldName, sizeof(fieldName)))
                {
                    result.m_error = "Expected field name string";
                    return result;
                }

                parser.SkipWhitespace();
                if (!parser.Expect(':'))
                {
                    result.m_error = "Expected ':'";
                    return result;
                }

                parser.SkipWhitespace();

                const FieldInfo* field = reflection.FindField(fieldName);
                if (field != nullptr)
                {
                    if (!DeserializeValue(static_cast<std::byte*>(component) + field->m_offset, *field, parser))
                    {
                        result.m_error = "Failed to parse field value";
                        return result;
                    }
                    ++result.m_fieldsRead;
                }
                else
                {
                    if (!parser.SkipValue())
                    {
                        result.m_error = "Failed to skip unknown field";
                        return result;
                    }
                    ++result.m_fieldsSkipped;
                }

                parser.SkipWhitespace();
                if (parser.Peek() == ',')
                {
                    parser.Advance();
                }
                else if (parser.Peek() == '}')
                {
                    parser.Advance();
                    result.m_success = true;
                    return result;
                }
                else
                {
                    result.m_error = "Expected ',' or '}'";
                    return result;
                }
            }

            result.m_error = "Unexpected end of input";
            return result;
        }

    private:
        struct Parser
        {
            const char* m_data;
            size_t m_pos = 0;

            explicit Parser(const char* json) noexcept
                : m_data{json}
            {
            }

            [[nodiscard]] bool HasMore() const noexcept
            {
                return m_data[m_pos] != '\0';
            }
            [[nodiscard]] char Peek() const noexcept
            {
                return m_data[m_pos];
            }

            void Advance() noexcept
            {
                if (m_data[m_pos] != '\0')
                {
                    ++m_pos;
                }
            }

            void SkipWhitespace() noexcept
            {
                while (m_data[m_pos] == ' ' || m_data[m_pos] == '\t' || m_data[m_pos] == '\n' || m_data[m_pos] == '\r')
                {
                    ++m_pos;
                }
            }

            [[nodiscard]] bool Expect(char c) noexcept
            {
                SkipWhitespace();
                if (m_data[m_pos] == c)
                {
                    ++m_pos;
                    return true;
                }
                return false;
            }

            [[nodiscard]] bool ReadString(char* out, size_t outSize) noexcept
            {
                if (m_data[m_pos] != '"')
                {
                    return false;
                }
                ++m_pos;

                size_t written = 0;
                while (m_data[m_pos] != '\0' && m_data[m_pos] != '"')
                {
                    char c = m_data[m_pos++];
                    if (c == '\\' && m_data[m_pos] != '\0')
                    {
                        const char esc = m_data[m_pos++];
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

            [[nodiscard]] bool ReadNumber(double& out) noexcept
            {
                const char* start = m_data + m_pos;
                char* end = nullptr;
                out = std::strtod(start, &end);
                if (end == start)
                {
                    return false;
                }
                m_pos = static_cast<size_t>(end - m_data);
                return true;
            }

            [[nodiscard]] bool ReadBool(bool& out) noexcept
            {
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

            [[nodiscard]] bool SkipValue() noexcept
            {
                SkipWhitespace();
                const char c = Peek();
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
                    bool tmp = false;
                    return ReadBool(tmp);
                }
                if (c == 'n' && std::strncmp(m_data + m_pos, "null", 4) == 0)
                {
                    m_pos += 4;
                    return true;
                }

                double tmp = 0.0;
                return ReadNumber(tmp);
            }

            [[nodiscard]] bool SkipObject() noexcept
            {
                if (m_data[m_pos] != '{')
                {
                    return false;
                }
                ++m_pos;

                int depth = 1;
                while (m_data[m_pos] != '\0' && depth > 0)
                {
                    if (m_data[m_pos] == '{')
                    {
                        ++depth;
                    }
                    else if (m_data[m_pos] == '}')
                    {
                        --depth;
                    }
                    else if (m_data[m_pos] == '"')
                    {
                        ++m_pos;
                        while (m_data[m_pos] != '\0' && m_data[m_pos] != '"')
                        {
                            if (m_data[m_pos] == '\\')
                            {
                                ++m_pos;
                            }
                            if (m_data[m_pos] != '\0')
                            {
                                ++m_pos;
                            }
                        }
                    }

                    if (m_data[m_pos] != '\0')
                    {
                        ++m_pos;
                    }
                }

                return depth == 0;
            }

            [[nodiscard]] bool SkipArray() noexcept
            {
                if (m_data[m_pos] != '[')
                {
                    return false;
                }
                ++m_pos;

                int depth = 1;
                while (m_data[m_pos] != '\0' && depth > 0)
                {
                    if (m_data[m_pos] == '[')
                    {
                        ++depth;
                    }
                    else if (m_data[m_pos] == ']')
                    {
                        --depth;
                    }
                    else if (m_data[m_pos] == '"')
                    {
                        ++m_pos;
                        while (m_data[m_pos] != '\0' && m_data[m_pos] != '"')
                        {
                            if (m_data[m_pos] == '\\')
                            {
                                ++m_pos;
                            }
                            if (m_data[m_pos] != '\0')
                            {
                                ++m_pos;
                            }
                        }
                    }

                    if (m_data[m_pos] != '\0')
                    {
                        ++m_pos;
                    }
                }

                return depth == 0;
            }
        };

        static bool DeserializeValue(void* ptr, const FieldInfo& field, Parser& parser) noexcept
        {
            switch (field.m_type)
            {
                case FieldType::INT8:
                case FieldType::INT16:
                case FieldType::INT32:
                case FieldType::INT64:
                case FieldType::UINT8:
                case FieldType::UINT16:
                case FieldType::UINT32:
                case FieldType::UINT64: {
                    double num = 0.0;
                    if (!parser.ReadNumber(num))
                    {
                        return false;
                    }
                    WriteInteger(ptr, field, num);
                    return true;
                }
                case FieldType::FLOAT32: {
                    double num = 0.0;
                    if (!parser.ReadNumber(num))
                    {
                        return false;
                    }
                    *static_cast<float*>(ptr) = static_cast<float>(num);
                    return true;
                }
                case FieldType::FLOAT64: {
                    double num = 0.0;
                    if (!parser.ReadNumber(num))
                    {
                        return false;
                    }
                    *static_cast<double*>(ptr) = num;
                    return true;
                }
                case FieldType::BOOL: {
                    bool value = false;
                    if (!parser.ReadBool(value))
                    {
                        return false;
                    }
                    *static_cast<bool*>(ptr) = value;
                    return true;
                }
                case FieldType::ENTITY: {
                    double num = 0.0;
                    if (!parser.ReadNumber(num))
                    {
                        return false;
                    }
                    *static_cast<Entity*>(ptr) = Entity::FromU64(static_cast<uint64_t>(num));
                    return true;
                }
                case FieldType::STRUCT:
                    if (field.m_nestedFields == nullptr)
                    {
                        return parser.SkipValue();
                    }
                    return DeserializeObject(ptr, field.m_nestedFields, field.m_nestedFieldCount, parser);
                case FieldType::ENUM:
                    return DeserializeEnum(ptr, field, parser);
                case FieldType::STRING:
                    return DeserializeString(ptr, field, parser);
                case FieldType::FIXED_ARRAY:
                    return DeserializeFixedArray(ptr, field, parser);
                case FieldType::INVALID:
                    return parser.SkipValue();
            }

            return false;
        }

        static bool DeserializeEnum(void* ptr, const FieldInfo& field, Parser& parser) noexcept
        {
            parser.SkipWhitespace();
            if (parser.Peek() == '"')
            {
                char name[64]{};
                if (!parser.ReadString(name, sizeof(name)))
                {
                    return false;
                }

                if (field.m_enumInfo != nullptr)
                {
                    int64_t value = 0;
                    if (field.m_enumInfo->ValueOf(name, value))
                    {
                        WriteEnumValue(ptr, field, value);
                        return true;
                    }
                }

                return false;
            }

            double num = 0.0;
            if (!parser.ReadNumber(num))
            {
                return false;
            }
            WriteEnumValue(ptr, field, static_cast<int64_t>(num));
            return true;
        }

        static bool DeserializeString(void* ptr, const FieldInfo& field, Parser& parser) noexcept
        {
            char tmp[64]{};
            if (!parser.ReadString(tmp, sizeof(tmp)))
            {
                return false;
            }

            size_t len = std::strlen(tmp);
            const size_t maxLen = field.m_size - 2;
            if (len > maxLen)
            {
                len = maxLen;
            }

            std::memcpy(ptr, tmp, len);
            static_cast<char*>(ptr)[len] = '\0';
            *reinterpret_cast<uint8_t*>(static_cast<std::byte*>(ptr) + (field.m_size - 1)) = static_cast<uint8_t>(len);
            return true;
        }

        static bool DeserializeFixedArray(void* ptr, const FieldInfo& field, Parser& parser) noexcept
        {
            parser.SkipWhitespace();
            if (parser.Peek() != '[')
            {
                return false;
            }
            parser.Advance();

            const size_t elemSize = field.m_elementCount > 0 ? field.m_size / field.m_elementCount : 0;
            for (size_t i = 0; i < field.m_elementCount; ++i)
            {
                parser.SkipWhitespace();
                if (i > 0)
                {
                    if (parser.Peek() != ',')
                    {
                        return false;
                    }
                    parser.Advance();
                    parser.SkipWhitespace();
                }

                FieldInfo elemField{};
                elemField.m_type = field.m_elementType;
                elemField.m_size = elemSize;
                auto* elemPtr = static_cast<std::byte*>(ptr) + (i * elemSize);
                if (!DeserializeValue(elemPtr, elemField, parser))
                {
                    return false;
                }
            }

            parser.SkipWhitespace();
            if (parser.Peek() != ']')
            {
                return false;
            }
            parser.Advance();
            return true;
        }

        static bool DeserializeObject(void* base, const FieldInfo* fields, size_t fieldCount, Parser& parser) noexcept
        {
            parser.SkipWhitespace();
            if (parser.Peek() != '{')
            {
                return false;
            }
            parser.Advance();
            parser.SkipWhitespace();

            if (parser.Peek() == '}')
            {
                parser.Advance();
                return true;
            }

            while (parser.HasMore())
            {
                parser.SkipWhitespace();
                char name[64]{};
                if (!parser.ReadString(name, sizeof(name)))
                {
                    return false;
                }

                parser.SkipWhitespace();
                if (parser.Peek() != ':')
                {
                    return false;
                }
                parser.Advance();
                parser.SkipWhitespace();

                const FieldInfo* found = nullptr;
                for (size_t i = 0; i < fieldCount; ++i)
                {
                    if (detail::StringsEqual(fields[i].m_name, name))
                    {
                        found = &fields[i];
                        break;
                    }
                }

                if (found != nullptr)
                {
                    auto* fieldPtr = static_cast<std::byte*>(base) + found->m_offset;
                    if (!DeserializeValue(fieldPtr, *found, parser))
                    {
                        return false;
                    }
                }
                else if (!parser.SkipValue())
                {
                    return false;
                }

                parser.SkipWhitespace();
                if (parser.Peek() == ',')
                {
                    parser.Advance();
                }
                else if (parser.Peek() == '}')
                {
                    parser.Advance();
                    return true;
                }
                else
                {
                    return false;
                }
            }

            return false;
        }

        static void WriteInteger(void* ptr, const FieldInfo& field, double num) noexcept
        {
            switch (field.m_type)
            {
                case FieldType::INT8:
                    *static_cast<int8_t*>(ptr) = static_cast<int8_t>(num);
                    break;
                case FieldType::INT16:
                    *static_cast<int16_t*>(ptr) = static_cast<int16_t>(num);
                    break;
                case FieldType::INT32:
                    *static_cast<int32_t*>(ptr) = static_cast<int32_t>(num);
                    break;
                case FieldType::INT64:
                    *static_cast<int64_t*>(ptr) = static_cast<int64_t>(num);
                    break;
                case FieldType::UINT8:
                    *static_cast<uint8_t*>(ptr) = static_cast<uint8_t>(num);
                    break;
                case FieldType::UINT16:
                    *static_cast<uint16_t*>(ptr) = static_cast<uint16_t>(num);
                    break;
                case FieldType::UINT32:
                    *static_cast<uint32_t*>(ptr) = static_cast<uint32_t>(num);
                    break;
                case FieldType::UINT64:
                    *static_cast<uint64_t*>(ptr) = static_cast<uint64_t>(num);
                    break;
                default:
                    break;
            }
        }

        static void WriteEnumValue(void* ptr, const FieldInfo& field, int64_t value) noexcept
        {
            const size_t size = field.m_enumInfo != nullptr ? field.m_enumInfo->m_underlyingSize : field.m_size;
            switch (size)
            {
                case 1:
                    *static_cast<uint8_t*>(ptr) = static_cast<uint8_t>(value);
                    break;
                case 2:
                    *static_cast<uint16_t*>(ptr) = static_cast<uint16_t>(value);
                    break;
                case 4:
                    *static_cast<uint32_t*>(ptr) = static_cast<uint32_t>(value);
                    break;
                case 8:
                    *static_cast<uint64_t*>(ptr) = static_cast<uint64_t>(value);
                    break;
                default:
                    break;
            }
        }
    };
} // namespace queen
