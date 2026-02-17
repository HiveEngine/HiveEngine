#pragma once

#include <queen/reflect/field_info.h>
#include <queen/core/type_id.h>
#include <queen/core/entity.h>
#include <hive/core/assert.h>
#include <cstddef>
#include <type_traits>

namespace queen
{
    /**
     * Component field registration builder
     *
     * Used by components in their static Reflect() function to register
     * their fields for serialization. Captures field layout at compile-time
     * and stores it in a fixed-size array.
     *
     * This is an intermediate solution while waiting for C++26 reflection.
     * Components must define a static Reflect() function that uses this class.
     *
     * Memory layout:
     * ┌────────────────────────────────────────────────────────────────┐
     * │ fields_: FieldInfo[MaxFields] (MaxFields * 40 bytes)           │
     * │ count_: size_t (8 bytes)                                       │
     * └────────────────────────────────────────────────────────────────┘
     *
     * Performance characteristics:
     * - Field registration: O(1) - array append
     * - Field lookup by name: O(n) - linear search
     * - Field lookup by index: O(1) - array access
     *
     * Limitations:
     * - Fixed maximum field count (template parameter)
     * - No support for arrays/vectors (yet)
     * - No support for inheritance
     *
     * Example:
     * @code
     *   struct Position {
     *       float x, y, z;
     *
     *       static void Reflect(ComponentReflector& r) {
     *           r.Field("x", &Position::x);
     *           r.Field("y", &Position::y);
     *           r.Field("z", &Position::z);
     *       }
     *   };
     * @endcode
     *
     * @tparam MaxFields Maximum number of fields to support (default 32)
     */
    template<size_t MaxFields = 32>
    class ComponentReflector
    {
    public:
        constexpr ComponentReflector() noexcept = default;

        /**
         * Register a field with automatic type deduction
         *
         * @param name Field name (must be string literal or static storage)
         * @param member_ptr Pointer-to-member for the field
         */
        template<typename T, typename C>
        void Field(const char* name, T C::* member_ptr) noexcept
        {
            hive::Assert(count_ < MaxFields, "Too many fields, increase MaxFields");

            FieldInfo& info = fields_[count_++];
            info.name = name;
            info.offset = GetMemberOffset(member_ptr);
            info.size = sizeof(T);

            // Special case for Entity
            if constexpr (std::is_same_v<T, Entity>)
            {
                info.type = FieldType::Entity;
            }
            else
            {
                info.type = detail::GetFieldType<T>();
            }

            // For struct types, store the nested type ID and reflection data if available
            if (info.type == FieldType::Struct)
            {
                info.nested_type_id = TypeIdOf<T>();

                // If the nested type is Reflectable, capture its field layout
                if constexpr (requires(ComponentReflector<MaxFields>& r) { T::Reflect(r); })
                {
                    static ComponentReflector<MaxFields> nested = []() {
                        ComponentReflector<MaxFields> r;
                        T::Reflect(r);
                        return r;
                    }();
                    info.nested_fields = nested.Data();
                    info.nested_field_count = nested.Count();
                }
            }
        }

        /**
         * Get number of registered fields
         */
        [[nodiscard]] constexpr size_t Count() const noexcept
        {
            return count_;
        }

        /**
         * Get field info by index
         */
        [[nodiscard]] constexpr const FieldInfo& operator[](size_t index) const noexcept
        {
            return fields_[index];
        }

        /**
         * Get field info by name (linear search)
         *
         * @return Pointer to FieldInfo or nullptr if not found
         */
        [[nodiscard]] constexpr const FieldInfo* FindField(const char* name) const noexcept
        {
            for (size_t i = 0; i < count_; ++i)
            {
                if (detail::StringsEqual(fields_[i].name, name))
                {
                    return &fields_[i];
                }
            }
            return nullptr;
        }

        /**
         * Get pointer to fields array
         */
        [[nodiscard]] constexpr const FieldInfo* Data() const noexcept
        {
            return fields_;
        }

        /**
         * Iterate over all fields
         */
        [[nodiscard]] constexpr const FieldInfo* begin() const noexcept
        {
            return fields_;
        }

        [[nodiscard]] constexpr const FieldInfo* end() const noexcept
        {
            return fields_ + count_;
        }

    private:
        template<typename T, typename C>
        static size_t GetMemberOffset(T C::* member_ptr) noexcept
        {
            // Use stack-allocated storage to avoid nullptr dereference UB
            alignas(C) unsigned char storage[sizeof(C)]{};
            return static_cast<size_t>(
                reinterpret_cast<const unsigned char*>(
                    &(reinterpret_cast<C*>(storage)->*member_ptr)
                ) - storage
            );
        }

        FieldInfo fields_[MaxFields]{};
        size_t count_ = 0;
    };

    /**
     * Type-erased reflection data
     *
     * Stores reflection metadata in a non-template form for use in
     * ComponentRegistry and serialization.
     */
    struct ComponentReflection
    {
        const FieldInfo* fields = nullptr;
        size_t field_count = 0;
        TypeId type_id = 0;
        const char* name = nullptr;

        [[nodiscard]] constexpr bool IsValid() const noexcept
        {
            return type_id != 0 && fields != nullptr;
        }

        [[nodiscard]] constexpr const FieldInfo* FindField(const char* field_name) const noexcept
        {
            for (size_t i = 0; i < field_count; ++i)
            {
                if (detail::StringsEqual(fields[i].name, field_name))
                {
                    return &fields[i];
                }
            }
            return nullptr;
        }

        [[nodiscard]] constexpr const FieldInfo* begin() const noexcept
        {
            return fields;
        }

        [[nodiscard]] constexpr const FieldInfo* end() const noexcept
        {
            return fields + field_count;
        }
    };
}
