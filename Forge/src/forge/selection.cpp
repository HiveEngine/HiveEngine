#include <forge/selection.h>

#include <algorithm>

namespace forge
{
    void EditorSelection::Select(queen::Entity entity)
    {
        m_selected.Clear();
        if (!entity.IsNull())
            m_selected.PushBack(entity);
        m_primary = entity;
    }

    void EditorSelection::Toggle(queen::Entity entity)
    {
        if (entity.IsNull())
            return;

        auto it = std::find(m_selected.begin(), m_selected.end(), entity);
        if (it != m_selected.end())
        {
            m_selected.Erase(it);
            if (m_primary == entity)
                m_primary = m_selected.IsEmpty() ? queen::Entity{} : m_selected.Back();
        }
        else
        {
            m_selected.PushBack(entity);
            m_primary = entity;
        }
    }

    void EditorSelection::Clear()
    {
        m_selected.Clear();
        m_primary = {};
    }

    bool EditorSelection::IsSelected(queen::Entity entity) const noexcept
    {
        return std::find(m_selected.begin(), m_selected.end(), entity) != m_selected.end();
    }
} // namespace forge
