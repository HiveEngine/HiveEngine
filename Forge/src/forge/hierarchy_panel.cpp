#include <wax/containers/vector.h>

#include <queen/hierarchy/children.h>
#include <queen/hierarchy/parent.h>
#include <queen/world/world.h>

#include <waggle/components/disabled.h>
#include <waggle/components/name.h>
#include <waggle/components/sibling_index.h>
#include <waggle/disabled_propagation.h>

#include <forge/editor_undo.h>
#include <forge/forge_module.h>
#include <forge/hierarchy_panel.h>
#include <forge/selection.h>

#include <QApplication>
#include <QDir>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QMenu>
#include <QPainter>
#include <QTreeWidget>
#include <QTreeWidgetItemIterator>
#include <QVBoxLayout>

#include <algorithm>
#include <cstdio>
#include <functional>
#include <unordered_set>

namespace forge
{
    class DropAwareTree : public QTreeWidget
    {
    public:
        using QTreeWidget::QTreeWidget;
        std::function<void(const QString&)> onAssetDropped;
        std::function<void(queen::Entity dragged, queen::Entity target, int dropPos)> onEntityReparent;
        std::function<void(queen::Entity dragged)> onEntityDetach;

    protected:
        void dragEnterEvent(QDragEnterEvent* e) override
        {
            auto* source = qobject_cast<QTreeWidget*>(e->source());
            if (source == this)
            {
                QTreeWidget::dragEnterEvent(e);
                return;
            }
            e->acceptProposedAction();
        }

        void dragMoveEvent(QDragMoveEvent* e) override
        {
            auto* source = qobject_cast<QTreeWidget*>(e->source());
            if (source == this)
            {
                QTreeWidget::dragMoveEvent(e);
                return;
            }
            e->acceptProposedAction();
        }

        void dropEvent(QDropEvent* e) override
        {
            auto* source = qobject_cast<QTreeWidget*>(e->source());
            if (source == this)
            {
                auto items = selectedItems();
                if (items.isEmpty())
                {
                    e->ignore();
                    return;
                }

                auto* draggedItem = items.first();
                uint32_t draggedIdx = draggedItem->data(0, Qt::UserRole).toUInt();
                uint16_t draggedGen = static_cast<uint16_t>(draggedItem->data(0, Qt::UserRole + 1).toUInt());
                queen::Entity dragged{draggedIdx, draggedGen};

                auto* targetItem = itemAt(e->position().toPoint());
                if (targetItem && targetItem != draggedItem)
                {
                    uint32_t targetIdx = targetItem->data(0, Qt::UserRole).toUInt();
                    uint16_t targetGen = static_cast<uint16_t>(targetItem->data(0, Qt::UserRole + 1).toUInt());
                    queen::Entity target{targetIdx, targetGen};

                    int pos = static_cast<int>(dropIndicatorPosition());
                    if (onEntityReparent)
                        onEntityReparent(dragged, target, pos);
                }
                else if (!targetItem)
                {
                    if (onEntityDetach)
                        onEntityDetach(dragged);
                }
                e->setDropAction(Qt::IgnoreAction);
                e->accept();
                return;
            }

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
    static const QColor ENABLED_COLOR{0xe8, 0xe8, 0xe8};
    static const QColor DISABLED_COLOR{0x55, 0x55, 0x55};

    static QPixmap MakeTrianglePixmap(bool open)
    {
        constexpr int kSize = 10;
        QPixmap pix{kSize, kSize};
        pix.fill(Qt::transparent);
        QPainter p{&pix};
        p.setRenderHint(QPainter::Antialiasing);
        p.setPen(Qt::NoPen);
        p.setBrush(QColor{0x99, 0x99, 0x99});
        QPolygonF tri;
        if (open)
        {
            tri << QPointF(1, 2) << QPointF(9, 2) << QPointF(5, 8);
        }
        else
        {
            tri << QPointF(3, 1) << QPointF(3, 9) << QPointF(9, 5);
        }
        p.drawPolygon(tri);
        return pix;
    }

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

    HierarchyPanel::HierarchyPanel(EditorSelection& selection, EditorUndoManager& editorUndo, QWidget* parent)
        : QWidget{parent}
        , m_selection{selection}
        , m_editorUndo{editorUndo}
    {
        auto* layout = new QVBoxLayout{this};
        layout->setContentsMargins(0, 0, 0, 0);

        auto* tree = new DropAwareTree{this};
        tree->onAssetDropped = [this](const QString& path) { emit assetDropped(path); };

        tree->onEntityReparent = [this](queen::Entity dragged, queen::Entity target, int dropPos) {
            if (!m_currentWorld || !m_currentWorld->IsAlive(dragged) || !m_currentWorld->IsAlive(target))
                return;
            if (dragged == target || m_currentWorld->IsDescendantOf(target, dragged))
                return;

            auto* oldParentComp = m_currentWorld->Get<queen::Parent>(dragged);
            queen::Entity oldParent = oldParentComp ? oldParentComp->m_entity : queen::Entity{};

            // QAbstractItemView::DropIndicatorPosition: OnItem=0, AboveItem=1, BelowItem=2, OnViewport=3
            if (dropPos == 0)
            {
                m_currentWorld->SetParent(dragged, target);
            }
            else
            {
                queen::Entity targetParent = m_currentWorld->GetParent(target);
                if (m_currentWorld->IsAlive(targetParent))
                {
                    m_currentWorld->SetParent(dragged, targetParent);
                    size_t targetIdx = m_currentWorld->GetChildIndex(targetParent, target);
                    m_currentWorld->ReorderChild(targetParent, dragged, dropPos == 2 ? targetIdx + 1 : targetIdx);
                }
                else
                {
                    m_currentWorld->RemoveParent(dragged);
                    auto* targetSi = m_currentWorld->Get<waggle::SiblingIndex>(target);
                    uint32_t targetOrder = targetSi ? targetSi->m_value : 0;
                    uint32_t newOrder = (dropPos == 2) ? targetOrder + 1 : (targetOrder > 0 ? targetOrder - 1 : 0);
                    m_currentWorld->Set(dragged, waggle::SiblingIndex{newOrder});
                    ReindexRoots();
                }
            }

            waggle::RecalculateEntityHierarchyDisabled(*m_currentWorld, dragged);
            Refresh(*m_currentWorld);

            auto captured = std::make_shared<std::pair<queen::Entity, queen::Entity>>(dragged, oldParent);
            queen::Entity newParent = m_currentWorld->GetParent(dragged);

            m_editorUndo.Push(
                [this, captured]() {
                    if (m_currentWorld->IsAlive(captured->second))
                        m_currentWorld->SetParent(captured->first, captured->second);
                    else
                        m_currentWorld->RemoveParent(captured->first);
                    waggle::RecalculateEntityHierarchyDisabled(*m_currentWorld, captured->first);
                    Refresh(*m_currentWorld);
                },
                [this, captured, newParent]() {
                    if (m_currentWorld->IsAlive(newParent))
                        m_currentWorld->SetParent(captured->first, newParent);
                    else
                        m_currentWorld->RemoveParent(captured->first);
                    waggle::RecalculateEntityHierarchyDisabled(*m_currentWorld, captured->first);
                    Refresh(*m_currentWorld);
                });

            emit sceneModified();
        };

        tree->onEntityDetach = [this](queen::Entity dragged) {
            if (!m_currentWorld || !m_currentWorld->IsAlive(dragged))
                return;
            if (!m_currentWorld->Has<queen::Parent>(dragged))
                return;

            auto oldParent = m_currentWorld->Get<queen::Parent>(dragged)->m_entity;
            auto captured = std::make_shared<std::pair<queen::Entity, queen::Entity>>(dragged, oldParent);

            m_currentWorld->RemoveParent(dragged);
            waggle::RecalculateEntityHierarchyDisabled(*m_currentWorld, dragged);
            Refresh(*m_currentWorld);

            m_editorUndo.Push(
                [this, captured]() {
                    m_currentWorld->SetParent(captured->first, captured->second);
                    waggle::RecalculateEntityHierarchyDisabled(*m_currentWorld, captured->first);
                    Refresh(*m_currentWorld);
                },
                [this, captured]() {
                    m_currentWorld->RemoveParent(captured->first);
                    waggle::RecalculateEntityHierarchyDisabled(*m_currentWorld, captured->first);
                    Refresh(*m_currentWorld);
                });

            emit sceneModified();
        };

        m_tree = tree;
        m_tree->setHeaderHidden(true);
        m_tree->setSelectionMode(QAbstractItemView::ExtendedSelection);
        m_tree->setContextMenuPolicy(Qt::CustomContextMenu);
        m_tree->setAcceptDrops(true);
        m_tree->setDragEnabled(true);
        m_tree->setDragDropMode(QAbstractItemView::InternalMove);
        m_tree->setDefaultDropAction(Qt::MoveAction);
        m_tree->setRootIsDecorated(true);
        m_tree->setIndentation(18);
        auto closedPix = MakeTrianglePixmap(false);
        auto openPix = MakeTrianglePixmap(true);
        auto tempDir = QDir::tempPath();
        auto closedPath = tempDir + "/hive_arrow_closed.png";
        auto openPath = tempDir + "/hive_arrow_open.png";
        closedPix.save(closedPath);
        openPix.save(openPath);

        m_tree->setStyleSheet(QString{
            "QTreeWidget { background: #1e1e1e; border: none; outline: none; }"
            "QTreeWidget::item { padding: 2px 0; color: #c8c8c8; }"
            "QTreeWidget::item:selected { background: #3d2e0a; color: #f0a500; }"
            "QTreeWidget::item:hover:!selected { background: #2a2a2a; }"
            "QTreeWidget::branch:has-children:!has-siblings:closed,"
            "QTreeWidget::branch:closed:has-children:has-siblings {"
            "  image: url(%1);"
            "}"
            "QTreeWidget::branch:open:has-children:!has-siblings,"
            "QTreeWidget::branch:open:has-children:has-siblings {"
            "  image: url(%2);"
            "}"
        }.arg(closedPath, openPath));

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

        std::unordered_set<uint32_t> expandedIndices;
        QTreeWidgetItemIterator it{m_tree};
        while (*it)
        {
            if ((*it)->isExpanded())
            {
                expandedIndices.insert((*it)->data(0, Qt::UserRole).toUInt());
            }
            ++it;
        }

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

        std::sort(roots.begin(), roots.end(), [&world](queen::Entity a, queen::Entity b) {
            auto* sa = world.Get<waggle::SiblingIndex>(a);
            auto* sb = world.Get<waggle::SiblingIndex>(b);
            uint32_t ia = sa ? sa->m_value : UINT32_MAX;
            uint32_t ib = sb ? sb->m_value : UINT32_MAX;
            if (ia != ib)
                return ia < ib;
            return a.Index() < b.Index();
        });

        for (queen::Entity root : roots)
        {
            if (world.IsAlive(root))
            {
                AddEntityNode(world, root, nullptr);
            }
        }

        for (uint32_t idx : expandedIndices)
        {
            auto found = m_entityItems.find(idx);
            if (found != m_entityItems.end())
            {
                found->second->setExpanded(true);
            }
        }

        m_tree->viewport()->update();
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

        item->setForeground(0, world.Has<waggle::HierarchyDisabled>(entity) ? DISABLED_COLOR : ENABLED_COLOR);

        if (m_selection.IsSelected(entity))
        {
            item->setSelected(true);
        }

        world.ForEachChild(entity, [&](queen::Entity child) { AddEntityNode(world, child, item); });
    }

    void HierarchyPanel::RefreshEntityItem(queen::Entity entity)
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

        it->second->setForeground(0, m_currentWorld->Has<waggle::HierarchyDisabled>(entity) ? DISABLED_COLOR
                                                                                            : ENABLED_COLOR);
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

    void HierarchyPanel::ReindexRoots()
    {
        if (!m_currentWorld)
            return;

        wax::Vector<queen::Entity> roots{forge::GetAllocator()};
        m_currentWorld->ForEachArchetype([&](auto& arch) {
            if (arch.template HasComponent<queen::Parent>())
                return;
            for (uint32_t row = 0; row < arch.EntityCount(); ++row)
                roots.PushBack(arch.GetEntity(row));
        });

        std::sort(roots.begin(), roots.end(), [this](queen::Entity a, queen::Entity b) {
            auto* sa = m_currentWorld->Get<waggle::SiblingIndex>(a);
            auto* sb = m_currentWorld->Get<waggle::SiblingIndex>(b);
            uint32_t ia = sa ? sa->m_value : UINT32_MAX;
            uint32_t ib = sb ? sb->m_value : UINT32_MAX;
            if (ia != ib)
                return ia < ib;
            return a.Index() < b.Index();
        });

        for (size_t i = 0; i < roots.Size(); ++i)
        {
            m_currentWorld->Set(roots[i], waggle::SiblingIndex{static_cast<uint32_t>(i)});
        }
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

            bool isDisabled = m_currentWorld->Has<waggle::Disabled>(entity);
            menu.addAction(isDisabled ? "Enable" : "Disable", [this, entity, isDisabled]() {
                auto before = std::make_shared<std::vector<std::pair<queen::Entity, bool>>>();

                if (m_selection.All().Size() > 1 && m_selection.IsSelected(entity))
                {
                    for (size_t i = 0; i < m_selection.All().Size(); ++i)
                    {
                        queen::Entity e = m_selection.All()[i];
                        before->push_back({e, m_currentWorld->Has<waggle::Disabled>(e)});
                    }
                }
                else
                {
                    before->push_back({entity, isDisabled});
                }

                bool nowDisabling = !isDisabled;
                for (auto& [e, _] : *before)
                {
                    waggle::SetEntityDisabled(*m_currentWorld, e, nowDisabling);
                }

                m_editorUndo.Push(
                    [this, before]() {
                        for (auto& [e, wasDisabled] : *before)
                        {
                            waggle::SetEntityDisabled(*m_currentWorld, e, wasDisabled);
                        }
                    },
                    [this, before, nowDisabling]() {
                        for (auto& [e, _] : *before)
                        {
                            waggle::SetEntityDisabled(*m_currentWorld, e, nowDisabling);
                        }
                    });

                emit sceneModified();
            });

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

                auto created = std::make_shared<queen::Entity>(entity);
                m_editorUndo.Push(
                    [this, created]() {
                        if (m_currentWorld->IsAlive(*created))
                        {
                            m_currentWorld->DespawnRecursive(*created);
                            m_selection.Clear();
                            Refresh(*m_currentWorld);
                        }
                    },
                    [this, created]() {
                        if (!m_currentWorld->IsAlive(*created))
                        {
                            auto e = m_currentWorld->Spawn().Build();
                            *created = e;
                            char buf[32];
                            snprintf(buf, sizeof(buf), "Entity %u", e.Index());
                            m_currentWorld->Set(e, waggle::Name{wax::FixedString{buf}});
                        }
                        m_selection.Select(*created);
                        Refresh(*m_currentWorld);
                    });

                m_selection.Select(entity);
                Refresh(*m_currentWorld);
                emit sceneModified();
                emit entitySelected(entity.Index());
            });
        }

        menu.exec(m_tree->viewport()->mapToGlobal(pos));
    }
} // namespace forge
