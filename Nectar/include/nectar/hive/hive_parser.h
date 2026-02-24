#pragma once

#include <nectar/hive/hive_document.h>
#include <wax/containers/string.h>
#include <wax/containers/string_view.h>
#include <wax/containers/vector.h>
#include <comb/default_allocator.h>

namespace nectar
{
    struct HiveParseError
    {
        size_t line{0};
        wax::String<> message{};
    };

    struct HiveParseResult
    {
        HiveDocument document;
        wax::Vector<HiveParseError> errors;

        [[nodiscard]] bool Success() const noexcept { return errors.IsEmpty(); }
    };

    /// Parses .hive file text into a HiveDocument.
    /// Best-effort: collects errors without stopping.
    class HiveParser
    {
    public:
        [[nodiscard]] static HiveParseResult Parse(wax::StringView content, comb::DefaultAllocator& alloc);
    };
}
