#include <hive/math/types.h>

#include <queen/core/type_id.h>
#include <queen/reflect/component_registry.h>
#include <queen/reflect/field_attributes.h>
#include <queen/reflect/field_info.h>
#include <queen/world/world.h>

#include <forge/inspector_panel.h>
#include <forge/selection.h>
#include <forge/undo.h>

#include <imgui.h>

#include <cmath>
#include <cstdio>
#include <cstring>

namespace forge
{
    namespace
    {
        struct DragState
        {
            queen::Entity m_entity{};
            queen::TypeId m_typeId{0};
            uint16_t m_offset{0};
            uint16_t m_size{0};
            std::byte m_before[64]{};
            bool m_active{false};
        };

        DragState g_sDrag{};

        void BeginDrag(queen::Entity entity, queen::TypeId typeId, uint16_t offset, uint16_t size, const void* current)
        {
            if (g_sDrag.m_active)
            {
                return;
            }

            g_sDrag.m_entity = entity;
            g_sDrag.m_typeId = typeId;
            g_sDrag.m_offset = offset;
            g_sDrag.m_size = size;
            if (size <= sizeof(g_sDrag.m_before))
            {
                std::memcpy(g_sDrag.m_before, current, size);
            }
            g_sDrag.m_active = true;
        }

        void EndDrag(UndoStack& undo, const void* current)
        {
            if (!g_sDrag.m_active)
            {
                return;
            }

            g_sDrag.m_active = false;
            if (std::memcmp(g_sDrag.m_before, current, g_sDrag.m_size) != 0)
            {
                undo.PushSetField(g_sDrag.m_entity, g_sDrag.m_typeId, g_sDrag.m_offset, g_sDrag.m_size,
                                  g_sDrag.m_before, current);
            }
        }

        [[nodiscard]] const char* GetFieldDisplayName(const queen::FieldInfo& field) noexcept
        {
            if (field.m_attributes != nullptr && field.m_attributes->m_displayName != nullptr)
            {
                return field.m_attributes->m_displayName;
            }

            return field.m_name;
        }

        [[nodiscard]] bool HasFlag(const queen::FieldInfo& field, queen::FieldFlag flag) noexcept
        {
            return field.m_attributes != nullptr && field.m_attributes->HasFlag(flag);
        }

        bool DrawFloat3Widget(const char* label, void* data, const queen::FieldInfo& field, queen::Entity entity,
                              queen::TypeId typeId, uint16_t baseOffset, UndoStack& undo)
        {
            auto* value = static_cast<float*>(data);
            const uint16_t offset = static_cast<uint16_t>(baseOffset + field.m_offset);

            if (HasFlag(field, queen::FieldFlag::COLOR))
            {
                if (ImGui::ColorEdit3(label, value))
                {
                    if (ImGui::IsItemActivated())
                    {
                        BeginDrag(entity, typeId, offset, 12, value);
                    }
                    return true;
                }
                if (ImGui::IsItemDeactivatedAfterEdit())
                {
                    EndDrag(undo, value);
                }
                return false;
            }

            float speed = 0.01f;
            if (field.m_attributes != nullptr && field.m_attributes->HasRange())
            {
                speed = (field.m_attributes->m_max - field.m_attributes->m_min) / 500.f;
            }

            if (ImGui::DragFloat3(label, value, speed))
            {
                if (ImGui::IsItemActivated())
                {
                    BeginDrag(entity, typeId, offset, 12, value);
                }
                return true;
            }
            if (ImGui::IsItemDeactivatedAfterEdit())
            {
                EndDrag(undo, value);
            }
            return false;
        }

        bool DrawQuatWidget(const char* label, void* data, queen::Entity entity, queen::TypeId typeId,
                            uint16_t baseOffset, const queen::FieldInfo& field, UndoStack& undo)
        {
            auto* q = static_cast<float*>(data);
            const uint16_t offset = static_cast<uint16_t>(baseOffset + field.m_offset);

            const float sinr = 2.f * (q[3] * q[0] + q[1] * q[2]);
            const float cosr = 1.f - 2.f * (q[0] * q[0] + q[1] * q[1]);
            const float pitch = std::atan2(sinr, cosr) * (180.f / 3.14159265f);

            const float sinp = 2.f * (q[3] * q[1] - q[2] * q[0]);
            float yaw = 0.f;
            if (std::fabs(sinp) >= 1.f)
            {
                yaw = std::copysign(90.f, sinp);
            }
            else
            {
                yaw = std::asin(sinp) * (180.f / 3.14159265f);
            }

            const float siny = 2.f * (q[3] * q[2] + q[0] * q[1]);
            const float cosy = 1.f - 2.f * (q[1] * q[1] + q[2] * q[2]);
            const float roll = std::atan2(siny, cosy) * (180.f / 3.14159265f);

            float euler[3] = {pitch, yaw, roll};
            bool changed = false;

            if (ImGui::DragFloat3(label, euler, 0.5f))
            {
                if (ImGui::IsItemActivated())
                {
                    BeginDrag(entity, typeId, offset, 16, q);
                }

                const float p = euler[0] * (3.14159265f / 180.f) * 0.5f;
                const float y = euler[1] * (3.14159265f / 180.f) * 0.5f;
                const float r = euler[2] * (3.14159265f / 180.f) * 0.5f;

                const float cp = std::cos(p);
                const float sp = std::sin(p);
                const float cy = std::cos(y);
                const float sy = std::sin(y);
                const float cr = std::cos(r);
                const float sr = std::sin(r);

                q[0] = sr * cp * cy - cr * sp * sy;
                q[1] = cr * sp * cy + sr * cp * sy;
                q[2] = cr * cp * sy - sr * sp * cy;
                q[3] = cr * cp * cy + sr * sp * sy;
                changed = true;
            }

            if (ImGui::IsItemDeactivatedAfterEdit())
            {
                EndDrag(undo, q);
            }

            return changed;
        }

        bool DrawField(const queen::FieldInfo& field, void* componentData, queen::Entity entity, queen::TypeId typeId,
                       uint16_t baseOffset, UndoStack& undo)
        {
            if (HasFlag(field, queen::FieldFlag::HIDDEN))
            {
                return false;
            }

            const char* label = GetFieldDisplayName(field);
            void* fieldData = static_cast<std::byte*>(componentData) + field.m_offset;
            const uint16_t offset = static_cast<uint16_t>(baseOffset + field.m_offset);
            const bool readOnly = HasFlag(field, queen::FieldFlag::READ_ONLY);
            bool changed = false;

            if (readOnly)
            {
                ImGui::BeginDisabled();
            }

            switch (field.m_type)
            {
                case queen::FieldType::FLOAT32: {
                    auto* value = static_cast<float*>(fieldData);
                    float speed = 0.01f;
                    float minValue = 0.f;
                    float maxValue = 0.f;
                    if (field.m_attributes != nullptr && field.m_attributes->HasRange())
                    {
                        minValue = field.m_attributes->m_min;
                        maxValue = field.m_attributes->m_max;
                        speed = (maxValue - minValue) / 500.f;
                    }

                    if (HasFlag(field, queen::FieldFlag::ANGLE))
                    {
                        changed = ImGui::SliderAngle(
                            label, value,
                            field.m_attributes ? field.m_attributes->m_min * (180.f / 3.14159265f) : -360.f,
                            field.m_attributes ? field.m_attributes->m_max * (180.f / 3.14159265f) : 360.f);
                    }
                    else
                    {
                        changed = ImGui::DragFloat(label, value, speed, minValue, maxValue);
                    }

                    if (ImGui::IsItemActivated())
                    {
                        BeginDrag(entity, typeId, offset, static_cast<uint16_t>(field.m_size), fieldData);
                    }
                    if (ImGui::IsItemDeactivatedAfterEdit())
                    {
                        EndDrag(undo, fieldData);
                    }
                    break;
                }

                case queen::FieldType::FLOAT64: {
                    auto* value = static_cast<double*>(fieldData);
                    float floatValue = static_cast<float>(*value);
                    if (ImGui::DragFloat(label, &floatValue, 0.01f))
                    {
                        *value = static_cast<double>(floatValue);
                        changed = true;
                    }
                    if (ImGui::IsItemActivated())
                    {
                        BeginDrag(entity, typeId, offset, static_cast<uint16_t>(field.m_size), fieldData);
                    }
                    if (ImGui::IsItemDeactivatedAfterEdit())
                    {
                        EndDrag(undo, fieldData);
                    }
                    break;
                }

                case queen::FieldType::INT32: {
                    auto* value = static_cast<int32_t*>(fieldData);
                    int minValue = 0;
                    int maxValue = 0;
                    if (field.m_attributes != nullptr && field.m_attributes->HasRange())
                    {
                        minValue = static_cast<int>(field.m_attributes->m_min);
                        maxValue = static_cast<int>(field.m_attributes->m_max);
                    }
                    changed = ImGui::DragInt(label, value, 1.f, minValue, maxValue);
                    if (ImGui::IsItemActivated())
                    {
                        BeginDrag(entity, typeId, offset, static_cast<uint16_t>(field.m_size), fieldData);
                    }
                    if (ImGui::IsItemDeactivatedAfterEdit())
                    {
                        EndDrag(undo, fieldData);
                    }
                    break;
                }

                case queen::FieldType::UINT32: {
                    auto* value = static_cast<uint32_t*>(fieldData);
                    int intValue = static_cast<int>(*value);
                    if (ImGui::DragInt(label, &intValue, 1.f, 0, 0))
                    {
                        *value = static_cast<uint32_t>(intValue);
                        changed = true;
                    }
                    if (ImGui::IsItemActivated())
                    {
                        BeginDrag(entity, typeId, offset, static_cast<uint16_t>(field.m_size), fieldData);
                    }
                    if (ImGui::IsItemDeactivatedAfterEdit())
                    {
                        EndDrag(undo, fieldData);
                    }
                    break;
                }

                case queen::FieldType::BOOL: {
                    auto* value = static_cast<bool*>(fieldData);
                    const bool before = *value;
                    if (ImGui::Checkbox(label, value))
                    {
                        undo.PushSetField(entity, typeId, offset, static_cast<uint16_t>(field.m_size), &before, value);
                        changed = true;
                    }
                    break;
                }

                case queen::FieldType::STRUCT: {
                    if (field.m_size == sizeof(hive::math::Float3) &&
                        field.m_nestedTypeId == queen::TypeIdOf<hive::math::Float3>())
                    {
                        changed = DrawFloat3Widget(label, fieldData, field, entity, typeId, baseOffset, undo);
                    }
                    else if (field.m_size == sizeof(hive::math::Quat) &&
                             field.m_nestedTypeId == queen::TypeIdOf<hive::math::Quat>())
                    {
                        changed = DrawQuatWidget(label, fieldData, entity, typeId, baseOffset, field, undo);
                    }
                    else if (field.m_nestedFields != nullptr && field.m_nestedFieldCount > 0)
                    {
                        if (ImGui::TreeNode(label))
                        {
                            for (size_t i = 0; i < field.m_nestedFieldCount; ++i)
                            {
                                changed |= DrawField(field.m_nestedFields[i], fieldData, entity, typeId,
                                                     static_cast<uint16_t>(baseOffset + field.m_offset), undo);
                            }
                            ImGui::TreePop();
                        }
                    }
                    else
                    {
                        ImGui::TextDisabled("%s (opaque)", label);
                    }
                    break;
                }

                case queen::FieldType::ENUM: {
                    if (field.m_enumInfo != nullptr && field.m_enumInfo->IsValid())
                    {
                        int64_t currentValue = 0;
                        std::memcpy(&currentValue, fieldData, field.m_size <= 8 ? field.m_size : 8);

                        const char* currentName = field.m_enumInfo->NameOf(currentValue);
                        if (currentName == nullptr)
                        {
                            currentName = "???";
                        }

                        if (ImGui::BeginCombo(label, currentName))
                        {
                            for (size_t i = 0; i < field.m_enumInfo->m_entryCount; ++i)
                            {
                                const auto& entry = field.m_enumInfo->m_entries[i];
                                const bool selected = entry.m_value == currentValue;
                                if (ImGui::Selectable(entry.m_name, selected))
                                {
                                    std::byte before[8]{};
                                    std::memcpy(before, fieldData, field.m_size);
                                    std::memcpy(fieldData, &entry.m_value, field.m_size);
                                    undo.PushSetField(entity, typeId, offset, static_cast<uint16_t>(field.m_size),
                                                      before, fieldData);
                                    changed = true;
                                }
                                if (selected)
                                {
                                    ImGui::SetItemDefaultFocus();
                                }
                            }
                            ImGui::EndCombo();
                        }
                    }
                    else
                    {
                        ImGui::TextDisabled("%s (enum, no info)", label);
                    }
                    break;
                }

                default:
                    ImGui::TextDisabled("%s (unsupported type %d)", label, static_cast<int>(field.m_type));
                    break;
            }

            if (readOnly)
            {
                ImGui::EndDisabled();
            }

            if (field.m_attributes != nullptr && field.m_attributes->m_tooltip != nullptr && ImGui::IsItemHovered())
            {
                ImGui::SetTooltip("%s", field.m_attributes->m_tooltip);
            }

            return changed;
        }

        bool DrawComponent(queen::World& world, queen::Entity entity, queen::TypeId typeId, void* componentData,
                           const queen::ComponentReflection& reflection, UndoStack& undo)
        {
            IM_UNUSED(world);

            const char* typeName = reflection.m_name != nullptr ? reflection.m_name : "Component";
            bool changed = false;

            const ImGuiTreeNodeFlags headerFlags =
                ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_Framed | ImGuiTreeNodeFlags_AllowOverlap;

            if (ImGui::CollapsingHeader(typeName, headerFlags))
            {
                ImGui::PushID(static_cast<int>(typeId));
                ImGui::Indent(4.f);

                for (size_t i = 0; i < reflection.m_fieldCount; ++i)
                {
                    changed |= DrawField(reflection.m_fields[i], componentData, entity, typeId, 0, undo);
                }

                ImGui::Unindent(4.f);
                ImGui::PopID();
            }

            return changed;
        }
    } // namespace

    bool DrawInspectorPanel(queen::World& world, EditorSelection& selection,
                            const queen::ComponentRegistry<256>& registry, UndoStack& undo)
    {
        const queen::Entity entity = selection.Primary();
        if (entity.IsNull() || !world.IsAlive(entity))
        {
            ImGui::TextDisabled("No entity selected");
            return false;
        }

        ImGui::Text("Entity %u", entity.Index());
        ImGui::Separator();
        bool changed = false;

        world.ForEachComponentType(entity, [&](queen::TypeId typeId) {
            const auto* reg = registry.Find(typeId);
            if (reg == nullptr || !reg->HasReflection())
            {
                return;
            }

            void* comp = world.GetComponentRaw(entity, typeId);
            if (comp == nullptr)
            {
                return;
            }

            changed |= DrawComponent(world, entity, typeId, comp, reg->m_reflection, undo);
        });

        return changed;
    }
} // namespace forge
