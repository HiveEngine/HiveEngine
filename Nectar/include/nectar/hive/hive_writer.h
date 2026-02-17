#pragma once

#include <nectar/hive/hive_document.h>
#include <wax/containers/string.h>
#include <comb/default_allocator.h>

namespace nectar
{
    /// Serializes a HiveDocument back to .hive text format.
    class HiveWriter
    {
    public:
        [[nodiscard]] static wax::String<> Write(const HiveDocument& doc, comb::DefaultAllocator& alloc);
    };
}
