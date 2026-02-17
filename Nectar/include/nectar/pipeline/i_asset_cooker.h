#pragma once

#include <wax/serialization/byte_buffer.h>
#include <wax/serialization/byte_span.h>
#include <wax/containers/string.h>
#include <wax/containers/string_view.h>
#include <comb/default_allocator.h>

namespace nectar
{
    struct CookResult
    {
        bool success{false};
        wax::ByteBuffer<comb::DefaultAllocator> cooked_data{};
        wax::String<> error_message{};
    };

    struct CookContext
    {
        wax::StringView platform;       // "pc", "ps5", "switch"
        comb::DefaultAllocator* alloc;
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
        virtual CookResult Cook(wax::ByteSpan intermediate_data,
                                const CookContext& context) = 0;
    };
}
