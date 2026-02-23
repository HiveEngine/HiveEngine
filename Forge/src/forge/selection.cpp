#include <forge/selection.h>
#include <algorithm>

namespace forge
{
    void EditorSelection::Select(queen::Entity entity)
    {
        selected_.clear();
        if (!entity.IsNull())
            selected_.push_back(entity);
        primary_ = entity;
    }

    void EditorSelection::Toggle(queen::Entity entity)
    {
        if (entity.IsNull()) return;

        auto it = std::find(selected_.begin(), selected_.end(), entity);
        if (it != selected_.end())
        {
            selected_.erase(it);
            // If we removed the primary, pick the last one (or null)
            if (primary_ == entity)
                primary_ = selected_.empty() ? queen::Entity{} : selected_.back();
        }
        else
        {
            selected_.push_back(entity);
            primary_ = entity;
        }
    }

    void EditorSelection::Clear()
    {
        selected_.clear();
        primary_ = {};
    }

    bool EditorSelection::IsSelected(queen::Entity entity) const noexcept
    {
        return std::find(selected_.begin(), selected_.end(), entity) != selected_.end();
    }
}
