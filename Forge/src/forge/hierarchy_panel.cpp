#include <wax/containers/vector.h>

#include <queen/hierarchy/children.h>
#include <queen/hierarchy/parent.h>
#include <queen/world/world.h>

#include <waggle/components/name.h>

#include <forge/forge_module.h>
#include <forge/hierarchy_panel.h>
#include <forge/selection.h>

#include <QApplication>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QMenu>
#include <QTreeWidget>
#include <QTreeWidgetItemIterator>
#include <QVBoxLayout>

#include <algorithm>
#include <cstdio>
#include <functional>

namespace forge
{
    class DropAwareTree : public QTreeWidget
    {
    public:
        using QTreeWidget::QTreeWidget;
        std::function<void(const QString&)> onAssetDropped;

    protected:
        void dragEnterEvent(QDragEnterEvent* e) override
        {
            e->acceptProposedAction();
        }
        void dragMoveEvent(QDragMoveEvent* e) override
        {
            e->acceptProposedAction();
        }

        void dropEvent(QDropEvent* e) override
        {
            auto* source = qobject_cast<QTreeWidget*>(e->source());
            if (source && source != this)
            {
                auto items = source->selectedItems();
                if (!items.isEmpty())
                {
                    auto path = items.first()->data(0, Qt::UserRole).toString();
                    if (!path.isEmpty() && onAssetDropped)
                        onAssetDropped(path);
                }
            }
            e->acceptProposedAction();
        }
    };
    static void DefaultEntityLabel(queen::World& world, queen::Entity entity, char* buf, size_t bufSize)
    {
        const auto* name = world.Get<waggle::Name>(entity);
        if (name != nullptr && !name->m_name.IsEmpty())
        {
            snprintf(buf, bufSize, "%s", name->m_name.CStr());
            return;
        }
        snprintf(buf, bufSize, "Entity %u", entity.Index());
    }

    HierarchyPanel::HierarchyPanel(EditorSelection& selection, QWidget* parent)
        : QWidget{parent}
        , m_selection{selection}
    {
        auto* layout = new QVBoxLayout{this};
        layout->setContentsMargins(0, 0, 0, 0);

        auto* tree = new DropAwareTree{this};
        tree->onAssetDropped = [this](const QString& path) {
            emit assetDropped(path);
        };
        m_tree = tree;
        m_tree->setHeaderHidden(true);
        m_tree->setSelectionMode(QAbstractItemView::ExtendedSelection);
        m_tree->setContextMenuPolicy(Qt::CustomContextMenu);
        m_tree->setAcceptDrops(true);
        m_tree->setRootIsDecorated(true);
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
        m_lastClickedItem = nullptr;
        m_entityItems.clear();
        m_tree->clear();

        wax::Vector<queen::Entity> roots{forge::GetAllocator()};
        world.ForEachArchetype([&](auto& arch) {
            if (arch.template HasComponent<queen::Parent>())
            {
                return;
            }
            for (uint32_t row = 0; row < arch.EntityCount(); ++row)
            {
                roots.PushBack(arch.GetEntity(row));
            }
        });

        std::sort(roots.begin(), roots.end(), [](queen::Entity a, queen::Entity b) { return a.Index() < b.Index(); });

        for (queen::Entity root : roots)
        {
            if (world.IsAlive(root))
            {
                AddEntityNode(world, root, nullptr);
            }
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
        m_entityItems[entity.Index()] = item;

        if (m_selection.IsSelected(entity))
        {
            item->setSelected(true);
        }

        world.ForEachChild(entity, [&](queen::Entity child) { AddEntityNode(world, child, item); });
    }

    void HierarchyPanel::RefreshEntityLabel(queen::Entity entity)
    {
        if (!m_currentWorld)
        {
            return;
        }

        auto it = m_entityItems.find(entity.Index());
        if (it == m_entityItems.end())
        {
            return;
        }

        EntityLabelFn fn = m_labelFn ? m_labelFn : DefaultEntityLabel;
        char label[64];
        fn(*m_currentWorld, entity, label, sizeof(label));
        it->second->setText(0, QString::fromUtf8(label));
    }

    void HierarchyPanel::OnItemClicked(QTreeWidgetItem* item, int /*column*/)
    {
        if (!item || !m_currentWorld)
        {
            return;
        }

        uint32_t index = item->data(0, Qt::UserRole).toUInt();
        uint16_t generation = static_cast<uint16_t>(item->data(0, Qt::UserRole + 1).toUInt());
        queen::Entity entity{index, generation};

        auto modifiers = QApplication::keyboardModifiers();

        if ((modifiers & Qt::ShiftModifier) && m_lastClickedItem)
        {
            SelectRange(m_lastClickedItem, item);
        }
        else if (modifiers & Qt::ControlModifier)
        {
            m_selection.Toggle(entity);
            m_lastClickedItem = item;
        }
        else
        {
            m_selection.Select(entity);
            m_lastClickedItem = item;
        }

        emit entitySelected(index);
    }

    void HierarchyPanel::SelectRange(QTreeWidgetItem* from, QTreeWidgetItem* to)
    {
        m_selection.Clear();

        if (from == to)
        {
            AddItemToSelection(from);
            return;
        }

        bool inRange = false;

        QTreeWidgetItemIterator it{m_tree};
        while (*it)
        {
            if (*it == from || *it == to)
            {
                if (!inRange)
                {
                    inRange = true;
                }
                else
                {
                    AddItemToSelection(*it);
                    break;
                }
            }

            if (inRange)
            {
                AddItemToSelection(*it);
            }

            ++it;
        }
    }

    void HierarchyPanel::AddItemToSelection(QTreeWidgetItem* item)
    {
        uint32_t index = item->data(0, Qt::UserRole).toUInt();
        uint16_t gen = static_cast<uint16_t>(item->data(0, Qt::UserRole + 1).toUInt());
        m_selection.AddToSelection(queen::Entity{index, gen});
    }

    void HierarchyPanel::ShowEntityContextMenu(const QPoint& pos)
    {
        if (!m_currentWorld)
        {
            return;
        }

        QTreeWidgetItem* item = m_tree->itemAt(pos);
        QMenu menu{this};

        if (item)
        {
            uint32_t index = item->data(0, Qt::UserRole).toUInt();
            uint16_t generation = static_cast<uint16_t>(item->data(0, Qt::UserRole + 1).toUInt());
            queen::Entity entity{index, generation};

            menu.addAction("Delete", [this, entity]() {
                if (m_selection.All().Size() > 1 && m_selection.IsSelected(entity))
                {
                    auto selected = m_selection.All();
                    m_selection.Clear();
                    for (size_t i = 0; i < selected.Size(); ++i)
                    {
                        if (m_currentWorld->IsAlive(selected[i]))
                        {
                            m_currentWorld->DespawnRecursive(selected[i]);
                        }
                    }
                }
                else
                {
                    m_selection.Clear();
                    m_currentWorld->DespawnRecursive(entity);
                }
                m_lastClickedItem = nullptr;
                Refresh(*m_currentWorld);
                emit sceneModified();
            });
        }
        else
        {
            m_selection.Clear();

            menu.addAction("New Entity", [this]() {
                const queen::Entity entity = m_currentWorld->Spawn().Build();
                char nameBuf[32];
                snprintf(nameBuf, sizeof(nameBuf), "Entity %u", entity.Index());
                m_currentWorld->Set(entity, waggle::Name{wax::FixedString{nameBuf}});
                m_selection.Select(entity);
                Refresh(*m_currentWorld);
                emit sceneModified();
                emit entitySelected(entity.Index());
            });
        }

        menu.exec(m_tree->viewport()->mapToGlobal(pos));
    }
} // namespace forge
