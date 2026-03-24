#include <wax/containers/fixed_string.h>

#include <queen/core/type_id.h>
#include <queen/reflect/component_registry.h>
#include <queen/world/world.h>

#include <waggle/components/disabled.h>
#include <waggle/components/name.h>
#include <waggle/disabled_propagation.h>

#include <forge/editor_undo.h>
#include <forge/entity_inspector.h>
#include <forge/entity_inspector_helpers.h>
#include <forge/selection.h>

#include <QCheckBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QVBoxLayout>

#include <cstring>
#include <vector>

namespace forge
{
    EntityInspector::EntityInspector(queen::World& world, EditorSelection& selection,
                                     const queen::ComponentRegistry<256>& registry, EditorUndoManager& editorUndo,
                                     QWidget* parent)
        : QWidget{parent}
    {
        m_rootLayout = new QVBoxLayout{this};
        m_rootLayout->setContentsMargins(4, 2, 4, 4);
        m_rootLayout->setSpacing(1);

        if (selection.All().Size() > 1)
        {
            BuildMultiEntity(world, selection.All(), registry, editorUndo);
        }
        else
        {
            BuildSingleEntity(world, selection.Primary(), registry, editorUndo);
        }
        m_rootLayout->addStretch(1);
    }

    void EntityInspector::BuildSingleEntity(queen::World& world, queen::Entity entity,
                                            const queen::ComponentRegistry<256>& registry,
                                            EditorUndoManager& editorUndo)
    {
        if (entity.IsNull() || !world.IsAlive(entity))
        {
            return;
        }

        constexpr queen::TypeId nameTypeId = queen::TypeIdOf<waggle::Name>();

        auto* nameData = static_cast<waggle::Name*>(world.GetComponentRaw(entity, nameTypeId));

        auto* headerRow = new QHBoxLayout;
        headerRow->setContentsMargins(2, 2, 2, 4);
        headerRow->setSpacing(4);

        auto* enableCheck = new QCheckBox;
        enableCheck->setChecked(!world.Has<waggle::Disabled>(entity));
        enableCheck->setToolTip("Enable/Disable entity");
        headerRow->addWidget(enableCheck);

        auto* nameEdit = new QLineEdit{
            nameData ? QString::fromUtf8(nameData->m_name.CStr(), static_cast<int>(nameData->m_name.Size()))
                     : QString{}};
        nameEdit->setObjectName("inspectorHeader");
        nameEdit->setPlaceholderText(QString{"Entity %1"}.arg(entity.Index()));
        headerRow->addWidget(nameEdit);

        m_rootLayout->addLayout(headerRow);

        QObject::connect(
            enableCheck, &QCheckBox::checkStateChanged, this,
            [this, &world, &editorUndo, entity](Qt::CheckState state) {
                bool disabling = (state != Qt::Checked);
                waggle::SetEntityDisabled(world, entity, disabling);
                editorUndo.Push(
                    [&world, entity, disabling]() {
                        waggle::SetEntityDisabled(world, entity, !disabling);
                    },
                    [&world, entity, disabling]() {
                        waggle::SetEntityDisabled(world, entity, disabling);
                    });
                emit entityLabelChanged(entity);
                emit sceneModified();
            });

        if (!nameData)
        {
            nameEdit->setReadOnly(true);
        }
        else
        {
            auto snapshot = std::make_shared<SnapshotState>();
            auto snapshotTaken = std::make_shared<bool>(false);

            QObject::connect(
                nameEdit, &QLineEdit::textEdited, this,
                [this, nameData, snapshot, snapshotTaken, entity](const QString& text) {
                    if (!*snapshotTaken)
                    {
                        Snapshot(*snapshot, entity, nameTypeId, 0,
                                 static_cast<uint16_t>(sizeof(wax::FixedString)),
                                 &nameData->m_name);
                        *snapshotTaken = true;
                    }
                    QByteArray utf8 = text.toUtf8();
                    nameData->m_name =
                        wax::FixedString{utf8.constData(), static_cast<size_t>(utf8.size())};
                    emit entityLabelChanged(entity);
                });

            QObject::connect(
                nameEdit, &QLineEdit::editingFinished, this,
                [this, nameData, snapshot, snapshotTaken, &world, &editorUndo]() {
                    if (*snapshotTaken)
                    {
                        CommitIfChanged(*snapshot, editorUndo, world, &nameData->m_name);
                        *snapshotTaken = false;
                        emit sceneModified();
                    }
                });
        }

        world.ForEachComponentType(entity, [&](queen::TypeId typeId) {
            if (typeId == nameTypeId)
                return;
            const auto* reg = registry.Find(typeId);
            if (reg == nullptr || !reg->HasReflection())
                return;
            void* comp = world.GetComponentRaw(entity, typeId);
            if (comp == nullptr)
                return;
            const auto& reflection = reg->m_reflection;
            const char* rawName = reflection.m_name != nullptr ? reflection.m_name : "Component";
            const char* name = rawName;
            if (const char* sep = std::strrchr(rawName, ':'))
                name = sep + 1;

            auto* group = new QGroupBox{QString::fromUtf8(name)};
            group->setCheckable(true);
            group->setChecked(true);
            auto* form = new QFormLayout{group};
            form->setContentsMargins(4, 4, 4, 4);
            form->setSpacing(3);
            form->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);
            FieldContext ctx{&world, entity, typeId, 0, &editorUndo};
            for (size_t i = 0; i < reflection.m_fieldCount; ++i)
                BuildFieldWidget(reflection.m_fields[i], comp, ctx, form);
            m_rootLayout->addWidget(group);
        });
    }

    void EntityInspector::BuildMultiEntity(queen::World& world,
                                           const wax::Vector<queen::Entity>& entities,
                                           const queen::ComponentRegistry<256>& registry,
                                           EditorUndoManager& editorUndo)
    {
        constexpr queen::TypeId nameTypeId = queen::TypeIdOf<waggle::Name>();

        auto* headerRow = new QHBoxLayout;
        headerRow->setContentsMargins(2, 2, 2, 4);
        headerRow->setSpacing(4);

        bool allDisabled = true;
        for (size_t i = 0; i < entities.Size(); ++i)
        {
            if (!world.Has<waggle::Disabled>(entities[i]))
            {
                allDisabled = false;
                break;
            }
        }

        auto* enableCheck = new QCheckBox;
        enableCheck->setChecked(!allDisabled);
        enableCheck->setToolTip("Enable/Disable entities");
        headerRow->addWidget(enableCheck);

        auto* header = new QLabel{QString{"%1 entities selected"}.arg(entities.Size())};
        header->setObjectName("inspectorHeader");
        headerRow->addWidget(header);

        m_rootLayout->addLayout(headerRow);

        QObject::connect(
            enableCheck, &QCheckBox::clicked, this,
            [this, &world, &editorUndo, entities = &entities](bool checked) {
                bool disabling = !checked;
                auto before =
                    std::make_shared<std::vector<std::pair<queen::Entity, bool>>>();
                for (size_t i = 0; i < entities->Size(); ++i)
                {
                    before->push_back(
                        {(*entities)[i], world.Has<waggle::Disabled>((*entities)[i])});
                    waggle::SetEntityDisabled(world, (*entities)[i], disabling);
                }
                editorUndo.Push(
                    [&world, before]() {
                        for (auto& [e, wasDisabled] : *before)
                            waggle::SetEntityDisabled(world, e, wasDisabled);
                    },
                    [&world, before, disabling]() {
                        for (auto& [e, _] : *before)
                            waggle::SetEntityDisabled(world, e, disabling);
                    });
                emit sceneModified();
            });

        queen::Entity primary = entities[0];
        if (primary.IsNull() || !world.IsAlive(primary))
            return;

        std::vector<queen::TypeId> commonTypes;
        world.ForEachComponentType(primary, [&](queen::TypeId typeId) {
            if (typeId == nameTypeId)
                return;
            for (size_t i = 0; i < entities.Size(); ++i)
            {
                if (!world.HasComponent(entities[i], typeId))
                    return;
            }
            commonTypes.push_back(typeId);
        });

        for (auto typeId : commonTypes)
        {
            const auto* reg = registry.Find(typeId);
            if (reg == nullptr || !reg->HasReflection())
                continue;
            void* primaryComp = world.GetComponentRaw(primary, typeId);
            if (primaryComp == nullptr)
                continue;

            const auto& reflection = reg->m_reflection;
            const char* rawName = reflection.m_name != nullptr ? reflection.m_name : "Component";
            const char* name = rawName;
            if (const char* sep = std::strrchr(rawName, ':'))
                name = sep + 1;

            auto* group = new QGroupBox{QString::fromUtf8(name)};
            group->setCheckable(true);
            group->setChecked(true);
            auto* form = new QFormLayout{group};
            form->setContentsMargins(4, 4, 4, 4);
            form->setSpacing(3);
            form->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);

            MultiFieldContext ctx{&world, &entities, typeId, 0, &editorUndo};
            for (size_t i = 0; i < reflection.m_fieldCount; ++i)
                BuildMultiFieldWidget(reflection.m_fields[i], primaryComp, ctx, form);
            m_rootLayout->addWidget(group);
        }
    }
} // namespace forge
