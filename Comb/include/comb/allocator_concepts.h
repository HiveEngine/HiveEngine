#pragma once

#include <concepts>
#include <cstddef>

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
}
