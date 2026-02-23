#pragma once

#include <queen/core/entity.h>

namespace queen { class World; struct RegisteredComponent; }
namespace queen { template<size_t> class ComponentRegistry; }

namespace forge
{
    class EditorSelection;
    class UndoStack;

    // Must be called between ImGui::Begin("Inspector") and ImGui::End().
    void DrawInspectorPanel(queen::World& world, EditorSelection& selection,
                            const queen::ComponentRegistry<256>& registry,
                            UndoStack& undo);
}
