#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wold-style-cast"
#pragma clang diagnostic ignored "-Wsign-conversion"
#pragma clang diagnostic ignored "-Wimplicit-float-conversion"
#pragma clang diagnostic ignored "-Wdouble-promotion"
#pragma clang diagnostic ignored "-Wshadow"
#pragma clang diagnostic ignored "-Wextra-semi-stmt"
#pragma clang diagnostic ignored "-Wmissing-field-initializers"
#pragma clang diagnostic ignored "-Wcast-align"
#include <cgltf.h>
#pragma clang diagnostic pop

#include <nectar/mesh/gltf_material.h>

#include <cstring>
#include <utility>

namespace nectar
{
    wax::Vector<GltfMaterialInfo> ParseGltfMaterials(wax::ByteSpan gltfData, comb::DefaultAllocator& alloc)
    {
        wax::Vector<GltfMaterialInfo> materials{alloc};

        cgltf_options options{};
        cgltf_data* data = nullptr;
        if (cgltf_parse(&options, gltfData.Data(), gltfData.Size(), &data) != cgltf_result_success)
        {
            return materials;
        }

        materials.Reserve(data->materials_count);

        for (cgltf_size i = 0; i < data->materials_count; ++i)
        {
            const auto& mat = data->materials[i];

            GltfMaterialInfo info{};
            info.m_materialIndex = static_cast<int32_t>(i);
            if (mat.name)
                info.m_name = wax::String{mat.name};

            if (mat.has_pbr_metallic_roughness)
            {
                std::memcpy(info.m_baseColorFactor, mat.pbr_metallic_roughness.base_color_factor, 4 * sizeof(float));

                const auto& texView = mat.pbr_metallic_roughness.base_color_texture;
                if (texView.texture && texView.texture->image && texView.texture->image->uri)
                {
                    info.m_baseColorTexture = wax::String{texView.texture->image->uri};
                }

                const auto& mrView = mat.pbr_metallic_roughness.metallic_roughness_texture;
                if (mrView.texture && mrView.texture->image && mrView.texture->image->uri)
                {
                    info.m_metallicRoughnessTexture = wax::String{mrView.texture->image->uri};
                }

                info.m_metallicFactor = mat.pbr_metallic_roughness.metallic_factor;
                info.m_roughnessFactor = mat.pbr_metallic_roughness.roughness_factor;
            }

            if (mat.normal_texture.texture && mat.normal_texture.texture->image &&
                mat.normal_texture.texture->image->uri)
            {
                info.m_normalTexture = wax::String{mat.normal_texture.texture->image->uri};
            }

            if (mat.alpha_mode == cgltf_alpha_mode_mask)
            {
                info.m_alphaCutoff = mat.alpha_cutoff;
            }
            info.m_doubleSided = mat.double_sided != 0;

            materials.PushBack(std::move(info));
        }

        cgltf_free(data);
        return materials;
    }
} // namespace nectar
