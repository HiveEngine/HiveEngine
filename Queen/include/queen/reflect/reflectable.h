#pragma once

#include <queen/reflect/component_reflector.h>
#include <concepts>
#include <type_traits>

namespace queen
{
    /**
     * Concept for reflectable components
     *
     * A component is Reflectable if it defines a static Reflect() function
     * that takes a ComponentReflector reference and registers its fields.
     *
     * This is the core constraint for serializable components. All components
     * that need serialization MUST satisfy this concept.
     *
     * Preparation for C++26 native reflection - when available, this concept
     * will be updated to use compiler-generated reflection instead.
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
     *   static_assert(Reflectable<Position>);
     * @endcode
     */
    template<typename T>
    concept Reflectable = requires(ComponentReflector<32>& reflector) {
        { T::Reflect(reflector) } -> std::same_as<void>;
    };

    /**
     * Helper to get reflection data for a type
     *
     * Calls the component's Reflect() function and returns the
     * captured reflection data.
     *
     * @tparam T The component type (must satisfy Reflectable)
     * @tparam MaxFields Maximum number of fields (default 32)
     * @return ComponentReflector with registered fields
     */
    template<Reflectable T, size_t MaxFields = 32>
    constexpr ComponentReflector<MaxFields> GetReflection() noexcept
    {
        ComponentReflector<MaxFields> reflector;
        T::Reflect(reflector);
        return reflector;
    }

    /**
     * Get type-erased reflection data
     *
     * Returns a ComponentReflection struct that can be stored
     * in a registry without template parameters.
     *
     * Note: The returned struct points to static storage, so it's
     * safe to store long-term.
     *
     * @tparam T The component type (must satisfy Reflectable)
     * @return ComponentReflection with type-erased field data
     */
    template<Reflectable T>
    ComponentReflection GetReflectionData() noexcept
    {
        // Static storage for reflection data (initialized once on first call)
        static ComponentReflector<32> reflector = []() {
            ComponentReflector<32> r;
            T::Reflect(r);
            return r;
        }();

        ComponentReflection result;
        result.fields = reflector.Data();
        result.field_count = reflector.Count();
        result.type_id = TypeIdOf<T>();
        result.name = TypeNameOf<T>().data();
        return result;
    }

    /**
     * Check if a type is reflectable at compile-time
     */
    template<typename T>
    struct IsReflectable : std::bool_constant<Reflectable<T>> {};

    template<typename T>
    inline constexpr bool IsReflectable_v = IsReflectable<T>::value;
}
