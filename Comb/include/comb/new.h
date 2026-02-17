/**
 * Placement new helpers for allocators
 *
 * Provides type-safe allocation/deallocation functions that work with
 * any Comb allocator. Replaces dangerous raw `new`/`delete` operators.
 *
 * Functions:
 * - New<T>(allocator, args...): Allocate + construct object
 * - Delete(allocator, ptr): Destruct + deallocate object
 * - NewArray<T>(allocator, count): Allocate + default-construct array
 * - DeleteArray(allocator, ptr, count): Destruct + deallocate array
 *
 * Features:
 * - Type-safe: Compiler ensures correct alignment
 * - Concept-constrained: Only works with comb::Allocator types
 * - Skips destructors for trivially destructible types
 * - Zero overhead: Inline functions, no vtables
 *
 * Example:
 * @code
 *   comb::LinearAllocator alloc{1024};
 *
 *   // Allocate + construct
 *   auto* obj = comb::New<MyClass>(alloc, arg1, arg2);
 *
 *   // Use object...
 *
 *   // Destruct + deallocate
 *   comb::Delete(alloc, obj);
 * @endcode
 */

#pragma once

#include <comb/allocator_concepts.h>
#include <new>           // For placement new
#include <utility>       // For std::forward
#include <type_traits>   // For is_trivially_destructible_v

namespace comb
{

/**
 * Allocate and construct an object using allocator
 *
 * @tparam T Object type to construct
 * @tparam Alloc Allocator type (must satisfy comb::Allocator concept)
 * @tparam Args Constructor argument types
 * @param allocator Allocator to use
 * @param args Constructor arguments
 * @return Pointer to constructed object, or nullptr if allocation failed
 */
template<typename T, Allocator Alloc, typename... Args>
[[nodiscard]] T* New(Alloc& allocator, Args&&... args)
{
    void* memory = allocator.Allocate(sizeof(T), alignof(T));

    if (memory == nullptr)
    {
        return nullptr;
    }

    return new (memory) T(std::forward<Args>(args)...);
}

/**
 * Destruct and deallocate an object using allocator
 *
 * @tparam T Object type
 * @tparam Alloc Allocator type (must satisfy comb::Allocator concept)
 * @param allocator Allocator to use (must be same as used for New)
 * @param ptr Pointer to object (nullptr is a no-op)
 */
template<typename T, Allocator Alloc>
void Delete(Alloc& allocator, T* ptr)
{
    if (ptr == nullptr)
    {
        return;
    }

    if constexpr (!std::is_trivially_destructible_v<T>)
    {
        ptr->~T();
    }

    allocator.Deallocate(ptr);
}

/**
 * Allocate and default-construct an array of objects using allocator
 *
 * @tparam T Element type
 * @tparam Alloc Allocator type (must satisfy comb::Allocator concept)
 * @param allocator Allocator to use
 * @param count Number of elements to allocate
 * @return Pointer to first element, or nullptr if allocation failed
 */
template<typename T, Allocator Alloc>
[[nodiscard]] T* NewArray(Alloc& allocator, size_t count)
{
    if (count == 0)
    {
        return nullptr;
    }

    void* memory = allocator.Allocate(sizeof(T) * count, alignof(T));

    if (memory == nullptr)
    {
        return nullptr;
    }

    T* array = static_cast<T*>(memory);
    for (size_t i = 0; i < count; ++i)
    {
        new (&array[i]) T();
    }

    return array;
}

/**
 * Destruct and deallocate an array of objects
 *
 * @tparam T Element type
 * @tparam Alloc Allocator type (must satisfy comb::Allocator concept)
 * @param allocator Allocator to use
 * @param ptr Pointer to first element (nullptr is a no-op)
 * @param count Number of elements in array
 */
template<typename T, Allocator Alloc>
void DeleteArray(Alloc& allocator, T* ptr, size_t count)
{
    if (ptr == nullptr)
    {
        return;
    }

    if constexpr (!std::is_trivially_destructible_v<T>)
    {
        for (size_t i = count; i > 0; --i)
        {
            ptr[i - 1].~T();
        }
    }

    allocator.Deallocate(ptr);
}

} // namespace comb
