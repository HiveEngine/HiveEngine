#pragma once

#include <queen/core/type_id.h>
#include <queen/reflect/field_info.h>
#include <hive/core/assert.h>
#include <cstddef>
#include <cstdint>
#include <type_traits>

namespace queen
{
    /**
     * A single name-value pair in an enum reflection
     */
    struct EnumEntry
    {
        const char* name = nullptr;
        int64_t value = 0;
    };

    /**
     * Type-erased enum reflection data
     *
     * Provides runtime name↔value mapping for reflected enums.
     * Stored as a pointer inside FieldInfo for FieldType::Enum fields.
     */
    struct EnumReflectionBase
    {
        const char* type_name = nullptr;
        TypeId type_id = 0;
        size_t underlying_size = 0;
        const EnumEntry* entries = nullptr;
        size_t entry_count = 0;

        /**
         * Get the name for a given enum value
         * @return Name string or nullptr if not found
         */
        [[nodiscard]] const char* NameOf(int64_t value) const noexcept
        {
            for (size_t i = 0; i < entry_count; ++i)
            {
                if (entries[i].value == value)
                {
                    return entries[i].name;
                }
            }
            return nullptr;
        }

        /**
         * Get the value for a given enum name
         * @return true if found, false otherwise
         */
        [[nodiscard]] bool ValueOf(const char* name, int64_t& out) const noexcept
        {
            if (name == nullptr) return false;

            for (size_t i = 0; i < entry_count; ++i)
            {
                if (detail::StringsEqual(entries[i].name, name))
                {
                    out = entries[i].value;
                    return true;
                }
            }
            return false;
        }

        [[nodiscard]] constexpr bool IsValid() const noexcept
        {
            return entries != nullptr && entry_count > 0;
        }
    };

    /**
     * Builder for enum reflection data
     *
     * Captures enum entries in a fixed-size array, then exposes them
     * via Base() as an EnumReflectionBase.
     *
     * Example:
     * @code
     *   template<> struct queen::EnumInfo<RenderMode> {
     *       static const EnumReflectionBase& Get() {
     *           static auto r = []() {
     *               EnumReflector<> e;
     *               e.Value("Opaque", RenderMode::Opaque);
     *               e.Value("Transparent", RenderMode::Transparent);
     *               return e;
     *           }();
     *           return r.Base();
     *       }
     *   };
     * @endcode
     *
     * @tparam MaxEntries Maximum number of enum entries to support
     */
    template<size_t MaxEntries = 32>
    class EnumReflector
    {
    public:
        constexpr EnumReflector() noexcept = default;

        /**
         * Register an enum value with its name
         */
        template<typename E>
        void Value(const char* name, E value) noexcept
        {
            static_assert(std::is_enum_v<E>, "Value() requires an enum type");
            hive::Assert(count_ < MaxEntries, "Too many enum entries, increase MaxEntries");

            entries_[count_].name = name;
            entries_[count_].value = static_cast<int64_t>(value);
            ++count_;

            // Update base on each insertion (so Base() is always valid)
            base_.entries = entries_;
            base_.entry_count = count_;
            base_.type_id = TypeIdOf<E>();
            base_.type_name = TypeNameOf<E>().data();
            base_.underlying_size = sizeof(std::underlying_type_t<E>);
        }

        /**
         * Get the type-erased reflection data
         */
        [[nodiscard]] const EnumReflectionBase& Base() const noexcept
        {
            return base_;
        }

        [[nodiscard]] size_t Count() const noexcept
        {
            return count_;
        }

    private:
        EnumEntry entries_[MaxEntries]{};
        size_t count_ = 0;
        EnumReflectionBase base_{};
    };

    /**
     * Extension point for enum reflection (template specialization)
     *
     * Users specialize this template for their enum types.
     * When C++26 reflection lands, the primary template will
     * auto-generate reflection from std::meta::enumerators_of(^E),
     * making specializations optional.
     *
     * Example:
     * @code
     *   enum class MyEnum : uint8_t { A, B, C };
     *
     *   template<> struct queen::EnumInfo<MyEnum> {
     *       static const EnumReflectionBase& Get() {
     *           static auto r = []() {
     *               EnumReflector<> e;
     *               e.Value("A", MyEnum::A);
     *               e.Value("B", MyEnum::B);
     *               e.Value("C", MyEnum::C);
     *               return e;
     *           }();
     *           return r.Base();
     *       }
     *   };
     * @endcode
     */
    template<typename E>
    struct EnumInfo;  // No default definition — must be specialized

    /**
     * Concept for reflectable enums
     *
     * An enum is ReflectableEnum if EnumInfo<E> is specialized with a Get() method.
     */
    template<typename E>
    concept ReflectableEnum = std::is_enum_v<E> && requires {
        { EnumInfo<E>::Get() } -> std::convertible_to<const EnumReflectionBase&>;
    };
}
