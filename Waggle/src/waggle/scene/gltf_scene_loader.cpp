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

#include <hive/core/log.h>
#include <hive/math/math.h>

#include <wax/containers/string.h>

#include <queen/world/world.h>

#include <nectar/material/material_data.h>
#include <nectar/material/material_serializer.h>
#include <nectar/mesh/gltf_material.h>

#include <waggle/components/camera.h>
#include <waggle/components/lighting.h>
#include <waggle/components/mesh_reference.h>
#include <waggle/components/name.h>
#include <waggle/components/transform.h>
#include <waggle/scene/gltf_scene_loader.h>

#include <cmath>
#include <cstdio>
#include <cstring>
#include <filesystem>

namespace waggle
{

    static const hive::LogCategory LOG_SCENE{"Waggle.GltfSceneLoader"};

    struct SceneLoadContext
    {
        queen::World& world;
        GltfSceneLoadResult& result;
        cgltf_data* gltfData;
    };

    // Decompose a column-major 4x4 matrix into TRS.
    // glTF stores matrices column-major, matching our Mat4 layout.
    static void DecomposeMatrix(const float m[16], hive::math::Float3& pos, hive::math::Quat& rot,
                                hive::math::Float3& scale)
    {
        // Column vectors
        float c0[3] = {m[0], m[1], m[2]};
        float c1[3] = {m[4], m[5], m[6]};
        float c2[3] = {m[8], m[9], m[10]};

        scale.m_x = std::sqrt(c0[0] * c0[0] + c0[1] * c0[1] + c0[2] * c0[2]);
        scale.m_y = std::sqrt(c1[0] * c1[0] + c1[1] * c1[1] + c1[2] * c1[2]);
        scale.m_z = std::sqrt(c2[0] * c2[0] + c2[1] * c2[1] + c2[2] * c2[2]);

        // Normalize columns to get rotation matrix
        float r00 = c0[0] / scale.m_x, r10 = c0[1] / scale.m_x, r20 = c0[2] / scale.m_x;
        float r01 = c1[0] / scale.m_y, r11 = c1[1] / scale.m_y, r21 = c1[2] / scale.m_y;
        float r02 = c2[0] / scale.m_z, r12 = c2[1] / scale.m_z, r22 = c2[2] / scale.m_z;

        // Rotation matrix to quaternion (Shepperd's method)
        float trace = r00 + r11 + r22;
        if (trace > 0.f)
        {
            float s = std::sqrt(trace + 1.f) * 2.f;
            rot.m_w = 0.25f * s;
            rot.m_x = (r21 - r12) / s;
            rot.m_y = (r02 - r20) / s;
            rot.m_z = (r10 - r01) / s;
        }
        else if (r00 > r11 && r00 > r22)
        {
            float s = std::sqrt(1.f + r00 - r11 - r22) * 2.f;
            rot.m_w = (r21 - r12) / s;
            rot.m_x = 0.25f * s;
            rot.m_y = (r01 + r10) / s;
            rot.m_z = (r02 + r20) / s;
        }
        else if (r11 > r22)
        {
            float s = std::sqrt(1.f + r11 - r00 - r22) * 2.f;
            rot.m_w = (r02 - r20) / s;
            rot.m_x = (r01 + r10) / s;
            rot.m_y = 0.25f * s;
            rot.m_z = (r12 + r21) / s;
        }
        else
        {
            float s = std::sqrt(1.f + r22 - r00 - r11) * 2.f;
            rot.m_w = (r10 - r01) / s;
            rot.m_x = (r02 + r20) / s;
            rot.m_y = (r12 + r21) / s;
            rot.m_z = 0.25f * s;
        }

        pos.m_x = m[12];
        pos.m_y = m[13];
        pos.m_z = m[14];
    }

    static queen::Entity LoadNode(SceneLoadContext& ctx, const cgltf_node* node, uint32_t nodeIndex)
    {
        Transform transform{};

        if (node->has_matrix)
        {
            DecomposeMatrix(node->matrix, transform.m_position, transform.m_rotation, transform.m_scale);
        }
        else
        {
            if (node->has_translation)
            {
                transform.m_position = {node->translation[0], node->translation[1], node->translation[2]};
            }
            if (node->has_rotation)
            {
                transform.m_rotation = {node->rotation[0], node->rotation[1], node->rotation[2], node->rotation[3]};
            }
            if (node->has_scale)
            {
                transform.m_scale = {node->scale[0], node->scale[1], node->scale[2]};
            }
        }

        // Build a name: use node name if available, otherwise "Node_N"
        char nameBuf[24];
        if (node->name && node->name[0] != '\0')
        {
            std::snprintf(nameBuf, sizeof(nameBuf), "%s", node->name);
        }
        else
        {
            std::snprintf(nameBuf, sizeof(nameBuf), "Node_%u", nodeIndex);
        }

        queen::Entity entity =
            ctx.world.Spawn(Name{wax::FixedString{nameBuf}}, std::move(transform), WorldMatrix{}, TransformVersion{});
        ++ctx.result.m_entityCount;

        if (node->mesh)
        {
            auto meshIndex = static_cast<int32_t>(node->mesh - ctx.gltfData->meshes);

            wax::FixedString meshName{};
            if (node->mesh->name && node->mesh->name[0] != '\0')
            {
                meshName = wax::FixedString{node->mesh->name};
            }
            else
            {
                char fallback[24];
                std::snprintf(fallback, sizeof(fallback), "Mesh_%u", static_cast<uint32_t>(meshIndex));
                meshName = wax::FixedString{fallback};
            }

            for (cgltf_size pi = 0; pi < node->mesh->primitives_count; ++pi)
            {
                const cgltf_primitive& prim = node->mesh->primitives[pi];

                char primName[24];
                std::snprintf(primName, sizeof(primName), "%.*s_prim%u", 16, meshName.CStr(),
                              static_cast<uint32_t>(pi));

                MeshReference meshRef{};
                meshRef.m_meshName = meshName;
                meshRef.m_indexCount = prim.indices ? static_cast<uint32_t>(prim.indices->count) : 0;
                meshRef.m_meshIndex = meshIndex;

                if (prim.material)
                {
                    auto matIdx = static_cast<int32_t>(prim.material - ctx.gltfData->materials);
                    meshRef.m_materialIndex = matIdx;
                    if (prim.material->name && prim.material->name[0] != '\0')
                    {
                        meshRef.m_materialName = wax::FixedString{prim.material->name};
                    }
                    else
                    {
                        char fallback[24];
                        std::snprintf(fallback, sizeof(fallback), "Material_%d", matIdx);
                        meshRef.m_materialName = wax::FixedString{fallback};
                    }
                }

                queen::Entity primEntity = ctx.world.Spawn(Name{wax::FixedString{primName}}, Transform{}, WorldMatrix{},
                                                           TransformVersion{}, std::move(meshRef));
                ctx.world.SetParent(primEntity, entity);
                ++ctx.result.m_entityCount;
                ++ctx.result.m_meshCount;
            }
        }

        if (node->camera)
        {
            if (node->camera->type == cgltf_camera_type_perspective)
            {
                const auto& persp = node->camera->data.perspective;
                Camera cam{};
                cam.m_fovRad = persp.yfov;
                cam.m_zNear = persp.znear;
                cam.m_zFar = persp.has_zfar ? persp.zfar : 1000.f;
                ctx.world.Add(entity, std::move(cam));
                ++ctx.result.m_cameraCount;
            }
        }

        if (node->light)
        {
            if (node->light->type == cgltf_light_type_directional)
            {
                DirectionalLight light{};
                light.m_color = {node->light->color[0], node->light->color[1], node->light->color[2]};
                light.m_intensity = node->light->intensity;
                ctx.world.Add(entity, std::move(light));
                ++ctx.result.m_lightCount;
            }
            else if (node->light->type == cgltf_light_type_point || node->light->type == cgltf_light_type_spot)
            {
                hive::LogInfo(LOG_SCENE, "Skipping unsupported light type on '{}'", nameBuf);
            }
        }

        for (cgltf_size ci = 0; ci < node->children_count; ++ci)
        {
            auto childNodeIndex = static_cast<uint32_t>(node->children[ci] - ctx.gltfData->nodes);
            queen::Entity child = LoadNode(ctx, node->children[ci], childNodeIndex);
            ctx.world.SetParent(child, entity);
        }

        return entity;
    }

    GltfSceneLoadResult LoadGltfScene(queen::World& world, const void* data, size_t dataSize, wax::StringView basePath,
                                      comb::DefaultAllocator& alloc)
    {
        GltfSceneLoadResult result{};

        cgltf_options options{};
        cgltf_data* gltfData = nullptr;
        cgltf_result res = cgltf_parse(&options, data, dataSize, &gltfData);
        if (res != cgltf_result_success)
        {
            hive::LogError(LOG_SCENE, "glTF parse failed");
            return result;
        }

        wax::String pathStr{basePath};
        if (!pathStr.IsEmpty())
        {
            res = cgltf_load_buffers(&options, gltfData, pathStr.CStr());
        }
        else
        {
            res = cgltf_load_buffers(&options, gltfData, nullptr);
        }
        if (res != cgltf_result_success)
        {
            cgltf_free(gltfData);
            hive::LogError(LOG_SCENE, "glTF buffer load failed");
            return result;
        }

        SceneLoadContext ctx{world, result, gltfData};

        cgltf_size sceneIdx = gltfData->scene ? static_cast<cgltf_size>(gltfData->scene - gltfData->scenes) : 0;
        if (sceneIdx < gltfData->scenes_count)
        {
            const cgltf_scene& scene = gltfData->scenes[sceneIdx];
            for (cgltf_size ni = 0; ni < scene.nodes_count; ++ni)
            {
                auto nodeIndex = static_cast<uint32_t>(scene.nodes[ni] - gltfData->nodes);
                LoadNode(ctx, scene.nodes[ni], nodeIndex);
            }
        }

        // Export .hmat files for each material
        {
            wax::ByteSpan gltfSpan{static_cast<const std::byte*>(data), dataSize};
            auto materials = nectar::ParseGltfMaterials(gltfSpan, alloc);

            wax::String dirPath{pathStr};
            auto slashPos = dirPath.RFind('/');
            auto backslashPos = dirPath.RFind('\\');
            size_t lastSlash = wax::String::npos;
            if (slashPos != wax::String::npos && backslashPos != wax::String::npos)
                lastSlash = slashPos > backslashPos ? slashPos : backslashPos;
            else if (slashPos != wax::String::npos)
                lastSlash = slashPos;
            else
                lastSlash = backslashPos;
            if (lastSlash != wax::String::npos)
            {
                dirPath = wax::String{dirPath.View().Substr(0, lastSlash)};
            }

            for (size_t mi = 0; mi < materials.Size(); ++mi)
            {
                const auto& src = materials[mi];

                nectar::MaterialData matData{};
                matData.m_name = src.m_name;
                std::memcpy(matData.m_baseColorFactor, src.m_baseColorFactor, sizeof(matData.m_baseColorFactor));
                matData.m_metallicFactor = src.m_metallicFactor;
                matData.m_roughnessFactor = src.m_roughnessFactor;
                matData.m_doubleSided = src.m_doubleSided;
                // Texture GUIDs are resolved during import (gltf_import_job), not here

                if (src.m_alphaCutoff > 0.f)
                {
                    matData.m_alphaCutoff = src.m_alphaCutoff;
                    matData.m_blendMode = nectar::MaterialData::BlendMode::ALPHA_TEST;
                }

                wax::String matName{};
                if (src.m_name.IsEmpty())
                {
                    char indexBuf[32];
                    std::snprintf(indexBuf, sizeof(indexBuf), "Material_%zu", mi);
                    matName = wax::String{indexBuf};
                }
                else
                {
                    matName = wax::String{src.m_name.View()};
                }
                wax::String matPath = dirPath + "/" + matName + ".hmat";

                std::error_code fsEc;
                if (std::filesystem::exists(matPath.CStr(), fsEc))
                    continue;

                if (nectar::SaveMaterial(matData, matPath.View(), alloc))
                {
                    hive::LogInfo(LOG_SCENE, "Saved material '{}' -> {}", matName.CStr(), matPath.CStr());
                }
                else
                {
                    hive::LogError(LOG_SCENE, "Failed to save material '{}'", matName.CStr());
                }
            }
        }

        cgltf_free(gltfData);

        result.m_success = true;
        hive::LogInfo(LOG_SCENE, "Loaded glTF scene: {} entities, {} meshes, {} cameras, {} lights",
                      result.m_entityCount, result.m_meshCount, result.m_cameraCount, result.m_lightCount);
        return result;
    }

} // namespace waggle
