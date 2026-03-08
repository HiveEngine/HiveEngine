#pragma once

#include <comb/default_allocator.h>

#include <wax/containers/string.h>

#include <nectar/hive/hive_document.h>

namespace nectar
{
    /// Serializes a HiveDocument back to .hive text format.
    class HiveWriter
    {
    public:
        [[nodiscard]] static wax::String Write(const HiveDocument& doc, comb::DefaultAllocator& alloc);
    };
} // namespace nectar
