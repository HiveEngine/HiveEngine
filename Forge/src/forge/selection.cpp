#include <forge/asset_browser.h>
#include <forge/selection.h>

#include <algorithm>

namespace forge
{
    void EditorSelection::Select(queen::Entity entity)
    {
        m_kind = entity.IsNull() ? SelectionKind::NONE : SelectionKind::ENTITY;
        m_selected.Clear();
        if (!entity.IsNull())
        {
            m_selected.PushBack(entity);
        }
        m_primary = entity;
        m_assetPath.clear();
    }

    void EditorSelection::AddToSelection(queen::Entity entity)
    {
        if (entity.IsNull())
        {
            return;
        }

        m_kind = SelectionKind::ENTITY;
        m_assetPath.clear();

        if (std::find(m_selected.begin(), m_selected.end(), entity) == m_selected.end())
        {
            m_selected.PushBack(entity);
            m_primary = entity;
        }
    }

    void EditorSelection::Toggle(queen::Entity entity)
    {
        if (entity.IsNull())
        {
            return;
        }

        m_kind = SelectionKind::ENTITY;
        m_assetPath.clear();

        auto it = std::find(m_selected.begin(), m_selected.end(), entity);
        if (it != m_selected.end())
        {
            m_selected.Erase(it);
            if (m_primary == entity)
            {
                m_primary = m_selected.IsEmpty() ? queen::Entity{} : m_selected.Back();
            }
            if (m_selected.IsEmpty())
            {
                m_kind = SelectionKind::NONE;
            }
        }
        else
        {
            m_selected.PushBack(entity);
            m_primary = entity;
        }
    }

    void EditorSelection::SelectAsset(const std::filesystem::path& path, AssetType type)
    {
        m_kind = SelectionKind::ASSET;
        m_assetPath = path;
        m_assetType = static_cast<uint8_t>(type);
        m_selected.Clear();
        m_primary = {};
    }

    void EditorSelection::Clear()
    {
        m_kind = SelectionKind::NONE;
        m_selected.Clear();
        m_primary = {};
        m_assetPath.clear();
    }

    bool EditorSelection::IsSelected(queen::Entity entity) const noexcept
    {
        return std::find(m_selected.begin(), m_selected.end(), entity) != m_selected.end();
    }

    AssetType EditorSelection::SelectedAssetType() const noexcept
    {
        return static_cast<AssetType>(m_assetType);
    }
} // namespace forge
