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

namespace nectar
{
    wax::Vector<GltfMaterialInfo> ParseGltfMaterials(wax::ByteSpan gltf_data,
                                                      comb::DefaultAllocator& alloc)
    {
        wax::Vector<GltfMaterialInfo> materials{alloc};

        cgltf_options options{};
        cgltf_data* data = nullptr;
        if (cgltf_parse(&options, gltf_data.Data(), gltf_data.Size(), &data) != cgltf_result_success)
            return materials;

        materials.Reserve(data->materials_count);

        for (cgltf_size i = 0; i < data->materials_count; ++i)
        {
            const auto& mat = data->materials[i];

            GltfMaterialInfo info{};
            info.material_index = static_cast<int32_t>(i);

            if (mat.has_pbr_metallic_roughness)
            {
                std::memcpy(info.base_color_factor,
                            mat.pbr_metallic_roughness.base_color_factor,
                            4 * sizeof(float));

                const auto& tex_view = mat.pbr_metallic_roughness.base_color_texture;
                if (tex_view.texture && tex_view.texture->image && tex_view.texture->image->uri)
                {
                    info.base_color_texture = wax::String<>{tex_view.texture->image->uri};
                }

                const auto& mr_view = mat.pbr_metallic_roughness.metallic_roughness_texture;
                if (mr_view.texture && mr_view.texture->image && mr_view.texture->image->uri)
                    info.metallic_roughness_texture = wax::String<>{mr_view.texture->image->uri};

                info.metallic_factor = mat.pbr_metallic_roughness.metallic_factor;
                info.roughness_factor = mat.pbr_metallic_roughness.roughness_factor;
            }

            if (mat.normal_texture.texture && mat.normal_texture.texture->image &&
                mat.normal_texture.texture->image->uri)
            {
                info.normal_texture = wax::String<>{mat.normal_texture.texture->image->uri};
            }

            if (mat.alpha_mode == cgltf_alpha_mode_mask)
                info.alpha_cutoff = mat.alpha_cutoff;
            info.double_sided = mat.double_sided != 0;

            materials.PushBack(std::move(info));
        }

        cgltf_free(data);
        return materials;
    }
}
