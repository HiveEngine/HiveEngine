#include <forge/inspector_panel.h>
#include <forge/selection.h>
#include <forge/undo.h>

#include <queen/world/world.h>
#include <queen/reflect/component_registry.h>
#include <queen/reflect/field_info.h>
#include <queen/reflect/field_attributes.h>
#include <queen/core/type_id.h>

#include <hive/math/types.h>

#include <imgui.h>

#include <cstring>
#include <cstdio>

namespace forge
{
    struct DragState
    {
        queen::Entity entity{};
        queen::TypeId type_id{0};
        uint16_t offset{0};
        uint16_t size{0};
        std::byte before[64]{};
        bool active{false};
    };

    static DragState s_drag{};

    static void BeginDrag(queen::Entity entity, queen::TypeId type_id,
                          uint16_t offset, uint16_t size, const void* current)
    {
        if (s_drag.active) return;
        s_drag.entity = entity;
        s_drag.type_id = type_id;
        s_drag.offset = offset;
        s_drag.size = size;
        if (size <= sizeof(s_drag.before))
            std::memcpy(s_drag.before, current, size);
        s_drag.active = true;
    }

    static void EndDrag(UndoStack& undo, const void* current)
    {
        if (!s_drag.active) return;
        s_drag.active = false;
        // Only push if value actually changed
        if (std::memcmp(s_drag.before, current, s_drag.size) != 0)
        {
            undo.PushSetField(s_drag.entity, s_drag.type_id,
                              s_drag.offset, s_drag.size,
                              s_drag.before, current);
        }
    }

    [[nodiscard]] static const char* GetFieldDisplayName(const queen::FieldInfo& field) noexcept
    {
        if (field.attributes && field.attributes->display_name)
            return field.attributes->display_name;
        return field.name;
    }

    [[nodiscard]] static bool HasFlag(const queen::FieldInfo& field, queen::FieldFlag flag) noexcept
    {
        return field.attributes && field.attributes->HasFlag(flag);
    }

    static bool DrawFloat3Widget(const char* label, void* data, const queen::FieldInfo& field,
                                 queen::Entity entity, queen::TypeId type_id,
                                 uint16_t base_offset, UndoStack& undo)
    {
        auto* v = static_cast<float*>(data);
        uint16_t offset = static_cast<uint16_t>(base_offset + field.offset);

        if (HasFlag(field, queen::FieldFlag::Color))
        {
            if (ImGui::ColorEdit3(label, v))
            {
                if (ImGui::IsItemActivated())
                    BeginDrag(entity, type_id, offset, 12, v);
                return true;
            }
            if (ImGui::IsItemDeactivatedAfterEdit())
                EndDrag(undo, v);
            return false;
        }

        float speed = 0.01f;
        if (field.attributes && field.attributes->HasRange())
            speed = (field.attributes->max - field.attributes->min) / 500.f;

        if (ImGui::DragFloat3(label, v, speed))
        {
            if (ImGui::IsItemActivated())
                BeginDrag(entity, type_id, offset, 12, v);
            return true;
        }
        if (ImGui::IsItemDeactivatedAfterEdit())
            EndDrag(undo, v);
        return false;
    }

    // Displayed as Euler angles in degrees
    static bool DrawQuatWidget(const char* label, void* data,
                               queen::Entity entity, queen::TypeId type_id,
                               uint16_t base_offset, const queen::FieldInfo& field,
                               UndoStack& undo)
    {
        auto* q = static_cast<float*>(data); // x, y, z, w
        uint16_t offset = static_cast<uint16_t>(base_offset + field.offset);

        float sinr = 2.f * (q[3] * q[0] + q[1] * q[2]);
        float cosr = 1.f - 2.f * (q[0] * q[0] + q[1] * q[1]);
        float pitch = atan2f(sinr, cosr) * (180.f / 3.14159265f);

        float sinp = 2.f * (q[3] * q[1] - q[2] * q[0]);
        float yaw;
        if (fabsf(sinp) >= 1.f)
            yaw = copysignf(90.f, sinp);
        else
            yaw = asinf(sinp) * (180.f / 3.14159265f);

        float siny = 2.f * (q[3] * q[2] + q[0] * q[1]);
        float cosy = 1.f - 2.f * (q[1] * q[1] + q[2] * q[2]);
        float roll = atan2f(siny, cosy) * (180.f / 3.14159265f);

        float euler[3] = {pitch, yaw, roll};
        bool changed = false;

        if (ImGui::DragFloat3(label, euler, 0.5f))
        {
            if (ImGui::IsItemActivated())
                BeginDrag(entity, type_id, offset, 16, q);

            float p = euler[0] * (3.14159265f / 180.f) * 0.5f;
            float y = euler[1] * (3.14159265f / 180.f) * 0.5f;
            float r = euler[2] * (3.14159265f / 180.f) * 0.5f;

            float cp = cosf(p), sp = sinf(p);
            float cy = cosf(y), sy = sinf(y);
            float cr = cosf(r), sr = sinf(r);

            q[0] = sr * cp * cy - cr * sp * sy; // x
            q[1] = cr * sp * cy + sr * cp * sy; // y
            q[2] = cr * cp * sy - sr * sp * cy; // z
            q[3] = cr * cp * cy + sr * sp * sy; // w
            changed = true;
        }
        if (ImGui::IsItemDeactivatedAfterEdit())
            EndDrag(undo, q);

        return changed;
    }

    static bool DrawField(const queen::FieldInfo& field, void* component_data,
                          queen::Entity entity, queen::TypeId type_id,
                          uint16_t base_offset, UndoStack& undo)
    {
        if (HasFlag(field, queen::FieldFlag::Hidden))
            return false;

        const char* label = GetFieldDisplayName(field);
        void* field_data = static_cast<std::byte*>(component_data) + field.offset;
        uint16_t offset = static_cast<uint16_t>(base_offset + field.offset);
        bool read_only = HasFlag(field, queen::FieldFlag::ReadOnly);
        bool changed = false;

        if (read_only)
            ImGui::BeginDisabled();

        switch (field.type)
        {
        case queen::FieldType::Float32:
        {
            auto* v = static_cast<float*>(field_data);
            float speed = 0.01f;
            float f_min = 0.f, f_max = 0.f;
            if (field.attributes && field.attributes->HasRange())
            {
                f_min = field.attributes->min;
                f_max = field.attributes->max;
                speed = (f_max - f_min) / 500.f;
            }

            if (HasFlag(field, queen::FieldFlag::Angle))
            {
                // SliderAngle expects radians, displays degrees
                changed = ImGui::SliderAngle(label, v,
                    field.attributes ? field.attributes->min * (180.f / 3.14159265f) : -360.f,
                    field.attributes ? field.attributes->max * (180.f / 3.14159265f) : 360.f);
            }
            else
            {
                changed = ImGui::DragFloat(label, v, speed, f_min, f_max);
            }

            if (ImGui::IsItemActivated())
                BeginDrag(entity, type_id, offset, static_cast<uint16_t>(field.size), field_data);
            if (ImGui::IsItemDeactivatedAfterEdit())
                EndDrag(undo, field_data);
            break;
        }

        case queen::FieldType::Float64:
        {
            auto* v = static_cast<double*>(field_data);
            float fv = static_cast<float>(*v);
            if (ImGui::DragFloat(label, &fv, 0.01f))
            {
                *v = static_cast<double>(fv);
                changed = true;
            }
            if (ImGui::IsItemActivated())
                BeginDrag(entity, type_id, offset, static_cast<uint16_t>(field.size), field_data);
            if (ImGui::IsItemDeactivatedAfterEdit())
                EndDrag(undo, field_data);
            break;
        }

        case queen::FieldType::Int32:
        {
            auto* v = static_cast<int32_t*>(field_data);
            int i_min = 0, i_max = 0;
            if (field.attributes && field.attributes->HasRange())
            {
                i_min = static_cast<int>(field.attributes->min);
                i_max = static_cast<int>(field.attributes->max);
            }
            changed = ImGui::DragInt(label, v, 1.f, i_min, i_max);
            if (ImGui::IsItemActivated())
                BeginDrag(entity, type_id, offset, static_cast<uint16_t>(field.size), field_data);
            if (ImGui::IsItemDeactivatedAfterEdit())
                EndDrag(undo, field_data);
            break;
        }

        case queen::FieldType::Uint32:
        {
            auto* v = static_cast<uint32_t*>(field_data);
            int iv = static_cast<int>(*v);
            if (ImGui::DragInt(label, &iv, 1.f, 0, 0))
            {
                *v = static_cast<uint32_t>(iv);
                changed = true;
            }
            if (ImGui::IsItemActivated())
                BeginDrag(entity, type_id, offset, static_cast<uint16_t>(field.size), field_data);
            if (ImGui::IsItemDeactivatedAfterEdit())
                EndDrag(undo, field_data);
            break;
        }

        case queen::FieldType::Bool:
        {
            auto* v = static_cast<bool*>(field_data);
            bool before = *v;
            if (ImGui::Checkbox(label, v))
            {
                undo.PushSetField(entity, type_id, offset,
                                  static_cast<uint16_t>(field.size), &before, v);
                changed = true;
            }
            break;
        }

        case queen::FieldType::Struct:
        {
            if (field.size == sizeof(hive::math::Float3) &&
                field.nested_type_id == queen::TypeIdOf<hive::math::Float3>())
            {
                changed = DrawFloat3Widget(label, field_data, field,
                                           entity, type_id, base_offset, undo);
            }
            else if (field.size == sizeof(hive::math::Quat) &&
                     field.nested_type_id == queen::TypeIdOf<hive::math::Quat>())
            {
                changed = DrawQuatWidget(label, field_data,
                                         entity, type_id, base_offset, field, undo);
            }
            else if (field.nested_fields && field.nested_field_count > 0)
            {
                if (ImGui::TreeNode(label))
                {
                    for (size_t i = 0; i < field.nested_field_count; ++i)
                    {
                        changed |= DrawField(field.nested_fields[i], field_data,
                                             entity, type_id,
                                             static_cast<uint16_t>(base_offset + field.offset), undo);
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

        case queen::FieldType::Enum:
        {
            if (field.enum_info && field.enum_info->IsValid())
            {
                int64_t current_val = 0;
                std::memcpy(&current_val, field_data, field.size <= 8 ? field.size : 8);

                const char* current_name = field.enum_info->NameOf(current_val);
                if (!current_name) current_name = "???";

                if (ImGui::BeginCombo(label, current_name))
                {
                    for (size_t i = 0; i < field.enum_info->entry_count; ++i)
                    {
                        const auto& entry = field.enum_info->entries[i];
                        bool selected = (entry.value == current_val);
                        if (ImGui::Selectable(entry.name, selected))
                        {
                            std::byte before[8]{};
                            std::memcpy(before, field_data, field.size);
                            std::memcpy(field_data, &entry.value, field.size);
                            undo.PushSetField(entity, type_id, offset,
                                              static_cast<uint16_t>(field.size), before, field_data);
                            changed = true;
                        }
                        if (selected) ImGui::SetItemDefaultFocus();
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
            ImGui::TextDisabled("%s (unsupported type %d)", label, static_cast<int>(field.type));
            break;
        }

        if (read_only)
            ImGui::EndDisabled();

        // Tooltip
        if (field.attributes && field.attributes->tooltip && ImGui::IsItemHovered())
            ImGui::SetTooltip("%s", field.attributes->tooltip);

        return changed;
    }

    static void DrawComponent(queen::World& world, queen::Entity entity,
                              queen::TypeId type_id, void* component_data,
                              const queen::ComponentReflection& reflection,
                              UndoStack& undo)
    {
        const char* type_name = reflection.name ? reflection.name : "Component";

        ImGuiTreeNodeFlags header_flags = ImGuiTreeNodeFlags_DefaultOpen
                                        | ImGuiTreeNodeFlags_Framed
                                        | ImGuiTreeNodeFlags_AllowOverlap;

        bool open = ImGui::CollapsingHeader(type_name, header_flags);

        if (open)
        {
            ImGui::PushID(static_cast<int>(type_id));
            ImGui::Indent(4.f);

            for (size_t i = 0; i < reflection.field_count; ++i)
            {
                DrawField(reflection.fields[i], component_data,
                          entity, type_id, 0, undo);
            }

            ImGui::Unindent(4.f);
            ImGui::PopID();
        }
    }

    void DrawInspectorPanel(queen::World& world, EditorSelection& selection,
                            const queen::ComponentRegistry<256>& registry,
                            UndoStack& undo)
    {
        queen::Entity entity = selection.Primary();
        if (entity.IsNull() || !world.IsAlive(entity))
        {
            ImGui::TextDisabled("No entity selected");
            return;
        }

        ImGui::Text("Entity %u", entity.Index());
        ImGui::Separator();

        world.ForEachComponentType(entity, [&](queen::TypeId type_id) {
            const auto* reg = registry.Find(type_id);
            if (!reg || !reg->HasReflection()) return;

            void* comp = world.GetComponentRaw(entity, type_id);
            if (!comp) return;

            DrawComponent(world, entity, type_id, comp, reg->reflection, undo);
        });
    }
}
