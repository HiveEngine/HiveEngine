#include <queen/hierarchy/children.h>
#include <queen/hierarchy/parent.h>
#include <queen/world/world.h>

#include <forge/hierarchy_panel.h>
#include <forge/selection.h>

#include <QApplication>
#include <QMenu>
#include <QTreeWidget>
#include <QVBoxLayout>

#include <algorithm>
#include <cstdio>
#include <vector>

namespace forge
{
    static void DefaultEntityLabel(queen::World& /*world*/, queen::Entity entity, char* buf, size_t bufSize)
    {
        snprintf(buf, bufSize, "Entity %u", entity.Index());
    }

    HierarchyPanel::HierarchyPanel(EditorSelection& selection, QWidget* parent)
        : QWidget{parent}
        , selection_{selection}
    {
        auto* layout = new QVBoxLayout{this};
        layout->setContentsMargins(0, 0, 0, 0);

        tree_ = new QTreeWidget{this};
        tree_->setHeaderHidden(true);
        tree_->setSelectionMode(QAbstractItemView::SingleSelection);
        tree_->setContextMenuPolicy(Qt::CustomContextMenu);
        tree_->setRootIsDecorated(false);
        tree_->setIndentation(16);

        layout->addWidget(tree_);

        connect(tree_, &QTreeWidget::itemClicked, this, &HierarchyPanel::OnItemClicked);
        connect(tree_, &QTreeWidget::customContextMenuRequested, this, &HierarchyPanel::ShowEntityContextMenu);
    }

    void HierarchyPanel::SetLabelFn(EntityLabelFn fn)
    {
        labelFn_ = fn;
    }

    void HierarchyPanel::Refresh(queen::World& world)
    {
        currentWorld_ = &world;
        tree_->clear();

        std::vector<queen::Entity> roots;
        world.ForEachArchetype([&](auto& arch) {
            if (arch.template HasComponent<queen::Parent>())
                return;
            for (uint32_t row = 0; row < arch.EntityCount(); ++row)
                roots.push_back(arch.GetEntity(row));
        });

        std::sort(roots.begin(), roots.end(), [](queen::Entity a, queen::Entity b) { return a.Index() < b.Index(); });

        for (queen::Entity root : roots)
        {
            if (world.IsAlive(root))
                AddEntityNode(world, root, nullptr);
        }
    }

    void HierarchyPanel::AddEntityNode(queen::World& world, queen::Entity entity, QTreeWidgetItem* parentItem)
    {
        EntityLabelFn fn = labelFn_ ? labelFn_ : DefaultEntityLabel;

        char label[64];
        fn(world, entity, label, sizeof(label));

        auto* item = parentItem ? new QTreeWidgetItem{parentItem} : new QTreeWidgetItem{tree_};
        item->setText(0, QString::fromUtf8(label));
        item->setData(0, Qt::UserRole, entity.Index());
        item->setData(0, Qt::UserRole + 1, entity.Generation());

        if (selection_.IsSelected(entity))
            item->setSelected(true);

        world.ForEachChild(entity, [&](queen::Entity child) { AddEntityNode(world, child, item); });
    }

    void HierarchyPanel::OnItemClicked(QTreeWidgetItem* item, int /*column*/)
    {
        if (!item || !currentWorld_)
            return;

        uint32_t index = item->data(0, Qt::UserRole).toUInt();
        uint16_t generation = static_cast<uint16_t>(item->data(0, Qt::UserRole + 1).toUInt());
        queen::Entity entity{index, generation};

        if (QApplication::keyboardModifiers() & Qt::ControlModifier)
            selection_.Toggle(entity);
        else
            selection_.Select(entity);

        emit entitySelected(index);
    }

    void HierarchyPanel::ShowEntityContextMenu(const QPoint& pos)
    {
        if (!currentWorld_)
            return;

        QTreeWidgetItem* item = tree_->itemAt(pos);
        QMenu menu{this};

        if (item)
        {
            uint32_t index = item->data(0, Qt::UserRole).toUInt();
            uint16_t generation = static_cast<uint16_t>(item->data(0, Qt::UserRole + 1).toUInt());
            queen::Entity entity{index, generation};

            menu.addAction("Delete", [this, entity]() {
                if (selection_.IsSelected(entity))
                    selection_.Clear();
                currentWorld_->DespawnRecursive(entity);
                Refresh(*currentWorld_);
                emit sceneModified();
            });
        }
        else
        {
            selection_.Clear();

            menu.addAction("New Entity", [this]() {
                const queen::Entity entity = currentWorld_->Spawn().Build();
                selection_.Select(entity);
                Refresh(*currentWorld_);
                emit sceneModified();
                emit entitySelected(entity.Index());
            });
        }

        menu.exec(tree_->viewport()->mapToGlobal(pos));
    }
} // namespace forge
