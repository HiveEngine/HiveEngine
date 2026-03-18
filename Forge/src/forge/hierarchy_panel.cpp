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
        , m_selection{selection}
    {
        auto* layout = new QVBoxLayout{this};
        layout->setContentsMargins(0, 0, 0, 0);

        m_tree = new QTreeWidget{this};
        m_tree->setHeaderHidden(true);
        m_tree->setSelectionMode(QAbstractItemView::SingleSelection);
        m_tree->setContextMenuPolicy(Qt::CustomContextMenu);
        m_tree->setRootIsDecorated(false);
        m_tree->setIndentation(16);

        layout->addWidget(m_tree);

        connect(m_tree, &QTreeWidget::itemClicked, this, &HierarchyPanel::OnItemClicked);
        connect(m_tree, &QTreeWidget::customContextMenuRequested, this, &HierarchyPanel::ShowEntityContextMenu);
    }

    void HierarchyPanel::SetLabelFn(EntityLabelFn fn)
    {
        m_labelFn = fn;
    }

    void HierarchyPanel::Refresh(queen::World& world)
    {
        m_currentWorld = &world;
        m_tree->clear();

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
        EntityLabelFn fn = m_labelFn ? m_labelFn : DefaultEntityLabel;

        char label[64];
        fn(world, entity, label, sizeof(label));

        auto* item = parentItem ? new QTreeWidgetItem{parentItem} : new QTreeWidgetItem{m_tree};
        item->setText(0, QString::fromUtf8(label));
        item->setData(0, Qt::UserRole, entity.Index());
        item->setData(0, Qt::UserRole + 1, entity.Generation());

        if (m_selection.IsSelected(entity))
            item->setSelected(true);

        world.ForEachChild(entity, [&](queen::Entity child) { AddEntityNode(world, child, item); });
    }

    void HierarchyPanel::OnItemClicked(QTreeWidgetItem* item, int /*column*/)
    {
        if (!item || !m_currentWorld)
            return;

        uint32_t index = item->data(0, Qt::UserRole).toUInt();
        uint16_t generation = static_cast<uint16_t>(item->data(0, Qt::UserRole + 1).toUInt());
        queen::Entity entity{index, generation};

        if (QApplication::keyboardModifiers() & Qt::ControlModifier)
            m_selection.Toggle(entity);
        else
            m_selection.Select(entity);

        emit entitySelected(index);
    }

    void HierarchyPanel::ShowEntityContextMenu(const QPoint& pos)
    {
        if (!m_currentWorld)
            return;

        QTreeWidgetItem* item = m_tree->itemAt(pos);
        QMenu menu{this};

        if (item)
        {
            uint32_t index = item->data(0, Qt::UserRole).toUInt();
            uint16_t generation = static_cast<uint16_t>(item->data(0, Qt::UserRole + 1).toUInt());
            queen::Entity entity{index, generation};

            menu.addAction("Delete", [this, entity]() {
                if (m_selection.IsSelected(entity))
                    m_selection.Clear();
                m_currentWorld->DespawnRecursive(entity);
                Refresh(*m_currentWorld);
                emit sceneModified();
            });
        }
        else
        {
            m_selection.Clear();

            menu.addAction("New Entity", [this]() {
                const queen::Entity entity = m_currentWorld->Spawn().Build();
                m_selection.Select(entity);
                Refresh(*m_currentWorld);
                emit sceneModified();
                emit entitySelected(entity.Index());
            });
        }

        menu.exec(m_tree->viewport()->mapToGlobal(pos));
    }
} // namespace forge
