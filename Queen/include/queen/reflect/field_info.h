#pragma once

#include <queen/core/type_id.h>
#include <cstddef>
#include <cstdint>

namespace queen
{
    /**
     * Field type enumeration for serialization
     *
     * Represents the primitive types that can be serialized.
     * Complex types are built from these primitives.
     */
    enum class FieldType : uint8_t
    {
        Invalid = 0,

        // Integers
        Int8,
        Int16,
        Int32,
        Int64,
        Uint8,
        Uint16,
        Uint32,
        Uint64,

        // Floating point
        Float32,
        Float64,

        // Other primitives
        Bool,
        Entity,     // queen::Entity

        // Compound
        Struct,     // Nested reflectable struct
        Enum,       // Backed by integer, with EnumReflectionBase* for name mapping
        String,     // wax::FixedString (fixed-capacity, no allocator)
        FixedArray, // C-style T[N] array
    };

    /**
     * Runtime field metadata for reflection
     *
     * Stores information about a single field in a component:
     * - Name for debugging/editors
     * - Offset for memory access
     * - Type for serialization
     * - Size for memcpy operations
     *
     * Memory layout:
     * ┌────────────────────────────────────────────────────────────────┐
     * │ name: const char* (8 bytes)                                    │
     * │ offset: size_t (8 bytes)                                       │
     * │ size: size_t (8 bytes)                                         │
     * │ type: FieldType (1 byte) + padding (7 bytes)                   │
     * │ nested_type_id: TypeId (8 bytes) - for Struct types            │
     * └────────────────────────────────────────────────────────────────┘
     *
     * Performance characteristics:
     * - All operations: O(1)
     * - Size: 40 bytes
     *
     * Example:
     * @code
     *   FieldInfo info{"x", offsetof(Position, x), sizeof(float), FieldType::Float32};
     *   void* field_ptr = static_cast<std::byte*>(component_ptr) + info.offset;
     * @endcode
     */
    namespace detail
    {
        /**
         * Constexpr-safe string comparison (no <cstring> dependency)
         */
        constexpr bool StringsEqual(const char* a, const char* b) noexcept
        {
            if (a == b) return true;
            if (a == nullptr || b == nullptr) return false;

            while (*a && *b)
            {
                if (*a != *b) return false;
                ++a;
                ++b;
            }
            return *a == *b;
        }
    }

    struct FieldAttributes;
    struct EnumReflectionBase;

    struct FieldInfo
    {
        const char* name = nullptr;
        size_t offset = 0;
        size_t size = 0;
        FieldType type = FieldType::Invalid;
        TypeId nested_type_id = 0;          // For FieldType::Struct, the TypeId of the nested type
        const FieldInfo* nested_fields = nullptr;   // For Struct: field layout of the nested type
        size_t nested_field_count = 0;              // For Struct: number of nested fields

        // Phase 1 extensions
        const EnumReflectionBase* enum_info = nullptr;   // For Enum: name↔value mapping
        size_t element_count = 0;                        // For FixedArray: number of elements
        FieldType element_type = FieldType::Invalid;     // For FixedArray: element type
        const FieldAttributes* attributes = nullptr;     // Editor annotations (optional)

        [[nodiscard]] constexpr bool IsValid() const noexcept
        {
            return name != nullptr && type != FieldType::Invalid;
        }

        [[nodiscard]] constexpr bool IsNumeric() const noexcept
        {
            return type >= FieldType::Int8 && type <= FieldType::Float64;
        }

        [[nodiscard]] constexpr bool IsInteger() const noexcept
        {
            return type >= FieldType::Int8 && type <= FieldType::Uint64;
        }

        [[nodiscard]] constexpr bool IsSigned() const noexcept
        {
            return type >= FieldType::Int8 && type <= FieldType::Int64;
        }

        [[nodiscard]] constexpr bool IsFloatingPoint() const noexcept
        {
            return type == FieldType::Float32 || type == FieldType::Float64;
        }

        [[nodiscard]] constexpr bool IsStruct() const noexcept
        {
            return type == FieldType::Struct;
        }

        [[nodiscard]] constexpr bool IsEnum() const noexcept
        {
            return type == FieldType::Enum;
        }

        [[nodiscard]] constexpr bool IsString() const noexcept
        {
            return type == FieldType::String;
        }

        [[nodiscard]] constexpr bool IsFixedArray() const noexcept
        {
            return type == FieldType::FixedArray;
        }
    };

    namespace detail
    {
        // Type to FieldType mapping
        template<typename T>
        constexpr FieldType GetFieldType() noexcept
        {
            if constexpr (std::is_same_v<T, int8_t>) return FieldType::Int8;
            else if constexpr (std::is_same_v<T, int16_t>) return FieldType::Int16;
            else if constexpr (std::is_same_v<T, int32_t>) return FieldType::Int32;
            else if constexpr (std::is_same_v<T, int64_t>) return FieldType::Int64;
            else if constexpr (std::is_same_v<T, uint8_t>) return FieldType::Uint8;
            else if constexpr (std::is_same_v<T, uint16_t>) return FieldType::Uint16;
            else if constexpr (std::is_same_v<T, uint32_t>) return FieldType::Uint32;
            else if constexpr (std::is_same_v<T, uint64_t>) return FieldType::Uint64;
            else if constexpr (std::is_same_v<T, float>) return FieldType::Float32;
            else if constexpr (std::is_same_v<T, double>) return FieldType::Float64;
            else if constexpr (std::is_same_v<T, bool>) return FieldType::Bool;
            else return FieldType::Struct;
        }
    }
}
