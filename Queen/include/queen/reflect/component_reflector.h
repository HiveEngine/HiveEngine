#pragma once

#include <queen/reflect/field_info.h>
#include <queen/reflect/field_attributes.h>
#include <queen/reflect/enum_reflection.h>
#include <queen/core/type_id.h>
#include <queen/core/entity.h>
#include <hive/core/assert.h>
#include <cstddef>
#include <type_traits>

namespace queen
{
    // Forward declaration
    template<size_t MaxFields>
    class ComponentReflector;

    namespace detail
    {
        // Detect wax::FixedString without including the header.
        // FixedString has: static constexpr size_t MaxCapacity = 22, CStr(), Size()
        template<typename T>
        concept IsFixedString = requires(const T& s) {
            { T::MaxCapacity } -> std::convertible_to<size_t>;
            { s.CStr() } -> std::same_as<const char*>;
            { s.Size() } -> std::same_as<size_t>;
        } && (sizeof(T) == 24) && std::is_trivially_copyable_v<T>;
    }

    /**
     * Chaining builder for field annotations
     *
     * Returned by ComponentReflector::Field(). Allows chaining editor
     * attributes without requiring a separate registration step.
     * Return value is safely discardable (backward-compatible).
     *
     * Example:
     * @code
     *   r.Field("speed", &T::speed).Range(0.f, 100.f).Tooltip("Movement speed");
     *   r.Field("name", &T::name);  // no chaining — zero cost
     * @endcode
     */
    class FieldBuilder
    {
    public:
        constexpr FieldBuilder(FieldInfo& info, FieldAttributes& attrs) noexcept
            : info_{info}
            , attrs_{attrs}
        {}

        FieldBuilder& Range(float min, float max, float step = 0.f) noexcept
        {
            EnsureAttributes();
            attrs_.min = min;
            attrs_.max = max;
            attrs_.step = step;
            return *this;
        }

        FieldBuilder& Tooltip(const char* text) noexcept
        {
            EnsureAttributes();
            attrs_.tooltip = text;
            return *this;
        }

        FieldBuilder& Category(const char* cat) noexcept
        {
            EnsureAttributes();
            attrs_.category = cat;
            return *this;
        }

        FieldBuilder& DisplayName(const char* name) noexcept
        {
            EnsureAttributes();
            attrs_.display_name = name;
            return *this;
        }

        FieldBuilder& Flag(FieldFlag flag) noexcept
        {
            EnsureAttributes();
            attrs_.flags |= static_cast<uint32_t>(flag);
            return *this;
        }

    private:
        void EnsureAttributes() noexcept
        {
            if (info_.attributes == nullptr)
            {
                info_.attributes = &attrs_;
            }
        }

        FieldInfo& info_;
        FieldAttributes& attrs_;
    };

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
     * Performance characteristics:
     * - Field registration: O(1) - array append
     * - Field lookup by name: O(n) - linear search
     * - Field lookup by index: O(1) - array access
     *
     * Limitations:
     * - Fixed maximum field count (template parameter)
     * - No support for inheritance
     *
     * Example:
     * @code
     *   struct Position {
     *       float x, y, z;
     *
     *       static void Reflect(ComponentReflector<>& r) {
     *           r.Field("x", &Position::x);
     *           r.Field("y", &Position::y);
     *           r.Field("z", &Position::z);
     *       }
     *   };
     *
     *   struct Enemy {
     *       float speed;
     *       RenderMode mode;
     *
     *       static void Reflect(ComponentReflector<>& r) {
     *           r.Field("speed", &Enemy::speed).Range(0.f, 100.f);
     *           r.Field("mode", &Enemy::mode);
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
         * Returns a FieldBuilder for optional chaining of editor annotations.
         * The return value can safely be discarded.
         *
         * @param name Field name (must be string literal or static storage)
         * @param member_ptr Pointer-to-member for the field
         * @return FieldBuilder for chaining annotations
         */
        template<typename T, typename C>
        FieldBuilder Field(const char* name, T C::* member_ptr) noexcept
        {
            hive::Assert(count_ < MaxFields, "Too many fields, increase MaxFields");

            size_t idx = count_++;
            FieldInfo& info = fields_[idx];
            info.name = name;
            info.offset = GetMemberOffset(member_ptr);
            info.size = sizeof(T);

            // Type deduction
            if constexpr (std::is_same_v<T, Entity>)
            {
                info.type = FieldType::Entity;
            }
            else if constexpr (std::is_enum_v<T>)
            {
                info.type = FieldType::Enum;

                // Auto-lookup EnumInfo<T> if specialized
                if constexpr (ReflectableEnum<T>)
                {
                    info.enum_info = &EnumInfo<T>::Get();
                }
            }
            else if constexpr (detail::IsFixedString<T>)
            {
                info.type = FieldType::String;
            }
            else if constexpr (std::is_array_v<T>)
            {
                info.type = FieldType::FixedArray;
                info.element_count = std::extent_v<T>;
                info.element_type = detail::GetFieldType<std::remove_extent_t<T>>();
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
                    // Holder avoids copying the reflector (which would leave
                    // FieldInfo::attributes pointers dangling).
                    struct NestedHolder
                    {
                        ComponentReflector<MaxFields> reflector;
                        NestedHolder() { T::Reflect(reflector); }
                    };
                    static NestedHolder nested;
                    info.nested_fields = nested.reflector.Data();
                    info.nested_field_count = nested.reflector.Count();
                }
            }

            return FieldBuilder{info, attributes_[idx]};
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
        FieldAttributes attributes_[MaxFields]{};
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
