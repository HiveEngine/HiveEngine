#pragma once

#include <concepts>
#include <cstddef>
#include <type_traits>
#include <hive/core/assert.h>

namespace comb
{
    /**
     * Concept defining the requirements for an allocator type
     *
     * An allocator must provide:
     * - Allocate(size, alignment) -> void*
     * - Deallocate(ptr) -> void
     * - GetUsedMemory() -> size_t
     * - GetTotalMemory() -> size_t
     * - GetName() -> const char*
     */
    template<typename T>
    concept Allocator = requires(T allocator, size_t size, size_t alignment, void* ptr)
    {
        { allocator.Allocate(size, alignment) } -> std::convertible_to<void*>;
        { allocator.Deallocate(ptr) } -> std::same_as<void>;
        { allocator.GetUsedMemory() } -> std::convertible_to<size_t>;
        { allocator.GetTotalMemory() } -> std::convertible_to<size_t>;
        { allocator.GetName() } -> std::convertible_to<const char*>;
    };

    /**
     * Allocate and construct an object using an allocator
     *
     * Returns nullptr if allocation fails (graceful failure in release).
     * Asserts in debug if allocation fails.
     *
     * @tparam T Type to construct
     * @tparam Alloc Allocator type (must satisfy Allocator concept)
     * @tparam Args Constructor argument types
     * @param allocator The allocator to use
     * @param args Constructor arguments
     * @return Pointer to constructed object, or nullptr on failure
     *
     */
    template<typename T, Allocator Alloc, typename... Args>
    [[nodiscard]] T* New(Alloc& allocator, Args&&... args)
    {
        void* mem = allocator.Allocate(sizeof(T), alignof(T));
        hive::Assert(mem != nullptr, "Allocation failed in New<T>()");
        if (!mem) return nullptr;
        return new (mem) T{std::forward<Args>(args)...};
    }

    /**
     * Destroy and deallocate an object using an allocator
     *
     * IMPORTANT: For maximum performance, ptr MUST NOT be nullptr.
     * Caller is responsible for null-checking before calling Delete().
     *
     * In debug builds, asserts if ptr is nullptr.
     * In release builds, undefined behavior if ptr is nullptr (zero overhead).
     *
     * @tparam T Type to destroy
     * @tparam Alloc Allocator type (must satisfy Allocator concept)
     * @param allocator The allocator to use
     * @param ptr Pointer to object (MUST NOT be nullptr)
     *
     */
    template<typename T, Allocator Alloc>
    void Delete(Alloc& allocator, T* ptr)
    {
        hive::Assert(ptr != nullptr, "Delete() called with nullptr - check before calling");

        if constexpr (!std::is_trivially_destructible_v<T>)
        {
            ptr->~T();
        }

        allocator.Deallocate(ptr);
    }
}
