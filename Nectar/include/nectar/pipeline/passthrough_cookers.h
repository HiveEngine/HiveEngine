#pragma once

#include <comb/default_allocator.h>

#include <wax/serialization/byte_buffer.h>
#include <wax/serialization/byte_span.h>

#include <nectar/pipeline/i_asset_cooker.h>

namespace nectar
{

    class PassthroughMeshCooker final : public IAssetCooker
    {
    public:
        wax::StringView TypeName() const override
        {
            return "Mesh";
        }
        uint32_t Version() const override
        {
            return 1;
        }
        CookResult Cook(wax::ByteSpan data, const CookContext& ctx) override
        {
            CookResult r;
            r.m_success = true;
            r.m_cookedData = wax::ByteBuffer{*ctx.m_alloc};
            r.m_cookedData.Append(data.Data(), data.Size());
            return r;
        }
    };

    class PassthroughTextureCooker final : public IAssetCooker
    {
    public:
        wax::StringView TypeName() const override
        {
            return "Texture";
        }
        uint32_t Version() const override
        {
            return 1;
        }
        CookResult Cook(wax::ByteSpan data, const CookContext& ctx) override
        {
            CookResult r;
            r.m_success = true;
            r.m_cookedData = wax::ByteBuffer{*ctx.m_alloc};
            r.m_cookedData.Append(data.Data(), data.Size());
            return r;
        }
    };

} // namespace nectar
