#pragma once

#include <queen/reflect/component_reflector.h>
#include <queen/reflect/enum_reflection.h>
#include <queen/core/entity.h>
#include <hive/core/assert.h>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>

namespace queen
{
    /**
     * Minimal JSON serializer driven by reflection data
     *
     * Writes component fields as JSON using a fixed-size char buffer.
     * No external dependencies. Supports all FieldTypes.
     *
     * Enums are written as string names when EnumReflectionBase is available,
     * otherwise as integers.
     *
     * @tparam BufSize Maximum output buffer size in bytes
     */
    template<size_t BufSize = 4096>
    class JsonSerializer
    {
    public:
        JsonSerializer() noexcept = default;

        /**
         * Serialize a component to JSON object string
         *
         * Produces: {"field1": value1, "field2": value2, ...}
         */
        void SerializeComponent(const void* component,
                                const ComponentReflection& reflection) noexcept
        {
            pos_ = 0;
            Put('{');
            for (size_t i = 0; i < reflection.field_count; ++i)
            {
                if (i > 0) Put(',');
                Put('"');
                WriteRaw(reflection.fields[i].name);
                Put('"');
                Put(':');
                SerializeField(component, reflection.fields[i]);
            }
            Put('}');
            buf_[pos_ < BufSize ? pos_ : BufSize - 1] = '\0';
        }

        /**
         * Serialize a single field value to JSON
         */
        void SerializeField(const void* base, const FieldInfo& field) noexcept
        {
            const auto* ptr = static_cast<const std::byte*>(base) + field.offset;
            SerializeValue(ptr, field);
        }

        [[nodiscard]] const char* CStr() const noexcept { return buf_; }
        [[nodiscard]] size_t Size() const noexcept { return pos_; }

    private:
        void SerializeValue(const void* ptr, const FieldInfo& field) noexcept
        {
            switch (field.type)
            {
                case FieldType::Int8:   WriteInt(*static_cast<const int8_t*>(ptr)); break;
                case FieldType::Int16:  WriteInt(*static_cast<const int16_t*>(ptr)); break;
                case FieldType::Int32:  WriteInt(*static_cast<const int32_t*>(ptr)); break;
                case FieldType::Int64:  WriteInt64(*static_cast<const int64_t*>(ptr)); break;
                case FieldType::Uint8:  WriteUint(*static_cast<const uint8_t*>(ptr)); break;
                case FieldType::Uint16: WriteUint(*static_cast<const uint16_t*>(ptr)); break;
                case FieldType::Uint32: WriteUint(*static_cast<const uint32_t*>(ptr)); break;
                case FieldType::Uint64: WriteUint64(*static_cast<const uint64_t*>(ptr)); break;
                case FieldType::Float32: WriteFloat(*static_cast<const float*>(ptr)); break;
                case FieldType::Float64: WriteDouble(*static_cast<const double*>(ptr)); break;
                case FieldType::Bool:
                    WriteRaw(*static_cast<const bool*>(ptr) ? "true" : "false");
                    break;
                case FieldType::Entity:
                    WriteUint64(static_cast<const Entity*>(ptr)->ToU64());
                    break;
                case FieldType::Struct:
                    if (field.nested_fields != nullptr)
                    {
                        Put('{');
                        for (size_t i = 0; i < field.nested_field_count; ++i)
                        {
                            if (i > 0) Put(',');
                            Put('"');
                            WriteRaw(field.nested_fields[i].name);
                            Put('"');
                            Put(':');
                            const auto* nested_ptr = static_cast<const std::byte*>(ptr) + field.nested_fields[i].offset;
                            SerializeValue(nested_ptr, field.nested_fields[i]);
                        }
                        Put('}');
                    }
                    else
                    {
                        WriteRaw("null");
                    }
                    break;
                case FieldType::Enum:
                {
                    int64_t val = ReadEnumValue(ptr, field);
                    const char* name = (field.enum_info != nullptr) ? field.enum_info->NameOf(val) : nullptr;
                    if (name != nullptr)
                    {
                        Put('"');
                        WriteRaw(name);
                        Put('"');
                    }
                    else
                    {
                        WriteInt64(val);
                    }
                    break;
                }
                case FieldType::String:
                {
                    // FixedString layout: char buffer[23], uint8_t size (at offset field.size - 1)
                    uint8_t len = *reinterpret_cast<const uint8_t*>(
                        static_cast<const std::byte*>(ptr) + (field.size - 1));
                    const char* str = static_cast<const char*>(ptr);
                    Put('"');
                    WriteEscaped(str, len);
                    Put('"');
                    break;
                }
                case FieldType::FixedArray:
                {
                    size_t elem_size = (field.element_count > 0) ? (field.size / field.element_count) : 0;
                    Put('[');
                    for (size_t i = 0; i < field.element_count; ++i)
                    {
                        if (i > 0) Put(',');
                        const auto* elem_ptr = static_cast<const std::byte*>(ptr) + (i * elem_size);
                        FieldInfo elem{};
                        elem.type = field.element_type;
                        elem.size = elem_size;
                        elem.offset = 0;
                        SerializeValue(elem_ptr, elem);
                    }
                    Put(']');
                    break;
                }
                case FieldType::Invalid:
                    WriteRaw("null");
                    break;
            }
        }

        static int64_t ReadEnumValue(const void* ptr, const FieldInfo& field) noexcept
        {
            size_t sz = field.enum_info ? field.enum_info->underlying_size : field.size;
            switch (sz)
            {
                case 1: return static_cast<int64_t>(*static_cast<const int8_t*>(ptr));
                case 2: return static_cast<int64_t>(*static_cast<const int16_t*>(ptr));
                case 4: return static_cast<int64_t>(*static_cast<const int32_t*>(ptr));
                case 8: return *static_cast<const int64_t*>(ptr);
                default: return 0;
            }
        }

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

        void WriteEscaped(const char* s, size_t len) noexcept
        {
            for (size_t i = 0; i < len && pos_ < BufSize - 2; ++i)
            {
                char c = s[i];
                if (c == '"' || c == '\\')
                {
                    Put('\\');
                    Put(c);
                }
                else if (c == '\n')
                {
                    Put('\\');
                    Put('n');
                }
                else if (c == '\t')
                {
                    Put('\\');
                    Put('t');
                }
                else
                {
                    Put(c);
                }
            }
        }

        void WriteInt(int32_t v) noexcept
        {
            char tmp[16];
            int n = std::snprintf(tmp, sizeof(tmp), "%d", v);
            for (int i = 0; i < n; ++i) Put(tmp[i]);
        }

        void WriteInt64(int64_t v) noexcept
        {
            char tmp[24];
            int n = std::snprintf(tmp, sizeof(tmp), "%lld", static_cast<long long>(v));
            for (int i = 0; i < n; ++i) Put(tmp[i]);
        }

        void WriteUint(uint32_t v) noexcept
        {
            char tmp[16];
            int n = std::snprintf(tmp, sizeof(tmp), "%u", v);
            for (int i = 0; i < n; ++i) Put(tmp[i]);
        }

        void WriteUint64(uint64_t v) noexcept
        {
            char tmp[24];
            int n = std::snprintf(tmp, sizeof(tmp), "%llu", static_cast<unsigned long long>(v));
            for (int i = 0; i < n; ++i) Put(tmp[i]);
        }

        void WriteFloat(float v) noexcept
        {
            char tmp[32];
            int n = std::snprintf(tmp, sizeof(tmp), "%.9g", static_cast<double>(v));
            for (int i = 0; i < n; ++i) Put(tmp[i]);
        }

        void WriteDouble(double v) noexcept
        {
            char tmp[32];
            int n = std::snprintf(tmp, sizeof(tmp), "%.17g", v);
            for (int i = 0; i < n; ++i) Put(tmp[i]);
        }

        char buf_[BufSize]{};
        size_t pos_ = 0;
    };
}
