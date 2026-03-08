#pragma once

#include <hive/core/assert.h>

#include <queen/core/entity.h>
#include <queen/reflect/component_reflector.h>
#include <queen/reflect/enum_reflection.h>

#include <cstddef>
#include <cstdint>
#include <cstdio>

namespace queen
{
    template <size_t BufSize = 4096> class JsonSerializer
    {
    public:
        JsonSerializer() noexcept = default;

        void SerializeComponent(const void* component, const ComponentReflection& reflection) noexcept {
            m_pos = 0;
            Put('{');
            for (size_t i = 0; i < reflection.m_fieldCount; ++i)
            {
                const FieldInfo& field = reflection.m_fields[i];
                if (i > 0)
                {
                    Put(',');
                }

                Put('"');
                WriteRaw(field.m_name);
                Put('"');
                Put(':');
                SerializeField(component, field);
            }
            Put('}');
            m_buf[m_pos < BufSize ? m_pos : BufSize - 1] = '\0';
        }

        void SerializeField(const void* base, const FieldInfo& field) noexcept {
            const auto* ptr = static_cast<const std::byte*>(base) + field.m_offset;
            SerializeValue(ptr, field);
        }

        [[nodiscard]] const char* CStr() const noexcept { return m_buf; }
        [[nodiscard]] size_t Size() const noexcept { return m_pos; }

    private:
        void SerializeValue(const void* ptr, const FieldInfo& field) noexcept {
            switch (field.m_type)
            {
                case FieldType::INT8:
                    WriteInt(*static_cast<const int8_t*>(ptr));
                    break;
                case FieldType::INT16:
                    WriteInt(*static_cast<const int16_t*>(ptr));
                    break;
                case FieldType::INT32:
                    WriteInt(*static_cast<const int32_t*>(ptr));
                    break;
                case FieldType::INT64:
                    WriteInt64(*static_cast<const int64_t*>(ptr));
                    break;
                case FieldType::UINT8:
                    WriteUint(*static_cast<const uint8_t*>(ptr));
                    break;
                case FieldType::UINT16:
                    WriteUint(*static_cast<const uint16_t*>(ptr));
                    break;
                case FieldType::UINT32:
                    WriteUint(*static_cast<const uint32_t*>(ptr));
                    break;
                case FieldType::UINT64:
                    WriteUint64(*static_cast<const uint64_t*>(ptr));
                    break;
                case FieldType::FLOAT32:
                    WriteFloat(*static_cast<const float*>(ptr));
                    break;
                case FieldType::FLOAT64:
                    WriteDouble(*static_cast<const double*>(ptr));
                    break;
                case FieldType::BOOL:
                    WriteRaw(*static_cast<const bool*>(ptr) ? "true" : "false");
                    break;
                case FieldType::ENTITY:
                    WriteUint64(static_cast<const Entity*>(ptr)->ToU64());
                    break;
                case FieldType::STRUCT:
                    SerializeStruct(ptr, field);
                    break;
                case FieldType::ENUM:
                    SerializeEnum(ptr, field);
                    break;
                case FieldType::STRING:
                    SerializeString(ptr, field);
                    break;
                case FieldType::FIXED_ARRAY:
                    SerializeFixedArray(ptr, field);
                    break;
                case FieldType::INVALID:
                    WriteRaw("null");
                    break;
            }
        }

        void SerializeStruct(const void* ptr, const FieldInfo& field) noexcept {
            if (field.m_nestedFields == nullptr)
            {
                WriteRaw("null");
                return;
            }

            Put('{');
            for (size_t i = 0; i < field.m_nestedFieldCount; ++i)
            {
                const FieldInfo& nestedField = field.m_nestedFields[i];
                if (i > 0)
                {
                    Put(',');
                }

                Put('"');
                WriteRaw(nestedField.m_name);
                Put('"');
                Put(':');

                const auto* nestedPtr = static_cast<const std::byte*>(ptr) + nestedField.m_offset;
                SerializeValue(nestedPtr, nestedField);
            }
            Put('}');
        }

        void SerializeEnum(const void* ptr, const FieldInfo& field) noexcept {
            const int64_t value = ReadEnumValue(ptr, field);
            const char* name = field.m_enumInfo != nullptr ? field.m_enumInfo->NameOf(value) : nullptr;
            if (name != nullptr)
            {
                Put('"');
                WriteRaw(name);
                Put('"');
            }
            else
            {
                WriteInt64(value);
            }
        }

        void SerializeString(const void* ptr, const FieldInfo& field) noexcept {
            const uint8_t len =
                *reinterpret_cast<const uint8_t*>(static_cast<const std::byte*>(ptr) + (field.m_size - 1));
            const char* str = static_cast<const char*>(ptr);
            Put('"');
            WriteEscaped(str, len);
            Put('"');
        }

        void SerializeFixedArray(const void* ptr, const FieldInfo& field) noexcept {
            const size_t elemSize = field.m_elementCount > 0 ? field.m_size / field.m_elementCount : 0;
            Put('[');
            for (size_t i = 0; i < field.m_elementCount; ++i)
            {
                if (i > 0)
                {
                    Put(',');
                }

                FieldInfo elemField{};
                elemField.m_type = field.m_elementType;
                elemField.m_size = elemSize;
                const auto* elemPtr = static_cast<const std::byte*>(ptr) + (i * elemSize);
                SerializeValue(elemPtr, elemField);
            }
            Put(']');
        }

        static int64_t ReadEnumValue(const void* ptr, const FieldInfo& field) noexcept {
            const size_t size = field.m_enumInfo != nullptr ? field.m_enumInfo->m_underlyingSize : field.m_size;
            switch (size)
            {
                case 1:
                    return static_cast<int64_t>(*static_cast<const int8_t*>(ptr));
                case 2:
                    return static_cast<int64_t>(*static_cast<const int16_t*>(ptr));
                case 4:
                    return static_cast<int64_t>(*static_cast<const int32_t*>(ptr));
                case 8:
                    return *static_cast<const int64_t*>(ptr);
                default:
                    return 0;
            }
        }

        void Put(char c) noexcept {
            if (m_pos < BufSize - 1)
            {
                m_buf[m_pos++] = c;
            }
        }

        void WriteRaw(const char* s) noexcept {
            if (s == nullptr)
            {
                return;
            }

            while (*s != '\0' && m_pos < BufSize - 1)
            {
                m_buf[m_pos++] = *s++;
            }
        }

        void WriteEscaped(const char* s, size_t len) noexcept {
            for (size_t i = 0; i < len && m_pos < BufSize - 2; ++i)
            {
                const char c = s[i];
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

        void WriteInt(int32_t v) noexcept {
            char tmp[16];
            const int count = std::snprintf(tmp, sizeof(tmp), "%d", v);
            for (int i = 0; i < count; ++i)
            {
                Put(tmp[i]);
            }
        }

        void WriteInt64(int64_t v) noexcept {
            char tmp[24];
            const int count = std::snprintf(tmp, sizeof(tmp), "%lld", static_cast<long long>(v));
            for (int i = 0; i < count; ++i)
            {
                Put(tmp[i]);
            }
        }

        void WriteUint(uint32_t v) noexcept {
            char tmp[16];
            const int count = std::snprintf(tmp, sizeof(tmp), "%u", v);
            for (int i = 0; i < count; ++i)
            {
                Put(tmp[i]);
            }
        }

        void WriteUint64(uint64_t v) noexcept {
            char tmp[24];
            const int count = std::snprintf(tmp, sizeof(tmp), "%llu", static_cast<unsigned long long>(v));
            for (int i = 0; i < count; ++i)
            {
                Put(tmp[i]);
            }
        }

        void WriteFloat(float v) noexcept {
            char tmp[32];
            const int count = std::snprintf(tmp, sizeof(tmp), "%.9g", static_cast<double>(v));
            for (int i = 0; i < count; ++i)
            {
                Put(tmp[i]);
            }
        }

        void WriteDouble(double v) noexcept {
            char tmp[32];
            const int count = std::snprintf(tmp, sizeof(tmp), "%.17g", v);
            for (int i = 0; i < count; ++i)
            {
                Put(tmp[i]);
            }
        }

        char m_buf[BufSize]{};
        size_t m_pos = 0;
    };
} // namespace queen
