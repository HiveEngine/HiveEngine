#include <nectar/pipeline/cooker_registry.h>

namespace nectar
{
    CookerRegistry::CookerRegistry(comb::DefaultAllocator& alloc)
        : m_alloc{&alloc}
        , m_typeMap{alloc, 32} {}

    void CookerRegistry::Register(IAssetCooker* cooker) {
        if (!cooker)
            return;

        auto name = cooker->TypeName();
        wax::String key{*m_alloc};
        key.Append(name.Data(), name.Size());

        auto* existing = m_typeMap.Find(key);
        if (existing)
            *existing = cooker;
        else
            m_typeMap.Insert(static_cast<wax::String&&>(key), cooker);
    }

    IAssetCooker* CookerRegistry::FindByType(wax::StringView typeName) const {
        wax::String key{*m_alloc};
        key.Append(typeName.Data(), typeName.Size());

        auto* found = m_typeMap.Find(key);
        return found ? *found : nullptr;
    }

    size_t CookerRegistry::Count() const noexcept {
        return m_typeMap.Count();
    }
} // namespace nectar
