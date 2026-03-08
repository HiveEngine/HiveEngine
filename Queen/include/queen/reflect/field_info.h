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
        INVALID = 0,

        // Integers
        INT8,
        INT16,
        INT32,
        INT64,
        UINT8,
        UINT16,
        UINT32,
        UINT64,

        // Floating point
        FLOAT32,
        FLOAT64,

        // Other primitives
        BOOL,
        ENTITY, // queen::Entity

        // Compound
        STRUCT,      // Nested reflectable struct
        ENUM,        // Backed by integer, with EnumReflectionBase* for name mapping
        STRING,      // wax::FixedString (fixed-capacity, no allocator)
        FIXED_ARRAY, // C-style T[N] array
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
            if (a == b)
                return true;
            if (a == nullptr || b == nullptr)
                return false;

            while (*a && *b)
            {
                if (*a != *b)
                    return false;
                ++a;
                ++b;
            }
            return *a == *b;
        }
    } // namespace detail

    struct FieldAttributes;
    struct EnumReflectionBase;

    struct FieldInfo
    {
        const char* m_name = nullptr;
        size_t m_offset = 0;
        size_t m_size = 0;
        FieldType m_type = FieldType::INVALID;
        TypeId m_nestedTypeId = 0;                 // For FieldType::Struct, the TypeId of the nested type
        const FieldInfo* m_nestedFields = nullptr; // For Struct: field layout of the nested type
        size_t m_nestedFieldCount = 0;             // For Struct: number of nested fields

        // Phase 1 extensions
        const EnumReflectionBase* m_enumInfo = nullptr; // For Enum: name↔value mapping
        size_t m_elementCount = 0;                      // For FixedArray: number of elements
        FieldType m_elementType = FieldType::INVALID;   // For FixedArray: element type
        const FieldAttributes* m_attributes = nullptr;  // Editor annotations (optional)

        [[nodiscard]] constexpr bool IsValid() const noexcept
        {
            return m_name != nullptr && m_type != FieldType::INVALID;
        }

        [[nodiscard]] constexpr bool IsNumeric() const noexcept
        {
            return m_type >= FieldType::INT8 && m_type <= FieldType::FLOAT64;
        }

        [[nodiscard]] constexpr bool IsInteger() const noexcept
        {
            return m_type >= FieldType::INT8 && m_type <= FieldType::UINT64;
        }

        [[nodiscard]] constexpr bool IsSigned() const noexcept
        {
            return m_type >= FieldType::INT8 && m_type <= FieldType::INT64;
        }

        [[nodiscard]] constexpr bool IsFloatingPoint() const noexcept
        {
            return m_type == FieldType::FLOAT32 || m_type == FieldType::FLOAT64;
        }

        [[nodiscard]] constexpr bool IsStruct() const noexcept
        {
            return m_type == FieldType::STRUCT;
        }

        [[nodiscard]] constexpr bool IsEnum() const noexcept
        {
            return m_type == FieldType::ENUM;
        }

        [[nodiscard]] constexpr bool IsString() const noexcept
        {
            return m_type == FieldType::STRING;
        }

        [[nodiscard]] constexpr bool IsFixedArray() const noexcept
        {
            return m_type == FieldType::FIXED_ARRAY;
        }
    };

    namespace detail
    {
        // Type to FieldType mapping
        template <typename T> constexpr FieldType GetFieldType() noexcept
        {
            if constexpr (std::is_same_v<T, int8_t>)
                return FieldType::INT8;
            else if constexpr (std::is_same_v<T, int16_t>)
                return FieldType::INT16;
            else if constexpr (std::is_same_v<T, int32_t>)
                return FieldType::INT32;
            else if constexpr (std::is_same_v<T, int64_t>)
                return FieldType::INT64;
            else if constexpr (std::is_same_v<T, uint8_t>)
                return FieldType::UINT8;
            else if constexpr (std::is_same_v<T, uint16_t>)
                return FieldType::UINT16;
            else if constexpr (std::is_same_v<T, uint32_t>)
                return FieldType::UINT32;
            else if constexpr (std::is_same_v<T, uint64_t>)
                return FieldType::UINT64;
            else if constexpr (std::is_same_v<T, float>)
                return FieldType::FLOAT32;
            else if constexpr (std::is_same_v<T, double>)
                return FieldType::FLOAT64;
            else if constexpr (std::is_same_v<T, bool>)
                return FieldType::BOOL;
            else
                return FieldType::STRUCT;
        }
    } // namespace detail
} // namespace queen
