#pragma once

#include <queen/reflect/reflectable.h>
#include <queen/reflect/component_serializer.h>
#include <queen/core/component_info.h>
#include <queen/core/type_id.h>
#include <hive/core/assert.h>
#include <cstddef>
#include <cstring>

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
        const void* default_value = nullptr;   // Snapshot of T{} for prefab diff

        [[nodiscard]] constexpr bool IsValid() const noexcept
        {
            return meta.IsValid();
        }

        [[nodiscard]] constexpr bool HasReflection() const noexcept
        {
            return reflection.IsValid();
        }

        [[nodiscard]] constexpr bool HasDefault() const noexcept
        {
            return default_value != nullptr;
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

            // Capture default-constructed instance for prefab diff
            if constexpr (std::is_default_constructible_v<T>)
            {
                static const T default_instance{};
                entry.default_value = &default_instance;
            }

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

            // Capture default-constructed instance
            if constexpr (std::is_default_constructible_v<T>)
            {
                static const T default_instance{};
                entry.default_value = &default_instance;
            }

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
                if (detail::StringsEqual(entries_[i].reflection.name, name))
                {
                    return &entries_[i];
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
         * Default-construct a component into pre-allocated memory
         *
         * @param type_id TypeId of the component to construct
         * @param dst Pre-allocated memory (must be at least meta.size bytes, properly aligned)
         * @return true if constructed, false if type not found
         */
        [[nodiscard]] bool Construct(TypeId type_id, void* dst) const noexcept
        {
            const RegisteredComponent* comp = Find(type_id);
            if (comp == nullptr || comp->meta.construct == nullptr) return false;
            comp->meta.construct(dst);
            return true;
        }

        /**
         * Clone a component from src to dst
         *
         * @param type_id TypeId of the component
         * @param dst Destination memory (must be properly allocated)
         * @param src Source component
         * @return true if cloned, false if type not found
         */
        [[nodiscard]] bool Clone(TypeId type_id, void* dst, const void* src) const noexcept
        {
            const RegisteredComponent* comp = Find(type_id);
            if (comp == nullptr || comp->meta.copy == nullptr) return false;
            comp->meta.copy(dst, src);
            return true;
        }

        /**
         * Compare a component instance with its registered default value
         *
         * Returns a bitmask where bit N is set if field N differs from the default.
         * Supports up to 64 fields. Returns ~0 if no default is available.
         *
         * @param type_id TypeId of the component
         * @param instance Pointer to the component instance
         * @return Bitmask of changed fields (0 = all match default)
         */
        [[nodiscard]] uint64_t DiffWithDefault(TypeId type_id, const void* instance) const noexcept
        {
            const RegisteredComponent* comp = Find(type_id);
            if (comp == nullptr || comp->default_value == nullptr || !comp->HasReflection())
            {
                return ~uint64_t{0};
            }

            uint64_t mask = 0;
            const auto& refl = comp->reflection;
            for (size_t i = 0; i < refl.field_count && i < 64; ++i)
            {
                const FieldInfo& field = refl.fields[i];
                const auto* a = static_cast<const std::byte*>(instance) + field.offset;
                const auto* b = static_cast<const std::byte*>(comp->default_value) + field.offset;

                if (std::memcmp(a, b, field.size) != 0)
                {
                    mask |= (uint64_t{1} << i);
                }
            }
            return mask;
        }

        /**
         * Get the default value snapshot for a component type
         *
         * @return Pointer to default-constructed instance, or nullptr
         */
        [[nodiscard]] const void* GetDefault(TypeId type_id) const noexcept
        {
            const RegisteredComponent* comp = Find(type_id);
            if (comp == nullptr) return nullptr;
            return comp->default_value;
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
