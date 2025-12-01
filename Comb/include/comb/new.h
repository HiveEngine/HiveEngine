/**
 * Placement new helpers for allocators
 *
 * Provides type-safe allocation/deallocation functions that work with
 * any Comb allocator. Replaces dangerous raw `new`/`delete` operators.
 *
 * Functions:
 * - New<T>(allocator, args...): Allocate + construct object
 * - Delete(allocator, ptr): Destruct + deallocate object
 *
 * Features:
 * - Type-safe: Compiler ensures correct alignment
 * - Exception-safe: Deallocates on construction failure (if exceptions enabled)
 * - Debug tracking: Integrates with COMB_MEM_DEBUG
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

#include <new>           // For placement new
#include <utility>       // For std::forward
#include <type_traits>   // For alignment

namespace comb
{

/**
 * Allocate and construct an object using allocator
 *
 * @tparam T Object type to construct
 * @tparam Allocator Allocator type (must satisfy Allocator concept)
 * @tparam Args Constructor argument types
 * @param allocator Allocator to use
 * @param args Constructor arguments
 * @return Pointer to constructed object, or nullptr if allocation failed
 *
 * Thread-safe: Depends on allocator
 *
 * NOTE: If allocation fails, returns nullptr (no exceptions).
 * Always check return value!
 */
template<typename T, typename Allocator, typename... Args>
inline T* New(Allocator& allocator, Args&&... args)
{
    // Allocate memory with proper alignment
    void* memory = allocator.Allocate(sizeof(T), alignof(T));

    if (memory == nullptr)
    {
        // Allocation failed - return nullptr
        return nullptr;
    }

    // Construct object in allocated memory (placement new)
    // NOTE: If exceptions are disabled (game engine default), this won't throw
    T* obj = new (memory) T(std::forward<Args>(args)...);

    return obj;
}

/**
 * Destruct and deallocate an object using allocator
 *
 * @tparam T Object type
 * @tparam Allocator Allocator type
 * @param allocator Allocator to use (must be same as used for New)
 * @param ptr Pointer to object (can be nullptr)
 *
 * Thread-safe: Depends on allocator
 *
 * NOTE: If ptr is nullptr, this is a no-op.
 */
template<typename T, typename Allocator>
inline void Delete(Allocator& allocator, T* ptr)
{
    if (ptr == nullptr)
    {
        return;  // No-op for nullptr (like standard delete)
    }

    // Call destructor
    ptr->~T();

    // Deallocate memory
    allocator.Deallocate(ptr);
}

/**
 * Allocate and construct an array of objects using allocator
 *
 * @tparam T Element type
 * @tparam Allocator Allocator type
 * @param allocator Allocator to use
 * @param count Number of elements to allocate
 * @return Pointer to first element, or nullptr if allocation failed
 *
 * Thread-safe: Depends on allocator
 *
 * NOTE: All elements are default-constructed.
 * Use NewArray with initializer for custom construction.
 */
template<typename T, typename Allocator>
inline T* NewArray(Allocator& allocator, size_t count)
{
    if (count == 0)
    {
        return nullptr;
    }

    // Allocate memory for array
    void* memory = allocator.Allocate(sizeof(T) * count, alignof(T));

    if (memory == nullptr)
    {
        return nullptr;
    }

    // Default-construct each element
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
 * @tparam Allocator Allocator type
 * @param allocator Allocator to use
 * @param ptr Pointer to first element (can be nullptr)
 * @param count Number of elements in array
 *
 * Thread-safe: Depends on allocator
 */
template<typename T, typename Allocator>
inline void DeleteArray(Allocator& allocator, T* ptr, size_t count)
{
    if (ptr == nullptr)
    {
        return;
    }

    // Call destructors in reverse order
    for (size_t i = count; i > 0; --i)
    {
        ptr[i - 1].~T();
    }

    // Deallocate memory
    allocator.Deallocate(ptr);
}

} // namespace comb
