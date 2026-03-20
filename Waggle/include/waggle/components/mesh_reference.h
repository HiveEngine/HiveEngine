#pragma once

#include <wax/containers/fixed_string.h>

#include <queen/reflect/component_reflector.h>

namespace waggle
{

    struct MeshReference
    {
        wax::FixedString m_meshName{};
        wax::FixedString m_materialName{};
        uint32_t m_indexCount{0};
        int32_t m_meshIndex{-1};
        int32_t m_materialIndex{-1};

        static void Reflect(queen::ComponentReflector<>& r)
        {
            r.Field("mesh", &MeshReference::m_meshName);
            r.Field("material", &MeshReference::m_materialName);
            r.Field("indices", &MeshReference::m_indexCount).Flag(queen::FieldFlag::READ_ONLY);
            r.Field("mesh_index", &MeshReference::m_meshIndex).Flag(queen::FieldFlag::READ_ONLY);
            r.Field("material_index", &MeshReference::m_materialIndex).Flag(queen::FieldFlag::READ_ONLY);
        }
    };

} // namespace waggle
