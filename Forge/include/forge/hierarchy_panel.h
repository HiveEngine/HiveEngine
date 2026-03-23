#pragma once

#include <queen/core/entity.h>

#include <QWidget>

#include <unordered_map>

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
        void AddEntityNode(queen::World& world, queen::Entity entity, QTreeWidgetItem* parentItem);
        void OnItemClicked(QTreeWidgetItem* item, int column);
        void ShowEntityContextMenu(const QPoint& pos);

        void SelectRange(QTreeWidgetItem* from, QTreeWidgetItem* to);
        void AddItemToSelection(QTreeWidgetItem* item);
        void ReindexRoots();

        QTreeWidget* m_tree{};
        EditorSelection& m_selection;
        EditorUndoManager& m_editorUndo;
        EntityLabelFn m_labelFn{};
        queen::World* m_currentWorld{};
        QTreeWidgetItem* m_lastClickedItem{};
        std::unordered_map<uint32_t, QTreeWidgetItem*> m_entityItems;
    };
} // namespace forge
