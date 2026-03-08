#pragma once

#include <comb/default_allocator.h>

#include <wax/containers/string.h>
#include <wax/containers/string_view.h>
#include <wax/containers/vector.h>

#include <nectar/hive/hive_document.h>

namespace nectar
{
    struct HiveParseError
    {
        size_t m_line{0};
        wax::String m_message{};
    };

    struct HiveParseResult
    {
        HiveDocument m_document;
        wax::Vector<HiveParseError> m_errors;

        [[nodiscard]] bool Success() const noexcept { return m_errors.IsEmpty(); }
    };

    /// Parses .hive file text into a HiveDocument.
    /// Best-effort: collects errors without stopping.
    class HiveParser
    {
    public:
        [[nodiscard]] static HiveParseResult Parse(wax::StringView content, comb::DefaultAllocator& alloc);
    };
} // namespace nectar
