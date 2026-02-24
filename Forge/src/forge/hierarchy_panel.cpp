#include <forge/hierarchy_panel.h>
#include <forge/selection.h>

#include <queen/world/world.h>
#include <queen/hierarchy/parent.h>
#include <queen/hierarchy/children.h>

#include <imgui.h>

#include <cstdio>
#include <vector>
#include <algorithm>

namespace forge
{
    static void DefaultEntityLabel(queen::World& /*world*/, queen::Entity entity,
                                   char* buf, size_t buf_size)
    {
        snprintf(buf, buf_size, "Entity %u", entity.Index());
    }

    static void DrawEntityNode(queen::World& world, EditorSelection& selection,
                               queen::Entity entity, EntityLabelFn label_fn)
    {
        char label[64];
        label_fn(world, entity, label, sizeof(label));

        size_t child_count = world.ChildCount(entity);
        bool has_children = child_count > 0;

        ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth;
        if (!has_children)
            flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
        if (selection.IsSelected(entity))
            flags |= ImGuiTreeNodeFlags_Selected;

        ImGui::PushID(static_cast<int>(entity.Index()));
        bool open = ImGui::TreeNodeEx(label, flags);

        // Selection on click
        if (ImGui::IsItemClicked(ImGuiMouseButton_Left) && !ImGui::IsItemToggledOpen())
        {
            if (ImGui::GetIO().KeyCtrl)
                selection.Toggle(entity);
            else
                selection.Select(entity);
        }

        // Context menu
        if (ImGui::BeginPopupContextItem())
        {
            if (ImGui::MenuItem("Delete"))
            {
                world.DespawnRecursive(entity);
                if (selection.IsSelected(entity))
                    selection.Clear();
            }
            ImGui::EndPopup();
        }

        if (open && has_children)
        {
            world.ForEachChild(entity, [&](queen::Entity child) {
                DrawEntityNode(world, selection, child, label_fn);
            });
            ImGui::TreePop();
        }

        ImGui::PopID();
    }

    void DrawHierarchyPanel(queen::World& world, EditorSelection& selection, EntityLabelFn label_fn)
    {
        if (!label_fn)
            label_fn = DefaultEntityLabel;

        // Collect root entities (no Parent component)
        std::vector<queen::Entity> roots;
        world.ForEachArchetype([&](auto& arch) {
            if (arch.template HasComponent<queen::Parent>()) return;
            for (uint32_t row = 0; row < arch.EntityCount(); ++row)
                roots.push_back(arch.GetEntity(row));
        });

        // Sort by entity index for stable ordering
        std::sort(roots.begin(), roots.end(), [](queen::Entity a, queen::Entity b) {
            return a.Index() < b.Index();
        });

        // Click on empty space → deselect
        if (ImGui::IsWindowHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Left)
            && !ImGui::IsAnyItemHovered())
        {
            selection.Clear();
        }

        // Context menu on empty space
        if (ImGui::BeginPopupContextWindow("hierarchy_ctx", ImGuiPopupFlags_NoOpenOverItems))
        {
            if (ImGui::MenuItem("New Entity"))
            {
                world.Spawn().Build();
            }
            ImGui::EndPopup();
        }

        // Draw tree
        for (queen::Entity root : roots)
        {
            if (world.IsAlive(root))
                DrawEntityNode(world, selection, root, label_fn);
        }
    }
}
