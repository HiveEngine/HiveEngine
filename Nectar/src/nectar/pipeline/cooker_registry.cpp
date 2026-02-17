#include <nectar/pipeline/cooker_registry.h>

namespace nectar
{
    CookerRegistry::CookerRegistry(comb::DefaultAllocator& alloc)
        : alloc_{&alloc}
        , type_map_{alloc, 32}
    {}

    void CookerRegistry::Register(IAssetCooker* cooker)
    {
        if (!cooker) return;

        auto name = cooker->TypeName();
        wax::String<> key{*alloc_};
        key.Append(name.Data(), name.Size());

        auto* existing = type_map_.Find(key);
        if (existing)
            *existing = cooker;
        else
            type_map_.Insert(static_cast<wax::String<>&&>(key), cooker);
    }

    IAssetCooker* CookerRegistry::FindByType(wax::StringView type_name) const
    {
        wax::String<> key{*alloc_};
        key.Append(type_name.Data(), type_name.Size());

        auto* found = type_map_.Find(key);
        return found ? *found : nullptr;
    }

    size_t CookerRegistry::Count() const noexcept
    {
        return type_map_.Count();
    }
}
