#pragma once

#include <queen/reflect/reflectable.h>
#include <queen/reflect/field_info.h>
#include <queen/core/entity.h>
#include <wax/serialization/binary_writer.h>
#include <wax/serialization/binary_reader.h>
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
    template<typename Allocator>
    void SerializeField(const void* component, const FieldInfo& field,
                        wax::BinaryWriter<Allocator>& writer) noexcept
    {
        const auto* field_ptr = static_cast<const std::byte*>(component) + field.offset;

        switch (field.type)
        {
            case FieldType::Int8:
                writer.template Write<int8_t>(*reinterpret_cast<const int8_t*>(field_ptr));
                break;
            case FieldType::Int16:
                writer.template Write<int16_t>(*reinterpret_cast<const int16_t*>(field_ptr));
                break;
            case FieldType::Int32:
                writer.template Write<int32_t>(*reinterpret_cast<const int32_t*>(field_ptr));
                break;
            case FieldType::Int64:
                writer.template Write<int64_t>(*reinterpret_cast<const int64_t*>(field_ptr));
                break;
            case FieldType::Uint8:
                writer.template Write<uint8_t>(*reinterpret_cast<const uint8_t*>(field_ptr));
                break;
            case FieldType::Uint16:
                writer.template Write<uint16_t>(*reinterpret_cast<const uint16_t*>(field_ptr));
                break;
            case FieldType::Uint32:
                writer.template Write<uint32_t>(*reinterpret_cast<const uint32_t*>(field_ptr));
                break;
            case FieldType::Uint64:
                writer.template Write<uint64_t>(*reinterpret_cast<const uint64_t*>(field_ptr));
                break;
            case FieldType::Float32:
                writer.template Write<float>(*reinterpret_cast<const float*>(field_ptr));
                break;
            case FieldType::Float64:
                writer.template Write<double>(*reinterpret_cast<const double*>(field_ptr));
                break;
            case FieldType::Bool:
                writer.template Write<uint8_t>(*reinterpret_cast<const bool*>(field_ptr) ? 1 : 0);
                break;
            case FieldType::Entity:
                writer.template Write<uint64_t>(reinterpret_cast<const Entity*>(field_ptr)->ToU64());
                break;
            case FieldType::Struct:
                if (field.nested_fields != nullptr)
                {
                    // Recursively serialize nested struct fields
                    for (size_t i = 0; i < field.nested_field_count; ++i)
                    {
                        SerializeField(field_ptr, field.nested_fields[i], writer);
                    }
                }
                else
                {
                    // Fallback: raw bytes for non-reflectable nested types
                    writer.WriteBytes(field_ptr, field.size);
                }
                break;
            case FieldType::Invalid:
                break;
        }
    }

    /**
     * Deserialize a single field from binary
     */
    inline void DeserializeField(void* component, const FieldInfo& field,
                                 wax::BinaryReader& reader) noexcept
    {
        auto* field_ptr = static_cast<std::byte*>(component) + field.offset;

        switch (field.type)
        {
            case FieldType::Int8:
                *reinterpret_cast<int8_t*>(field_ptr) = reader.Read<int8_t>();
                break;
            case FieldType::Int16:
                *reinterpret_cast<int16_t*>(field_ptr) = reader.Read<int16_t>();
                break;
            case FieldType::Int32:
                *reinterpret_cast<int32_t*>(field_ptr) = reader.Read<int32_t>();
                break;
            case FieldType::Int64:
                *reinterpret_cast<int64_t*>(field_ptr) = reader.Read<int64_t>();
                break;
            case FieldType::Uint8:
                *reinterpret_cast<uint8_t*>(field_ptr) = reader.Read<uint8_t>();
                break;
            case FieldType::Uint16:
                *reinterpret_cast<uint16_t*>(field_ptr) = reader.Read<uint16_t>();
                break;
            case FieldType::Uint32:
                *reinterpret_cast<uint32_t*>(field_ptr) = reader.Read<uint32_t>();
                break;
            case FieldType::Uint64:
                *reinterpret_cast<uint64_t*>(field_ptr) = reader.Read<uint64_t>();
                break;
            case FieldType::Float32:
                *reinterpret_cast<float*>(field_ptr) = reader.Read<float>();
                break;
            case FieldType::Float64:
                *reinterpret_cast<double*>(field_ptr) = reader.Read<double>();
                break;
            case FieldType::Bool:
                *reinterpret_cast<bool*>(field_ptr) = reader.Read<uint8_t>() != 0;
                break;
            case FieldType::Entity:
                *reinterpret_cast<Entity*>(field_ptr) = Entity::FromU64(reader.Read<uint64_t>());
                break;
            case FieldType::Struct:
                if (field.nested_fields != nullptr)
                {
                    // Recursively deserialize nested struct fields
                    for (size_t i = 0; i < field.nested_field_count; ++i)
                    {
                        DeserializeField(field_ptr, field.nested_fields[i], reader);
                    }
                }
                else
                {
                    // Fallback: raw bytes for non-reflectable nested types
                    reader.ReadBytes(field_ptr, field.size);
                }
                break;
            case FieldType::Invalid:
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
    template<typename Allocator>
    void SerializeComponent(const void* component, const ComponentReflection& reflection,
                            wax::BinaryWriter<Allocator>& writer) noexcept
    {
        for (size_t i = 0; i < reflection.field_count; ++i)
        {
            SerializeField(component, reflection.fields[i], writer);
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
                                     wax::BinaryReader& reader) noexcept
    {
        for (size_t i = 0; i < reflection.field_count; ++i)
        {
            DeserializeField(component, reflection.fields[i], reader);
        }
    }

    /**
     * Serialize a reflectable component (type-safe version)
     */
    template<Reflectable T, typename Allocator>
    void Serialize(const T& component, wax::BinaryWriter<Allocator>& writer) noexcept
    {
        auto reflection = GetReflectionData<T>();
        SerializeComponent(&component, reflection, writer);
    }

    /**
     * Deserialize a reflectable component (type-safe version)
     */
    template<Reflectable T>
    void Deserialize(T& component, wax::BinaryReader& reader) noexcept
    {
        auto reflection = GetReflectionData<T>();
        DeserializeComponent(&component, reflection, reader);
    }
}
