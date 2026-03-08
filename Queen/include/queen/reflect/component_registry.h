#pragma once

#include <hive/core/assert.h>

#include <queen/core/component_info.h>
#include <queen/core/type_id.h>
#include <queen/reflect/component_serializer.h>
#include <queen/reflect/reflectable.h>

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
        ComponentMeta m_meta;
        ComponentReflection m_reflection;
        const void* m_defaultValue = nullptr; // Snapshot of T{} for prefab diff

        [[nodiscard]] constexpr bool IsValid() const noexcept
        {
            return m_meta.IsValid();
        }

        [[nodiscard]] constexpr bool HasReflection() const noexcept
        {
            return m_reflection.IsValid();
        }

        [[nodiscard]] constexpr bool HasDefault() const noexcept
        {
            return m_defaultValue != nullptr;
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
    template <size_t MaxComponents = 256> class ComponentRegistry
    {
    public:
        constexpr ComponentRegistry() noexcept = default;

        /**
         * Register a reflectable component type
         *
         * @tparam T Component type (must satisfy Reflectable concept)
         */
        template <Reflectable T> void Register() noexcept
        {
            hive::Assert(m_count < MaxComponents, "ComponentRegistry full");
            hive::Assert(Find(TypeIdOf<T>()) == nullptr, "Component already registered");

            RegisteredComponent entry;
            entry.m_meta = ComponentMeta::Of<T>();
            entry.m_reflection = GetReflectionData<T>();

            // Capture default-constructed instance for prefab diff
            if constexpr (std::is_default_constructible_v<T>)
            {
                static const T DEFAULT_INSTANCE{};
                entry.m_defaultValue = &DEFAULT_INSTANCE;
            }

            // Insert in sorted order by type_id for binary search
            size_t insertPos = m_count;
            for (size_t i = 0; i < m_count; ++i)
            {
                if (m_entries[i].m_meta.m_typeId > entry.m_meta.m_typeId)
                {
                    insertPos = i;
                    break;
                }
            }

            // Shift elements to make room
            for (size_t i = m_count; i > insertPos; --i)
            {
                m_entries[i] = m_entries[i - 1];
            }

            m_entries[insertPos] = entry;
            ++m_count;
        }

        /**
         * Register a non-reflectable component type (metadata only)
         *
         * For components that don't need serialization (e.g., tag components,
         * runtime-only components).
         *
         * @tparam T Component type
         */
        template <typename T> void RegisterWithoutReflection() noexcept
        {
            hive::Assert(m_count < MaxComponents, "ComponentRegistry full");
            hive::Assert(Find(TypeIdOf<T>()) == nullptr, "Component already registered");

            RegisteredComponent entry;
            entry.m_meta = ComponentMeta::Of<T>();
            // reflection stays default (invalid)

            // Capture default-constructed instance
            if constexpr (std::is_default_constructible_v<T>)
            {
                static const T DEFAULT_INSTANCE{};
                entry.m_defaultValue = &DEFAULT_INSTANCE;
            }

            // Insert in sorted order
            size_t insertPos = m_count;
            for (size_t i = 0; i < m_count; ++i)
            {
                if (m_entries[i].m_meta.m_typeId > entry.m_meta.m_typeId)
                {
                    insertPos = i;
                    break;
                }
            }

            for (size_t i = m_count; i > insertPos; --i)
            {
                m_entries[i] = m_entries[i - 1];
            }

            m_entries[insertPos] = entry;
            ++m_count;
        }

        /**
         * Find component by TypeId (binary search)
         *
         * @return Pointer to RegisteredComponent or nullptr if not found
         */
        [[nodiscard]] const RegisteredComponent* Find(TypeId typeId) const noexcept
        {
            if (m_count == 0)
                return nullptr;

            // Binary search
            size_t left = 0;
            size_t right = m_count;

            while (left < right)
            {
                size_t mid = left + (right - left) / 2;
                TypeId midId = m_entries[mid].m_meta.m_typeId;

                if (midId == typeId)
                {
                    return &m_entries[mid];
                }
                else if (midId < typeId)
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
            if (name == nullptr)
                return nullptr;

            for (size_t i = 0; i < m_count; ++i)
            {
                if (detail::StringsEqual(m_entries[i].m_reflection.m_name, name))
                {
                    return &m_entries[i];
                }
            }

            return nullptr;
        }

        /**
         * Get component by index
         */
        [[nodiscard]] const RegisteredComponent& operator[](size_t index) const noexcept
        {
            hive::Assert(index < m_count, "Index out of bounds");
            return m_entries[index];
        }

        /**
         * Get number of registered components
         */
        [[nodiscard]] size_t Count() const noexcept
        {
            return m_count;
        }

        /**
         * Check if a component type is registered
         */
        [[nodiscard]] bool Contains(TypeId typeId) const noexcept
        {
            return Find(typeId) != nullptr;
        }

        /**
         * Check if a component type is registered
         */
        template <typename T> [[nodiscard]] bool Contains() const noexcept
        {
            return Contains(TypeIdOf<T>());
        }

        /**
         * Iterate over all registered components
         */
        [[nodiscard]] const RegisteredComponent* Begin() const noexcept
        {
            return m_entries;
        }

        [[nodiscard]] const RegisteredComponent* End() const noexcept
        {
            return m_entries + m_count;
        }

        /**
         * Default-construct a component into pre-allocated memory
         *
         * @param type_id TypeId of the component to construct
         * @param dst Pre-allocated memory (must be at least meta.size bytes, properly aligned)
         * @return true if constructed, false if type not found
         */
        [[nodiscard]] bool Construct(TypeId typeId, void* dst) const noexcept
        {
            const RegisteredComponent* comp = Find(typeId);
            if (comp == nullptr || comp->m_meta.m_construct == nullptr)
                return false;
            comp->m_meta.m_construct(dst);
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
        [[nodiscard]] bool Clone(TypeId typeId, void* dst, const void* src) const noexcept
        {
            const RegisteredComponent* comp = Find(typeId);
            if (comp == nullptr || comp->m_meta.m_copy == nullptr)
                return false;
            comp->m_meta.m_copy(dst, src);
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
        [[nodiscard]] uint64_t DiffWithDefault(TypeId typeId, const void* instance) const noexcept
        {
            const RegisteredComponent* comp = Find(typeId);
            if (comp == nullptr || comp->m_defaultValue == nullptr || !comp->HasReflection())
            {
                return ~uint64_t{0};
            }

            uint64_t mask = 0;
            const auto& refl = comp->m_reflection;
            for (size_t i = 0; i < refl.m_fieldCount && i < 64; ++i)
            {
                const FieldInfo& field = refl.m_fields[i];
                const auto* a = static_cast<const std::byte*>(instance) + field.m_offset;
                const auto* b = static_cast<const std::byte*>(comp->m_defaultValue) + field.m_offset;

                if (std::memcmp(a, b, field.m_size) != 0)
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
        [[nodiscard]] const void* GetDefault(TypeId typeId) const noexcept
        {
            const RegisteredComponent* comp = Find(typeId);
            if (comp == nullptr)
                return nullptr;
            return comp->m_defaultValue;
        }

        /**
         * Clear all registered components
         */
        void Clear() noexcept
        {
            m_count = 0;
        }

    private:
        RegisteredComponent m_entries[MaxComponents]{};
        size_t m_count = 0;
    };
} // namespace queen
