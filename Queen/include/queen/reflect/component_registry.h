#pragma once

#include <queen/reflect/reflectable.h>
#include <queen/reflect/component_serializer.h>
#include <queen/core/component_info.h>
#include <queen/core/type_id.h>
#include <hive/core/assert.h>
#include <cstddef>

namespace queen
{
    /**
     * Extended component metadata with reflection and serialization
     *
     * Combines ComponentMeta (lifecycle functions) with ComponentReflection
     * (field layout) for complete component handling.
     */
    struct RegisteredComponent
    {
        ComponentMeta meta;
        ComponentReflection reflection;

        [[nodiscard]] constexpr bool IsValid() const noexcept
        {
            return meta.IsValid();
        }

        [[nodiscard]] constexpr bool HasReflection() const noexcept
        {
            return reflection.IsValid();
        }
    };

    /**
     * Runtime component type registry
     *
     * Stores metadata for registered component types, enabling:
     * - Type lookup by TypeId at runtime
     * - Component serialization/deserialization
     * - Editor/inspector integration
     *
     * Components must be explicitly registered before use.
     * Registration captures both lifecycle functions and reflection data.
     *
     * Memory layout:
     * ┌────────────────────────────────────────────────────────────────┐
     * │ entries_: RegisteredComponent[MaxComponents]                   │
     * │ count_: size_t                                                 │
     * │ type_id_to_index_: TypeId[MaxComponents] (sorted for bsearch)  │
     * └────────────────────────────────────────────────────────────────┘
     *
     * Performance characteristics:
     * - Register: O(n) - maintains sorted order for binary search
     * - Lookup by TypeId: O(log n) - binary search
     * - Lookup by index: O(1) - array access
     *
     * Limitations:
     * - Fixed maximum component count (template parameter)
     * - Not thread-safe (register at startup only)
     * - TypeIds must be unique (hash collisions will assert)
     *
     * Example:
     * @code
     *   ComponentRegistry<128> registry;
     *
     *   // Register components at startup
     *   registry.Register<Position>();
     *   registry.Register<Velocity>();
     *
     *   // Lookup at runtime
     *   TypeId posId = TypeIdOf<Position>();
     *   const RegisteredComponent* info = registry.Find(posId);
     *   if (info && info->HasReflection()) {
     *       // Serialize component using reflection
     *   }
     * @endcode
     *
     * @tparam MaxComponents Maximum number of component types to support
     */
    template<size_t MaxComponents = 256>
    class ComponentRegistry
    {
    public:
        constexpr ComponentRegistry() noexcept = default;

        /**
         * Register a reflectable component type
         *
         * @tparam T Component type (must satisfy Reflectable concept)
         */
        template<Reflectable T>
        void Register() noexcept
        {
            hive::Assert(count_ < MaxComponents, "ComponentRegistry full");
            hive::Assert(Find(TypeIdOf<T>()) == nullptr, "Component already registered");

            RegisteredComponent entry;
            entry.meta = ComponentMeta::Of<T>();
            entry.reflection = GetReflectionData<T>();

            // Insert in sorted order by type_id for binary search
            size_t insert_pos = count_;
            for (size_t i = 0; i < count_; ++i)
            {
                if (entries_[i].meta.type_id > entry.meta.type_id)
                {
                    insert_pos = i;
                    break;
                }
            }

            // Shift elements to make room
            for (size_t i = count_; i > insert_pos; --i)
            {
                entries_[i] = entries_[i - 1];
            }

            entries_[insert_pos] = entry;
            ++count_;
        }

        /**
         * Register a non-reflectable component type (metadata only)
         *
         * For components that don't need serialization (e.g., tag components,
         * runtime-only components).
         *
         * @tparam T Component type
         */
        template<typename T>
        void RegisterWithoutReflection() noexcept
        {
            hive::Assert(count_ < MaxComponents, "ComponentRegistry full");
            hive::Assert(Find(TypeIdOf<T>()) == nullptr, "Component already registered");

            RegisteredComponent entry;
            entry.meta = ComponentMeta::Of<T>();
            // reflection stays default (invalid)

            // Insert in sorted order
            size_t insert_pos = count_;
            for (size_t i = 0; i < count_; ++i)
            {
                if (entries_[i].meta.type_id > entry.meta.type_id)
                {
                    insert_pos = i;
                    break;
                }
            }

            for (size_t i = count_; i > insert_pos; --i)
            {
                entries_[i] = entries_[i - 1];
            }

            entries_[insert_pos] = entry;
            ++count_;
        }

        /**
         * Find component by TypeId (binary search)
         *
         * @return Pointer to RegisteredComponent or nullptr if not found
         */
        [[nodiscard]] const RegisteredComponent* Find(TypeId type_id) const noexcept
        {
            if (count_ == 0) return nullptr;

            // Binary search
            size_t left = 0;
            size_t right = count_;

            while (left < right)
            {
                size_t mid = left + (right - left) / 2;
                TypeId mid_id = entries_[mid].meta.type_id;

                if (mid_id == type_id)
                {
                    return &entries_[mid];
                }
                else if (mid_id < type_id)
                {
                    left = mid + 1;
                }
                else
                {
                    right = mid;
                }
            }

            return nullptr;
        }

        /**
         * Find component by name (linear search)
         *
         * @return Pointer to RegisteredComponent or nullptr if not found
         */
        [[nodiscard]] const RegisteredComponent* FindByName(const char* name) const noexcept
        {
            if (name == nullptr) return nullptr;

            for (size_t i = 0; i < count_; ++i)
            {
                if (entries_[i].reflection.name != nullptr)
                {
                    // Simple string compare
                    const char* a = entries_[i].reflection.name;
                    const char* b = name;
                    bool equal = true;

                    while (*a && *b)
                    {
                        if (*a != *b)
                        {
                            equal = false;
                            break;
                        }
                        ++a;
                        ++b;
                    }

                    if (equal && *a == *b)
                    {
                        return &entries_[i];
                    }
                }
            }

            return nullptr;
        }

        /**
         * Get component by index
         */
        [[nodiscard]] const RegisteredComponent& operator[](size_t index) const noexcept
        {
            hive::Assert(index < count_, "Index out of bounds");
            return entries_[index];
        }

        /**
         * Get number of registered components
         */
        [[nodiscard]] size_t Count() const noexcept
        {
            return count_;
        }

        /**
         * Check if a component type is registered
         */
        [[nodiscard]] bool Contains(TypeId type_id) const noexcept
        {
            return Find(type_id) != nullptr;
        }

        /**
         * Check if a component type is registered
         */
        template<typename T>
        [[nodiscard]] bool Contains() const noexcept
        {
            return Contains(TypeIdOf<T>());
        }

        /**
         * Iterate over all registered components
         */
        [[nodiscard]] const RegisteredComponent* begin() const noexcept
        {
            return entries_;
        }

        [[nodiscard]] const RegisteredComponent* end() const noexcept
        {
            return entries_ + count_;
        }

        /**
         * Clear all registered components
         */
        void Clear() noexcept
        {
            count_ = 0;
        }

    private:
        RegisteredComponent entries_[MaxComponents]{};
        size_t count_ = 0;
    };
}
