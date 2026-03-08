#pragma once

#include <comb/default_allocator.h>

#include <wax/containers/string.h>
#include <wax/containers/string_view.h>
#include <wax/serialization/byte_buffer.h>
#include <wax/serialization/byte_span.h>

namespace nectar
{
    struct CookResult
    {
        bool m_success{false};
        wax::ByteBuffer m_cookedData{};
        wax::String m_errorMessage{};
    };

    struct CookContext
    {
        wax::StringView m_platform; // "pc", "ps5", "switch"
        comb::DefaultAllocator* m_alloc;
    };

    /// Type-erased base for asset cookers.
    /// Converts intermediate format to platform-optimized format.
    class IAssetCooker
    {
    public:
        virtual ~IAssetCooker() = default;

        /// Asset type this cooker handles (e.g. "Texture", "Mesh").
        [[nodiscard]] virtual wax::StringView TypeName() const = 0;

        /// Cooker version. Incrementing invalidates all cooked results.
        [[nodiscard]] virtual uint32_t Version() const = 0;

        /// Cook intermediate data into platform-optimized format.
        virtual CookResult Cook(wax::ByteSpan intermediateData, const CookContext& context) = 0;
    };
} // namespace nectar
