#pragma once

#include <queen/core/entity.h>

#include <QWidget>

#include <unordered_map>
#include <unordered_set>

class QTreeWidget;
class QTreeWidgetItem;

namespace queen
{
    class World;
}

namespace forge
{
    class EditorSelection;
    class EditorUndoManager;

    using EntityLabelFn = void (*)(queen::World& world, queen::Entity entity, char* buf, size_t bufSize);

    class HierarchyPanel : public QWidget
    {
        Q_OBJECT

    public:
        explicit HierarchyPanel(EditorSelection& selection, EditorUndoManager& editorUndo, QWidget* parent = nullptr);

        void SetLabelFn(EntityLabelFn fn);
        void Refresh(queen::World& world);
        void RefreshEntityItem(queen::Entity entity);

    signals:
        void entitySelected(uint32_t entityIndex);
        void sceneModified();
        void assetDropped(const QString& path);

    private:
        void SetupTreeWidget();
        void SetupShortcuts();
        void SetupDropCallbacks();

        void AddEntityNode(queen::World& world, queen::Entity entity, QTreeWidgetItem* parentItem);
        void OnItemClicked(QTreeWidgetItem* item, int column);
        void ShowEntityContextMenu(const QPoint& pos);

        void HandleReparent(queen::Entity dragged, queen::Entity target, int dropPos);
        void HandleDetach(queen::Entity dragged);
        void DuplicateSelection();
        void PasteClipboard();

        void SelectRange(QTreeWidgetItem* from, QTreeWidgetItem* to);
        void AddItemToSelection(QTreeWidgetItem* item);
        void ReindexRoots();
        queen::Entity CloneEntityRecursive(queen::Entity source);
        void DeleteEntitiesWithUndo(const queen::Entity* entities, size_t count);

        static constexpr uint64_t EntityKey(queen::Entity e)
        {
            return (uint64_t(e.Generation()) << 32) | e.Index();
        }

        QTreeWidget* m_tree{};
        EditorSelection& m_selection;
        EditorUndoManager& m_editorUndo;
        EntityLabelFn m_labelFn{};
        queen::World* m_currentWorld{};
        QTreeWidgetItem* m_lastClickedItem{};
        std::unordered_map<uint32_t, QTreeWidgetItem*> m_entityItems;
        std::vector<queen::Entity> m_clipboard;
        std::unordered_set<uint64_t> m_undoHidden;
    };
} // namespace forge
