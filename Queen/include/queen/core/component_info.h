#pragma once

#include <queen/core/type_id.h>
#include <cstddef>
#include <new>
#include <cstring>

namespace queen
{
    /**
     * Type-erased component metadata
     *
     * Stores size, alignment, and function pointers for type-erased operations
     * on components (construct, destruct, move, copy). Used by Column and Table
     * to manage heterogeneous component storage without templates.
     *
     * Memory layout:
     * ┌────────────────────────────────────────────────────────────┐
     * │ type_id: TypeId (8 bytes)                                  │
     * │ size: size_t (8 bytes)                                     │
     * │ alignment: size_t (8 bytes)                                │
     * │ construct: void(*)(void*) (8 bytes)                        │
     * │ destruct: void(*)(void*) (8 bytes)                         │
     * │ move: void(*)(void*, void*) (8 bytes)                      │
     * │ copy: void(*)(void*, const void*) (8 bytes)                │
     * └────────────────────────────────────────────────────────────┘
     *
     * Performance characteristics:
     * - All operations: O(1) - function pointer call
     * - Memory: 56 bytes per ComponentInfo
     *
     * Limitations:
     * - Requires trivially copyable or properly implemented move/copy
     * - No RTTI - relies on compile-time type information
     *
     * Example:
     * @code
     *   ComponentInfo info = ComponentInfo::Of<Position>();
     *
     *   void* storage = allocator.Allocate(info.size, info.alignment);
     *   info.construct(storage);  // Default construct
     *   info.destruct(storage);   // Destruct
     * @endcode
     */
    struct ComponentInfo
    {
        using ConstructFn = void(*)(void*);
        using DestructFn = void(*)(void*);
        using MoveFn = void(*)(void* dst, void* src);
        using CopyFn = void(*)(void* dst, const void* src);

        TypeId type_id = 0;
        size_t size = 0;
        size_t alignment = 0;
        ConstructFn construct = nullptr;
        DestructFn destruct = nullptr;
        MoveFn move = nullptr;
        CopyFn copy = nullptr;

        [[nodiscard]] constexpr bool IsValid() const noexcept
        {
            return type_id != 0 && size > 0;
        }

        [[nodiscard]] constexpr bool IsTrivial() const noexcept
        {
            return destruct == nullptr;
        }

        template<typename T>
        [[nodiscard]] static constexpr ComponentInfo Of() noexcept
        {
            ComponentInfo info;
            info.type_id = TypeIdOf<T>();
            info.size = sizeof(T);
            info.alignment = alignof(T);

            if constexpr (std::is_default_constructible_v<T>)
            {
                info.construct = [](void* ptr) {
                    new (ptr) T{};
                };
            }

            if constexpr (!std::is_trivially_destructible_v<T>)
            {
                info.destruct = [](void* ptr) {
                    static_cast<T*>(ptr)->~T();
                };
            }

            if constexpr (std::is_move_constructible_v<T>)
            {
                info.move = [](void* dst, void* src) {
                    new (dst) T{std::move(*static_cast<T*>(src))};
                };
            }
            else if constexpr (std::is_copy_constructible_v<T>)
            {
                info.move = [](void* dst, void* src) {
                    new (dst) T{*static_cast<T*>(src)};
                };
            }

            if constexpr (std::is_copy_constructible_v<T>)
            {
                info.copy = [](void* dst, const void* src) {
                    new (dst) T{*static_cast<const T*>(src)};
                };
            }

            return info;
        }

        template<typename T>
        [[nodiscard]] static constexpr ComponentInfo OfTag() noexcept
        {
            ComponentInfo info;
            info.type_id = TypeIdOf<T>();
            info.size = 0;
            info.alignment = 1;
            return info;
        }
    };

    static_assert(sizeof(ComponentInfo) == 56, "ComponentInfo should be 56 bytes");
}
