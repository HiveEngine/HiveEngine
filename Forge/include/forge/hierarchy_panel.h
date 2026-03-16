#pragma once

#include <queen/core/entity.h>

namespace queen
{
    class World;
}
namespace forge
{
    class EditorSelection;
}

namespace forge
{
    // Optional callback to format entity display names.
    // If null, defaults to "Entity <index>".
    using EntityLabelFn = void (*)(queen::World& world, queen::Entity entity, char* buf, size_t bufSize);

    // Must be called between ImGui::Begin("Hierarchy") and ImGui::End().
    // Returns true when the scene hierarchy was modified.
    bool DrawHierarchyPanel(queen::World& world, EditorSelection& selection, EntityLabelFn labelFn = nullptr);
} // namespace forge
