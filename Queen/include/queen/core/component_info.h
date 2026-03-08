#pragma once

#include <queen/core/type_id.h>

#include <cstddef>
#include <new>
#include <type_traits>
#include <utility>

namespace queen
{
    /**
     * Storage type hint for components
     *
     * Components can declare their preferred storage type:
     * - Dense: Archetype/Table storage (default, cache-friendly iteration)
     * - Sparse: SparseSet storage (volatile components, fast add/remove)
     */
    enum class StorageType : uint8_t
    {
        DENSE,
        SPARSE
    };

    namespace detail
    {
        template <typename T, typename = void> struct HasStorageType : std::false_type
        {
        };

        template <typename T> struct HasStorageType<T, std::void_t<decltype(T::storage)>> : std::true_type
        {
        };

        template <typename T> constexpr StorageType DeduceStorage() noexcept {
            if constexpr (HasStorageType<T>::value)
            {
                return T::storage;
            }
            else
            {
                return StorageType::DENSE;
            }
        }
    } // namespace detail

    /**
     * Compile-time component type information
     *
     * Provides static metadata about a component type including:
     * - Type ID for runtime identification
     * - Size and alignment for memory allocation
     * - Trivial properties for optimization
     * - Storage hint for archetype vs sparse storage
     * - Lifecycle functions (construct, destruct, move, copy)
     *
     * Use cases:
     * - Column/Table type-erased storage
     * - Archetype creation and matching
     * - Component serialization
     *
     * Example:
     * @code
     *   using Info = ComponentInfo<Position>;
     *
     *   void* storage = allocator.Allocate(Info::size, Info::alignment);
     *   Info::Construct(storage);
     *   // ...
     *   Info::Destruct(storage);
     * @endcode
     */
    template <typename T> struct ComponentInfo
    {
        static constexpr TypeId id = TypeIdOf<T>();
        static constexpr size_t size = sizeof(T);
        static constexpr size_t alignment = alignof(T);
        static constexpr bool isTriviallyCopyable = std::is_trivially_copyable_v<T>;
        static constexpr bool isTriviallyDestructible = std::is_trivially_destructible_v<T>;
        inline static const StorageType storage = detail::DeduceStorage<T>();

        static void Construct(void* ptr) { new (ptr) T{}; }

        static void Destruct(void* ptr) { static_cast<T*>(ptr)->~T(); }

        static void Move(void* dst, void* src) { new (dst) T{std::move(*static_cast<T*>(src))}; }

        static void Copy(void* dst, const void* src) { new (dst) T{*static_cast<const T*>(src)}; }
    };

    /**
     * Runtime component metadata (type-erased)
     *
     * Stores component metadata in a non-template form for use in
     * Column, Table, and other type-erased containers.
     *
     * Memory layout:
     * ┌────────────────────────────────────────────────────────────┐
     * │ type_id: TypeId (8 bytes)                                  │
     * │ size: size_t (8 bytes)                                     │
     * │ alignment: size_t (8 bytes)                                │
     * │ storage: StorageType (1 byte) + padding (7 bytes)          │
     * │ construct: void(*)(void*) (8 bytes)                        │
     * │ destruct: void(*)(void*) (8 bytes)                         │
     * │ move: void(*)(void*, void*) (8 bytes)                      │
     * │ copy: void(*)(void*, const void*) (8 bytes)                │
     * └────────────────────────────────────────────────────────────┘
     *
     * Performance characteristics:
     * - All operations: O(1) - function pointer call
     *
     * Example:
     * @code
     *   ComponentMeta meta = ComponentMeta::Of<Position>();
     *   void* storage = allocator.Allocate(meta.size, meta.alignment);
     *   meta.construct(storage);
     * @endcode
     */
    struct ComponentMeta
    {
        using ConstructFn = void (*)(void*);
        using DestructFn = void (*)(void*);
        using MoveFn = void (*)(void* dst, void* src);
        using CopyFn = void (*)(void* dst, const void* src);

        TypeId m_typeId = 0;
        size_t m_size = 0;
        size_t m_alignment = 0;
        StorageType m_storage = StorageType::DENSE;
        ConstructFn m_construct = nullptr;
        DestructFn m_destruct = nullptr;
        MoveFn m_move = nullptr;
        CopyFn m_copy = nullptr;

        [[nodiscard]] constexpr bool IsValid() const noexcept { return m_typeId != 0 && m_size > 0; }

        [[nodiscard]] constexpr bool IsTrivial() const noexcept { return m_destruct == nullptr; }

        template <typename T> [[nodiscard]] static ComponentMeta Of() noexcept {
            using Info = ComponentInfo<T>;

            ComponentMeta meta{};
            meta.m_typeId = Info::id;
            meta.m_size = Info::size;
            meta.m_alignment = Info::alignment;
            meta.m_storage = Info::storage;

            if constexpr (std::is_default_constructible_v<T>)
            {
                meta.m_construct = &Info::Construct;
            }

            if constexpr (!std::is_trivially_destructible_v<T>)
            {
                meta.m_destruct = &Info::Destruct;
            }

            if constexpr (std::is_move_constructible_v<T> || std::is_copy_constructible_v<T>)
            {
                meta.m_move = &Info::Move;
            }

            if constexpr (std::is_copy_constructible_v<T>)
            {
                meta.m_copy = &Info::Copy;
            }

            return meta;
        }

        template <typename T> [[nodiscard]] static ComponentMeta OfTag() noexcept {
            ComponentMeta meta{};
            meta.m_typeId = TypeIdOf<T>();
            meta.m_size = 0;
            meta.m_alignment = 1;
            return meta;
        }
    };
} // namespace queen
