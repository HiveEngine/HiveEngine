#pragma once

#include <queen/core/entity.h>

#include <QWidget>

class QTreeWidget;
class QTreeWidgetItem;

namespace queen
{
    class World;
}

namespace forge
{
    class EditorSelection;

    using EntityLabelFn = void (*)(queen::World& world, queen::Entity entity, char* buf, size_t bufSize);

    class HierarchyPanel : public QWidget
    {
        Q_OBJECT

    public:
        explicit HierarchyPanel(EditorSelection& selection, QWidget* parent = nullptr);

        void SetLabelFn(EntityLabelFn fn);
        void Refresh(queen::World& world);

    signals:
        void entitySelected(uint32_t entityIndex);
        void sceneModified();

    private:
        void AddEntityNode(queen::World& world, queen::Entity entity, QTreeWidgetItem* parentItem);
        void OnItemClicked(QTreeWidgetItem* item, int column);
        void ShowEntityContextMenu(const QPoint& pos);

        QTreeWidget* tree_{};
        EditorSelection& selection_;
        EntityLabelFn labelFn_{};
        queen::World* currentWorld_{};
    };
} // namespace forge
