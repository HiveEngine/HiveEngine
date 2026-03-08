#pragma once

#include <wax/serialization/binary_reader.h>
#include <wax/serialization/binary_writer.h>

#include <queen/core/entity.h>
#include <queen/reflect/field_info.h>
#include <queen/reflect/reflectable.h>

#include <cstddef>
#include <cstring>

namespace queen
{
    /**
     * Component serialization using reflection data
     *
     * Provides serialize/deserialize functions that use ComponentReflection
     * to read/write component fields. Supports all primitive types and
     * nested reflectable structs.
     *
     * Binary format per component:
     * ┌────────────────────────────────────────────────────────────────┐
     * │ For each field (in registration order):                       │
     * │   - Raw bytes (field.size bytes)                              │
     * └────────────────────────────────────────────────────────────────┘
     *
     * Note: Field names are NOT serialized - layout must match exactly.
     * Use versioning at a higher level if schema changes are needed.
     *
     * Performance characteristics:
     * - Serialize: O(n) where n = field count
     * - Deserialize: O(n) where n = field count
     * - Memory: No allocations (writes to provided writer/reader)
     *
     * Limitations:
     * - No support for variable-length data (strings, vectors)
     * - Schema must match exactly between serialize/deserialize
     * - Nested Reflectable types must be registered
     *
     * Example:
     * @code
     *   Position pos{1.0f, 2.0f, 3.0f};
     *   auto reflection = GetReflectionData<Position>();
     *
     *   // Serialize
     *   BinaryWriter writer{allocator};
     *   SerializeComponent(&pos, reflection, writer);
     *
     *   // Deserialize
     *   Position loaded{};
     *   BinaryReader reader{writer.View()};
     *   DeserializeComponent(&loaded, reflection, reader);
     * @endcode
     */

    /**
     * Serialize a single field to binary
     */
    inline void SerializeField(const void* component, const FieldInfo& field, wax::BinaryWriter& writer) noexcept {
        const auto* fieldPtr = static_cast<const std::byte*>(component) + field.m_offset;

        switch (field.m_type)
        {
            case FieldType::INT8:
                writer.template Write<int8_t>(*reinterpret_cast<const int8_t*>(fieldPtr));
                break;
            case FieldType::INT16:
                writer.template Write<int16_t>(*reinterpret_cast<const int16_t*>(fieldPtr));
                break;
            case FieldType::INT32:
                writer.template Write<int32_t>(*reinterpret_cast<const int32_t*>(fieldPtr));
                break;
            case FieldType::INT64:
                writer.template Write<int64_t>(*reinterpret_cast<const int64_t*>(fieldPtr));
                break;
            case FieldType::UINT8:
                writer.template Write<uint8_t>(*reinterpret_cast<const uint8_t*>(fieldPtr));
                break;
            case FieldType::UINT16:
                writer.template Write<uint16_t>(*reinterpret_cast<const uint16_t*>(fieldPtr));
                break;
            case FieldType::UINT32:
                writer.template Write<uint32_t>(*reinterpret_cast<const uint32_t*>(fieldPtr));
                break;
            case FieldType::UINT64:
                writer.template Write<uint64_t>(*reinterpret_cast<const uint64_t*>(fieldPtr));
                break;
            case FieldType::FLOAT32:
                writer.template Write<float>(*reinterpret_cast<const float*>(fieldPtr));
                break;
            case FieldType::FLOAT64:
                writer.template Write<double>(*reinterpret_cast<const double*>(fieldPtr));
                break;
            case FieldType::BOOL:
                writer.template Write<uint8_t>(*reinterpret_cast<const bool*>(fieldPtr) ? 1 : 0);
                break;
            case FieldType::ENTITY:
                writer.template Write<uint64_t>(reinterpret_cast<const Entity*>(fieldPtr)->ToU64());
                break;
            case FieldType::STRUCT:
                if (field.m_nestedFields != nullptr)
                {
                    // Recursively serialize nested struct fields
                    for (size_t i = 0; i < field.m_nestedFieldCount; ++i)
                    {
                        SerializeField(fieldPtr, field.m_nestedFields[i], writer);
                    }
                }
                else
                {
                    // Fallback: raw bytes for non-reflectable nested types
                    writer.WriteBytes(fieldPtr, field.m_size);
                }
                break;
            case FieldType::ENUM: {
                // Serialize as raw bytes matching the underlying type size
                size_t enumSize = field.m_enumInfo ? field.m_enumInfo->m_underlyingSize : field.m_size;
                switch (enumSize)
                {
                    case 1:
                        writer.template Write<uint8_t>(*reinterpret_cast<const uint8_t*>(fieldPtr));
                        break;
                    case 2:
                        writer.template Write<uint16_t>(*reinterpret_cast<const uint16_t*>(fieldPtr));
                        break;
                    case 4:
                        writer.template Write<uint32_t>(*reinterpret_cast<const uint32_t*>(fieldPtr));
                        break;
                    case 8:
                        writer.template Write<uint64_t>(*reinterpret_cast<const uint64_t*>(fieldPtr));
                        break;
                    default:
                        writer.WriteBytes(fieldPtr, field.m_size);
                        break;
                }
                break;
            }
            case FieldType::STRING: {
                // FixedString: write length (uint8) + chars
                // Layout: char buffer[23] at offset 0, uint8_t size at offset 23
                uint8_t len =
                    *reinterpret_cast<const uint8_t*>(static_cast<const std::byte*>(fieldPtr) + (field.m_size - 1));
                writer.template Write<uint8_t>(len);
                writer.WriteBytes(fieldPtr, len);
                break;
            }
            case FieldType::FIXED_ARRAY: {
                // Write each element sequentially
                size_t elemSize = (field.m_elementCount > 0) ? (field.m_size / field.m_elementCount) : 0;
                for (size_t i = 0; i < field.m_elementCount; ++i)
                {
                    const auto* elemPtr = static_cast<const std::byte*>(fieldPtr) + (i * elemSize);
                    FieldInfo elemField{};
                    elemField.m_name = "";
                    elemField.m_offset = 0;
                    elemField.m_size = elemSize;
                    elemField.m_type = field.m_elementType;
                    SerializeField(elemPtr, elemField, writer);
                }
                break;
            }
            case FieldType::INVALID:
                break;
        }
    }

    /**
     * Deserialize a single field from binary
     */
    inline void DeserializeField(void* component, const FieldInfo& field, wax::BinaryReader& reader) noexcept {
        auto* fieldPtr = static_cast<std::byte*>(component) + field.m_offset;

        switch (field.m_type)
        {
            case FieldType::INT8:
                *reinterpret_cast<int8_t*>(fieldPtr) = reader.Read<int8_t>();
                break;
            case FieldType::INT16:
                *reinterpret_cast<int16_t*>(fieldPtr) = reader.Read<int16_t>();
                break;
            case FieldType::INT32:
                *reinterpret_cast<int32_t*>(fieldPtr) = reader.Read<int32_t>();
                break;
            case FieldType::INT64:
                *reinterpret_cast<int64_t*>(fieldPtr) = reader.Read<int64_t>();
                break;
            case FieldType::UINT8:
                *reinterpret_cast<uint8_t*>(fieldPtr) = reader.Read<uint8_t>();
                break;
            case FieldType::UINT16:
                *reinterpret_cast<uint16_t*>(fieldPtr) = reader.Read<uint16_t>();
                break;
            case FieldType::UINT32:
                *reinterpret_cast<uint32_t*>(fieldPtr) = reader.Read<uint32_t>();
                break;
            case FieldType::UINT64:
                *reinterpret_cast<uint64_t*>(fieldPtr) = reader.Read<uint64_t>();
                break;
            case FieldType::FLOAT32:
                *reinterpret_cast<float*>(fieldPtr) = reader.Read<float>();
                break;
            case FieldType::FLOAT64:
                *reinterpret_cast<double*>(fieldPtr) = reader.Read<double>();
                break;
            case FieldType::BOOL:
                *reinterpret_cast<bool*>(fieldPtr) = reader.Read<uint8_t>() != 0;
                break;
            case FieldType::ENTITY:
                *reinterpret_cast<Entity*>(fieldPtr) = Entity::FromU64(reader.Read<uint64_t>());
                break;
            case FieldType::STRUCT:
                if (field.m_nestedFields != nullptr)
                {
                    // Recursively deserialize nested struct fields
                    for (size_t i = 0; i < field.m_nestedFieldCount; ++i)
                    {
                        DeserializeField(fieldPtr, field.m_nestedFields[i], reader);
                    }
                }
                else
                {
                    // Fallback: raw bytes for non-reflectable nested types
                    reader.ReadBytes(fieldPtr, field.m_size);
                }
                break;
            case FieldType::ENUM: {
                size_t enumSize = field.m_enumInfo ? field.m_enumInfo->m_underlyingSize : field.m_size;
                switch (enumSize)
                {
                    case 1:
                        *reinterpret_cast<uint8_t*>(fieldPtr) = reader.Read<uint8_t>();
                        break;
                    case 2:
                        *reinterpret_cast<uint16_t*>(fieldPtr) = reader.Read<uint16_t>();
                        break;
                    case 4:
                        *reinterpret_cast<uint32_t*>(fieldPtr) = reader.Read<uint32_t>();
                        break;
                    case 8:
                        *reinterpret_cast<uint64_t*>(fieldPtr) = reader.Read<uint64_t>();
                        break;
                    default:
                        reader.ReadBytes(fieldPtr, field.m_size);
                        break;
                }
                break;
            }
            case FieldType::STRING: {
                // FixedString: read length (uint8) + chars, then null-terminate
                uint8_t len = reader.Read<uint8_t>();
                reader.ReadBytes(fieldPtr, len);
                // Null-terminate the buffer
                auto* charPtr = reinterpret_cast<char*>(fieldPtr);
                charPtr[len] = '\0';
                // Write size byte (last byte of the FixedString struct)
                *reinterpret_cast<uint8_t*>(static_cast<std::byte*>(fieldPtr) + (field.m_size - 1)) = len;
                break;
            }
            case FieldType::FIXED_ARRAY: {
                size_t elemSize = (field.m_elementCount > 0) ? (field.m_size / field.m_elementCount) : 0;
                for (size_t i = 0; i < field.m_elementCount; ++i)
                {
                    auto* elemPtr = static_cast<std::byte*>(fieldPtr) + (i * elemSize);
                    FieldInfo elemField{};
                    elemField.m_name = "";
                    elemField.m_offset = 0;
                    elemField.m_size = elemSize;
                    elemField.m_type = field.m_elementType;
                    DeserializeField(elemPtr, elemField, reader);
                }
                break;
            }
            case FieldType::INVALID:
                break;
        }
    }

    /**
     * Serialize a component using its reflection data
     *
     * @param component Pointer to the component to serialize
     * @param reflection Reflection data describing the component's fields
     * @param writer BinaryWriter to write to
     */
    inline void SerializeComponent(const void* component, const ComponentReflection& reflection,
                                   wax::BinaryWriter& writer) noexcept {
        for (size_t i = 0; i < reflection.m_fieldCount; ++i)
        {
            SerializeField(component, reflection.m_fields[i], writer);
        }
    }

    /**
     * Deserialize a component using its reflection data
     *
     * @param component Pointer to the component to deserialize into
     * @param reflection Reflection data describing the component's fields
     * @param reader BinaryReader to read from
     */
    inline void DeserializeComponent(void* component, const ComponentReflection& reflection,
                                     wax::BinaryReader& reader) noexcept {
        for (size_t i = 0; i < reflection.m_fieldCount; ++i)
        {
            DeserializeField(component, reflection.m_fields[i], reader);
        }
    }

    /**
     * Serialize a reflectable component (type-safe version)
     */
    template <Reflectable T> void Serialize(const T& component, wax::BinaryWriter& writer) noexcept {
        auto reflection = GetReflectionData<T>();
        SerializeComponent(&component, reflection, writer);
    }

    /**
     * Deserialize a reflectable component (type-safe version)
     */
    template <Reflectable T> void Deserialize(T& component, wax::BinaryReader& reader) noexcept {
        auto reflection = GetReflectionData<T>();
        DeserializeComponent(&component, reflection, reader);
    }
} // namespace queen
