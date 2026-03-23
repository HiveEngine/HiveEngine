#pragma once

#include <hive/hive_config.h>

#include <cstddef>

namespace comb
{
    [[nodiscard]] HIVE_API size_t GetPageSize();
    [[nodiscard]] HIVE_API void* AllocatePages(size_t size);
    HIVE_API void FreePages(void* ptr, size_t size);
} // namespace comb
