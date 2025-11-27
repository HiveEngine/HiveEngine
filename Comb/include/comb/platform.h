#pragma once

#include <cstddef>

namespace comb
{
    /**
     * Get the system page size
     * @return Page size in bytes (typically 4096)
     */
    size_t GetPageSize();

    /**
     * Allocate virtual memory pages from the OS
     * @param size Number of bytes to allocate (should be multiple of page size)
     * @return Pointer to allocated memory, or nullptr on failure
     */
    void* AllocatePages(size_t size);

    /**
     * Free virtual memory pages back to the OS
     * @param ptr Pointer to memory allocated by AllocatePages
     * @param size Size that was originally allocated
     */
    void FreePages(void* ptr, size_t size);
}
