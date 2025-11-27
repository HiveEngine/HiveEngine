#pragma once

#include <hive/core/assert.h>

#include <cstddef>
#include <cstdint>
#include <array>
#include <algorithm>
#include <concepts>

namespace comb
{
    /**
     * Check if a value is a power of 2
     * @param value Value to check
     * @return True if value is a power of 2, false otherwise
     */
    constexpr bool IsPowerOfTwo(size_t value)
    {
        return value > 0 && (value & (value - 1)) == 0;
    }

    /**
     * Check if an address is aligned to the specified alignment
     * @param address Address to check
     * @param alignment Alignment requirement (must be power of 2)
     * @return True if address is aligned, false otherwise
     */
    constexpr bool IsAligned(uintptr_t address, size_t alignment)
    {
        hive::Assert(IsPowerOfTwo(alignment), "Alignment must be power of 2");
        return (address & (alignment - 1)) == 0;
    }

    /**
     * Check if a pointer is aligned to the specified alignment
     * @param ptr Pointer to check
     * @param alignment Alignment requirement (must be power of 2)
     * @return True if pointer is aligned, false otherwise
     */
    inline bool IsAligned(const void* ptr, size_t alignment)
    {
        return IsAligned(reinterpret_cast<uintptr_t>(ptr), alignment);
    }

    /**
     * Round up a value to the next multiple of alignment
     * @param value Value to align
     * @param alignment Alignment requirement (must be power of 2)
     * @return Aligned value
     */
    constexpr size_t AlignUp(size_t value, size_t alignment)
    {
        hive::Assert(IsPowerOfTwo(alignment), "Alignment must be power of 2");
        return (value + (alignment - 1)) & ~(alignment - 1);
    }

    /**
     * Round up a pointer to the next multiple of alignment
     * @param ptr Pointer to align
     * @param alignment Alignment requirement (must be power of 2)
     * @return Aligned pointer
     */
    inline void* AlignUp(void* ptr, size_t alignment)
    {
        const auto addr = reinterpret_cast<uintptr_t>(ptr);
        const auto aligned = AlignUp(addr, alignment);
        return reinterpret_cast<void*>(aligned);
    }

    /**
     * Find the next power of 2 greater than or equal to value
     * @param value Input value
     * @return Next power of 2
     */
    constexpr size_t NextPowerOfTwo(size_t value)
    {
        if (value == 0) return 1;

        if (IsPowerOfTwo(value)) return value;

        value--;
        value |= value >> 1;
        value |= value >> 2;
        value |= value >> 4;
        value |= value >> 8;
        value |= value >> 16;

        if constexpr (sizeof(size_t) == 8)
        {
            value |= value >> 32;
        }

        return value + 1;
    }

    /**
     * Calculate padding needed to align an address
     * @param address Current address
     * @param alignment Alignment requirement (must be power of 2)
     * @return Number of bytes of padding needed
     */
    constexpr size_t GetAlignmentPadding(uintptr_t address, size_t alignment)
    {
        hive::Assert(IsPowerOfTwo(alignment), "Alignment must be power of 2");
        const auto padding = AlignUp(address, alignment) - address;
        return padding;
    }

    /**
     * Calculate padding needed to align a pointer
     * @param ptr Current pointer
     * @param alignment Alignment requirement (must be power of 2)
     * @return Number of bytes of padding needed
     */
    inline size_t GetAlignmentPadding(const void* ptr, size_t alignment)
    {
        return GetAlignmentPadding(reinterpret_cast<uintptr_t>(ptr), alignment);
    }

    /**
     * Create a constexpr array from variadic arguments
     * All arguments must be of the same type
     * @tparam T Element type
     * @tparam Ts... Additional element types (must be same as T)
     * @param n First element
     * @param ns... Remaining elements
     * @return constexpr array containing all elements
     */
    template<typename T, std::same_as<T>... Ts>
    constexpr std::array<T, sizeof...(Ts) + 1> MakeArray(T n, Ts... ns)
    {
        return {n, ns...};
    }

    /**
     * Check if a container is sorted in ascending order
     * @tparam T Container type
     * @param container Container to check
     * @return True if sorted, false otherwise
     */
    template<typename T>
    constexpr bool IsSorted(const T& container)
    {
        return std::is_sorted(std::begin(container), std::end(container));
    }
}
