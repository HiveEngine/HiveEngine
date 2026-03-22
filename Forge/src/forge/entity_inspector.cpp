#include <hive/math/types.h>

#include <wax/containers/fixed_string.h>

#include <queen/core/type_id.h>
#include <queen/reflect/component_registry.h>
#include <queen/reflect/field_info.h>
#include <queen/world/world.h>

#include <forge/editor_undo.h>
#include <forge/entity_inspector.h>
#include <forge/inspector_widgets.h>
#include <forge/selection.h>
#include <forge/undo.h>

#include <QCheckBox>
#include <QColorDialog>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QVBoxLayout>

#include <cmath>
#include <cstring>
#include <memory>
#include <vector>

namespace forge
{
    namespace
    {
        struct SnapshotState
        {
            std::byte m_before[64]{};
            uint16_t m_size{0};
            queen::Entity m_entity{};
            queen::TypeId m_typeId{0};
            uint16_t m_offset{0};
        };

        void Snapshot(SnapshotState& state, queen::Entity entity, queen::TypeId typeId, uint16_t offset, uint16_t size,
                      const void* current)
        {
            state.m_entity = entity;
            state.m_typeId = typeId;
            state.m_offset = offset;
            state.m_size = size;
            if (size <= sizeof(state.m_before))
            {
                std::memcpy(state.m_before, current, size);
            }
        }

        void CommitIfChanged(SnapshotState& state, UndoStack& undo, const void* current)
        {
            if (state.m_size == 0)
            {
                return;
            }
            if (std::memcmp(state.m_before, current, state.m_size) != 0)
            {
                undo.PushSetField(state.m_entity, state.m_typeId, state.m_offset, state.m_size, state.m_before,
                                  current);
            }
            state.m_size = 0;
        }

        bool AreFieldValuesUniform(queen::World& world, const wax::Vector<queen::Entity>& entities,
                                   queen::TypeId typeId, uint16_t fieldOffset, uint16_t fieldSize)
        {
            if (entities.Size() <= 1)
                return true;

            void* firstComp = world.GetComponentRaw(entities[0], typeId);
            if (firstComp == nullptr)
                return true;

            auto* firstBytes = static_cast<std::byte*>(firstComp) + fieldOffset;

            for (size_t i = 1; i < entities.Size(); ++i)
            {
                void* comp = world.GetComponentRaw(entities[i], typeId);
                if (comp == nullptr)
                    return false;

                auto* bytes = static_cast<std::byte*>(comp) + fieldOffset;
                if (std::memcmp(firstBytes, bytes, fieldSize) != 0)
                    return false;
            }
            return true;
        }

        template <typename T>
        void ApplyMultiEdit(queen::World& world, const wax::Vector<queen::Entity>& entities, queen::TypeId typeId,
                            uint16_t fieldOffset, T newValue, EditorUndoManager& editorUndo, EntityInspector* panel)
        {
            auto beforeSnapshots = std::make_shared<std::vector<std::pair<queen::Entity, T>>>();
            for (size_t i = 0; i < entities.Size(); ++i)
            {
                void* comp = world.GetComponentRaw(entities[i], typeId);
                if (comp == nullptr)
                    continue;
                auto* ptr = reinterpret_cast<T*>(static_cast<std::byte*>(comp) + fieldOffset);
                beforeSnapshots->push_back({entities[i], *ptr});
                *ptr = newValue;
            }

            auto capturedEntities = std::make_shared<wax::Vector<queen::Entity>>(entities);
            editorUndo.Push(
                [&world, typeId, fieldOffset, beforeSnapshots] {
                    for (auto& [e, before] : *beforeSnapshots)
                    {
                        void* comp = world.GetComponentRaw(e, typeId);
                        if (comp != nullptr)
                        {
                            *reinterpret_cast<T*>(static_cast<std::byte*>(comp) + fieldOffset) = before;
                        }
                    }
                },
                [&world, typeId, fieldOffset, newValue, capturedEntities] {
                    for (size_t i = 0; i < capturedEntities->Size(); ++i)
                    {
                        void* comp = world.GetComponentRaw((*capturedEntities)[i], typeId);
                        if (comp != nullptr)
                        {
                            *reinterpret_cast<T*>(static_cast<std::byte*>(comp) + fieldOffset) = newValue;
                        }
                    }
                });

            emit panel->sceneModified();
        }
    } // namespace

    EntityInspector::EntityInspector(queen::World& world, EditorSelection& selection,
                                     const queen::ComponentRegistry<256>& registry, UndoStack& undo,
                                     EditorUndoManager& editorUndo, QWidget* parent)
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
            BuildSingleEntity(world, selection.Primary(), registry, undo);
        }
        m_rootLayout->addStretch(1);
    }

    void EntityInspector::BuildSingleEntity(queen::World& world, queen::Entity entity,
                                            const queen::ComponentRegistry<256>& registry, UndoStack& undo)
    {
        if (entity.IsNull() || !world.IsAlive(entity))
            return;

        auto* header = new QLabel{QString{"Entity %1"}.arg(entity.Index())};
        header->setObjectName("inspectorHeader");
        header->setContentsMargins(2, 0, 0, 0);
        m_rootLayout->addWidget(header);

        world.ForEachComponentType(entity, [&](queen::TypeId typeId) {
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
            FieldContext ctx{&world, entity, typeId, 0, &undo};
            for (size_t i = 0; i < reflection.m_fieldCount; ++i)
                BuildFieldWidget(reflection.m_fields[i], comp, ctx, form);
            m_rootLayout->addWidget(group);
        });
    }

    void EntityInspector::BuildMultiEntity(queen::World& world, const wax::Vector<queen::Entity>& entities,
                                           const queen::ComponentRegistry<256>& registry, EditorUndoManager& editorUndo)
    {
        auto* header = new QLabel{QString{"%1 entities selected"}.arg(entities.Size())};
        header->setObjectName("inspectorHeader");
        header->setContentsMargins(2, 0, 0, 0);
        m_rootLayout->addWidget(header);

        queen::Entity primary = entities[0];
        if (primary.IsNull() || !world.IsAlive(primary))
            return;

        std::vector<queen::TypeId> commonTypes;
        world.ForEachComponentType(primary, [&](queen::TypeId typeId) {
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
            {
                BuildMultiFieldWidget(reflection.m_fields[i], primaryComp, ctx, form);
            }

            m_rootLayout->addWidget(group);
        }
    }

    void EntityInspector::BuildFieldWidget(const queen::FieldInfo& field, void* data, const FieldContext& ctx,
                                           QFormLayout* form)
    {
        if (HasFlag(field, queen::FieldFlag::HIDDEN))
        {
            return;
        }

        const char* label = GetDisplayName(field);
        void* fieldData = static_cast<std::byte*>(data) + field.m_offset;
        const uint16_t offset = static_cast<uint16_t>(ctx.m_baseOffset + field.m_offset);
        const bool readOnly = HasFlag(field, queen::FieldFlag::READ_ONLY);
        auto& undo = *ctx.m_undo;
        const auto entity = ctx.m_entity;
        const auto typeId = ctx.m_typeId;

        QWidget* widget = nullptr;

        switch (field.m_type)
        {
            case queen::FieldType::FLOAT32: {
                auto* value = static_cast<float*>(fieldData);
                auto* spin = MakeDoubleSpinBox(static_cast<double>(*value));
                ApplyRange(spin, field);

                if (HasFlag(field, queen::FieldFlag::ANGLE))
                {
                    spin->setSuffix(QString::fromUtf8("\xC2\xB0"));
                    spin->setValue(static_cast<double>(*value * DEG_PER_RAD));
                }

                auto snapshot = std::make_shared<SnapshotState>();

                QObject::connect(spin, &QDoubleSpinBox::editingFinished, this,
                                 [this, spin, value, snapshot, entity, typeId, offset, &undo,
                                  isAngle = HasFlag(field, queen::FieldFlag::ANGLE)]() {
                                     float newVal = static_cast<float>(spin->value());
                                     if (isAngle)
                                     {
                                         newVal *= RAD_PER_DEG;
                                     }
                                     if (newVal != *value)
                                     {
                                         CommitIfChanged(*snapshot, undo, value);
                                         Snapshot(*snapshot, entity, typeId, offset, sizeof(float), value);
                                         *value = newVal;
                                         CommitIfChanged(*snapshot, undo, value);
                                         emit sceneModified();
                                     }
                                 });

                widget = spin;
                break;
            }

            case queen::FieldType::FLOAT64: {
                auto* value = static_cast<double*>(fieldData);
                auto* spin = MakeDoubleSpinBox(*value);

                auto snapshot = std::make_shared<SnapshotState>();

                QObject::connect(spin, &QDoubleSpinBox::editingFinished, this,
                                 [this, spin, value, snapshot, entity, typeId, offset, &undo]() {
                                     double newVal = spin->value();
                                     if (newVal != *value)
                                     {
                                         Snapshot(*snapshot, entity, typeId, offset,
                                                  static_cast<uint16_t>(sizeof(double)), value);
                                         *value = newVal;
                                         CommitIfChanged(*snapshot, undo, value);
                                         emit sceneModified();
                                     }
                                 });

                widget = spin;
                break;
            }

            case queen::FieldType::INT32: {
                auto* value = static_cast<int32_t*>(fieldData);
                auto* spin = new QSpinBox;
                spin->setMinimum(std::numeric_limits<int32_t>::min());
                spin->setMaximum(std::numeric_limits<int32_t>::max());
                if (field.m_attributes != nullptr && field.m_attributes->HasRange())
                {
                    spin->setMinimum(static_cast<int>(field.m_attributes->m_min));
                    spin->setMaximum(static_cast<int>(field.m_attributes->m_max));
                }
                spin->setValue(*value);

                auto snapshot = std::make_shared<SnapshotState>();

                QObject::connect(spin, &QSpinBox::editingFinished, this,
                                 [this, spin, value, snapshot, entity, typeId, offset, &undo]() {
                                     int32_t newVal = spin->value();
                                     if (newVal != *value)
                                     {
                                         Snapshot(*snapshot, entity, typeId, offset, sizeof(int32_t), value);
                                         *value = newVal;
                                         CommitIfChanged(*snapshot, undo, value);
                                         emit sceneModified();
                                     }
                                 });

                widget = spin;
                break;
            }

            case queen::FieldType::UINT32: {
                auto* value = static_cast<uint32_t*>(fieldData);
                auto* spin = new QSpinBox;
                spin->setMinimum(0);
                spin->setMaximum(std::numeric_limits<int>::max());
                spin->setValue(static_cast<int>(*value));

                auto snapshot = std::make_shared<SnapshotState>();

                QObject::connect(spin, &QSpinBox::editingFinished, this,
                                 [this, spin, value, snapshot, entity, typeId, offset, &undo]() {
                                     auto newVal = static_cast<uint32_t>(spin->value());
                                     if (newVal != *value)
                                     {
                                         Snapshot(*snapshot, entity, typeId, offset, sizeof(uint32_t), value);
                                         *value = newVal;
                                         CommitIfChanged(*snapshot, undo, value);
                                         emit sceneModified();
                                     }
                                 });

                widget = spin;
                break;
            }

            case queen::FieldType::BOOL: {
                auto* value = static_cast<bool*>(fieldData);
                auto* check = new QCheckBox;
                check->setChecked(*value);

                QObject::connect(
                    check, &QCheckBox::toggled, this, [this, value, entity, typeId, offset, &undo](bool checked) {
                        bool before = *value;
                        *value = checked;
                        undo.PushSetField(entity, typeId, offset, static_cast<uint16_t>(sizeof(bool)), &before, value);
                        emit sceneModified();
                    });

                widget = check;
                break;
            }

            case queen::FieldType::STRUCT: {
                if (field.m_nestedTypeId == queen::TypeIdOf<hive::math::Float3>())
                {
                    auto* floats = static_cast<float*>(fieldData);

                    if (HasFlag(field, queen::FieldFlag::COLOR))
                    {
                        QColor initial =
                            QColor::fromRgbF(static_cast<double>(floats[0]), static_cast<double>(floats[1]),
                                             static_cast<double>(floats[2]));

                        auto* btn = new QPushButton{initial.name()};
                        btn->setStyleSheet(QString{"background-color: %1;"}.arg(initial.name()));

                        auto snapshot = std::make_shared<SnapshotState>();

                        QObject::connect(
                            btn, &QPushButton::clicked, this,
                            [this, btn, floats, snapshot, entity, typeId, offset, &undo]() {
                                QColor cur =
                                    QColor::fromRgbF(static_cast<double>(floats[0]), static_cast<double>(floats[1]),
                                                     static_cast<double>(floats[2]));

                                Snapshot(*snapshot, entity, typeId, offset, 12, floats);

                                QColor picked = QColorDialog::getColor(cur, btn);
                                if (picked.isValid())
                                {
                                    floats[0] = static_cast<float>(picked.redF());
                                    floats[1] = static_cast<float>(picked.greenF());
                                    floats[2] = static_cast<float>(picked.blueF());
                                    btn->setText(picked.name());
                                    btn->setStyleSheet(QString{"background-color: %1;"}.arg(picked.name()));
                                    CommitIfChanged(*snapshot, undo, floats);
                                    emit sceneModified();
                                }
                                else
                                {
                                    snapshot->m_size = 0;
                                }
                            });

                        widget = btn;
                    }
                    else
                    {
                        AxisSpinConfig configs[3];
                        for (int i = 0; i < 3; ++i)
                        {
                            configs[i].m_value = static_cast<double>(floats[i]);
                            if (field.m_attributes != nullptr && field.m_attributes->HasRange())
                            {
                                configs[i].m_min = static_cast<double>(field.m_attributes->m_min);
                                configs[i].m_max = static_cast<double>(field.m_attributes->m_max);
                            }
                        }

                        auto snapshot = std::make_shared<SnapshotState>();

                        widget = BuildAxisRow(
                            3, configs,
                            [this, floats, snapshot, entity, typeId, offset, &undo](int axis, float newVal) {
                                if (newVal != floats[axis])
                                {
                                    if (snapshot->m_size == 0)
                                    {
                                        Snapshot(*snapshot, entity, typeId, offset, 12, floats);
                                    }
                                    floats[axis] = newVal;
                                    CommitIfChanged(*snapshot, undo, floats);
                                    emit sceneModified();
                                }
                            });
                    }
                }
                else if (field.m_nestedTypeId == queen::TypeIdOf<hive::math::Quat>())
                {
                    auto* q = static_cast<float*>(fieldData);

                    // Quat to Euler (same math as ImGui version)
                    auto quatToEuler = [](const float* quat, float* euler) {
                        const float sinr = 2.f * (quat[3] * quat[0] + quat[1] * quat[2]);
                        const float cosr = 1.f - 2.f * (quat[0] * quat[0] + quat[1] * quat[1]);
                        euler[0] = std::atan2(sinr, cosr) * DEG_PER_RAD;

                        const float sinp = 2.f * (quat[3] * quat[1] - quat[2] * quat[0]);
                        if (std::fabs(sinp) >= 1.f)
                        {
                            euler[1] = std::copysign(90.f, sinp);
                        }
                        else
                        {
                            euler[1] = std::asin(sinp) * DEG_PER_RAD;
                        }

                        const float siny = 2.f * (quat[3] * quat[2] + quat[0] * quat[1]);
                        const float cosy = 1.f - 2.f * (quat[1] * quat[1] + quat[2] * quat[2]);
                        euler[2] = std::atan2(siny, cosy) * DEG_PER_RAD;
                    };

                    auto eulerToQuat = [](const float* euler, float* quat) {
                        const float p = euler[0] * RAD_PER_DEG * 0.5f;
                        const float y = euler[1] * RAD_PER_DEG * 0.5f;
                        const float r = euler[2] * RAD_PER_DEG * 0.5f;

                        const float cp = std::cos(p);
                        const float sp = std::sin(p);
                        const float cy = std::cos(y);
                        const float sy = std::sin(y);
                        const float cr = std::cos(r);
                        const float sr = std::sin(r);

                        quat[0] = sr * cp * cy - cr * sp * sy;
                        quat[1] = cr * sp * cy + sr * cp * sy;
                        quat[2] = cr * cp * sy - sr * sp * cy;
                        quat[3] = cr * cp * cy + sr * sp * sy;
                    };

                    float euler[3];
                    quatToEuler(q, euler);

                    static constexpr const char* DEG_SUFFIX = "\xC2\xB0";
                    AxisSpinConfig configs[3];
                    for (int i = 0; i < 3; ++i)
                    {
                        configs[i] = {static_cast<double>(euler[i]), 0.5, -360.0, 360.0, DEG_SUFFIX};
                    }

                    auto snapshot = std::make_shared<SnapshotState>();

                    widget = BuildAxisRow(3, configs,
                                          [this, q, snapshot, entity, typeId, offset, &undo, quatToEuler,
                                           eulerToQuat](int axis, float newDeg) {
                                              float curEuler[3];
                                              quatToEuler(q, curEuler);
                                              if (newDeg != curEuler[axis])
                                              {
                                                  if (snapshot->m_size == 0)
                                                  {
                                                      Snapshot(*snapshot, entity, typeId, offset, 16, q);
                                                  }
                                                  curEuler[axis] = newDeg;
                                                  eulerToQuat(curEuler, q);
                                                  CommitIfChanged(*snapshot, undo, q);
                                                  emit sceneModified();
                                              }
                                          });
                }
                else if (field.m_nestedFields != nullptr && field.m_nestedFieldCount > 0)
                {
                    auto* nestedGroup = new QGroupBox{QString::fromUtf8(label)};
                    auto* nestedForm = new QFormLayout{nestedGroup};

                    FieldContext nestedCtx{ctx.m_world, ctx.m_entity, ctx.m_typeId,
                                           static_cast<uint16_t>(ctx.m_baseOffset + field.m_offset), ctx.m_undo};

                    for (size_t i = 0; i < field.m_nestedFieldCount; ++i)
                    {
                        BuildFieldWidget(field.m_nestedFields[i], fieldData, nestedCtx, nestedForm);
                    }

                    form->addRow(nestedGroup);
                    return;
                }
                else
                {
                    widget = new QLabel{"(unsupported)"};
                    widget->setEnabled(false);
                }
                break;
            }

            case queen::FieldType::ENUM: {
                if (field.m_enumInfo != nullptr && field.m_enumInfo->IsValid())
                {
                    auto* combo = new QComboBox;

                    int64_t currentValue = 0;
                    std::memcpy(&currentValue, fieldData, field.m_size <= 8 ? field.m_size : size_t{8});

                    int currentIndex = -1;
                    for (size_t i = 0; i < field.m_enumInfo->m_entryCount; ++i)
                    {
                        const auto& entry = field.m_enumInfo->m_entries[i];
                        combo->addItem(QString::fromUtf8(entry.m_name),
                                       QVariant{static_cast<qlonglong>(entry.m_value)});
                        if (entry.m_value == currentValue)
                        {
                            currentIndex = static_cast<int>(i);
                        }
                    }
                    if (currentIndex >= 0)
                    {
                        combo->setCurrentIndex(currentIndex);
                    }

                    QObject::connect(
                        combo, &QComboBox::currentIndexChanged, this,
                        [this, combo, fieldData, fieldSize = field.m_size, entity, typeId, offset, &undo](int index) {
                            if (index < 0)
                            {
                                return;
                            }
                            int64_t newValue = combo->itemData(index).toLongLong();
                            std::byte before[8]{};
                            std::memcpy(before, fieldData, fieldSize);
                            std::memcpy(fieldData, &newValue, fieldSize);
                            undo.PushSetField(entity, typeId, offset, static_cast<uint16_t>(fieldSize), before,
                                              fieldData);
                            emit sceneModified();
                        });

                    widget = combo;
                }
                else
                {
                    widget = new QLabel{"(unsupported)"};
                    widget->setEnabled(false);
                }
                break;
            }

            case queen::FieldType::STRING: {
                auto* value = static_cast<wax::FixedString*>(fieldData);
                auto* lineEdit = new QLineEdit{QString::fromUtf8(value->CStr(), static_cast<int>(value->Size()))};
                lineEdit->setMaxLength(static_cast<int>(wax::FixedString::MaxCapacity));

                auto snapshot = std::make_shared<SnapshotState>();
                auto snapshotTaken = std::make_shared<bool>(false);

                QObject::connect(lineEdit, &QLineEdit::textEdited, this,
                                 [this, value, snapshot, snapshotTaken, entity, typeId, offset](const QString& text) {
                                     if (!*snapshotTaken)
                                     {
                                         Snapshot(*snapshot, entity, typeId, offset,
                                                  static_cast<uint16_t>(sizeof(wax::FixedString)), value);
                                         *snapshotTaken = true;
                                     }
                                     QByteArray utf8 = text.toUtf8();
                                     *value = wax::FixedString{utf8.constData(), static_cast<size_t>(utf8.size())};
                                     emit entityLabelChanged(entity);
                                 });

                QObject::connect(lineEdit, &QLineEdit::editingFinished, this,
                                 [this, value, snapshot, snapshotTaken, &undo]() {
                                     if (*snapshotTaken)
                                     {
                                         CommitIfChanged(*snapshot, undo, value);
                                         *snapshotTaken = false;
                                         emit sceneModified();
                                     }
                                 });

                widget = lineEdit;
                break;
            }

            default: {
                widget = new QLabel{"(unsupported)"};
                widget->setEnabled(false);
                break;
            }
        }

        if (widget == nullptr)
        {
            return;
        }

        if (readOnly)
        {
            widget->setEnabled(false);
        }

        ApplyTooltip(widget, field);
        form->addRow(label, widget);
    }

    void EntityInspector::BuildMultiFieldWidget(const queen::FieldInfo& field, void* primaryData,
                                                const MultiFieldContext& ctx, QFormLayout* form)
    {
        if (HasFlag(field, queen::FieldFlag::HIDDEN))
            return;

        const char* label = GetDisplayName(field);
        void* fieldData = static_cast<std::byte*>(primaryData) + field.m_offset;
        const uint16_t offset = static_cast<uint16_t>(ctx.m_baseOffset + field.m_offset);
        const auto& entities = *ctx.m_entities;
        auto& world = *ctx.m_world;
        auto typeId = ctx.m_typeId;
        auto* editorUndo = ctx.m_editorUndo;

        bool uniform = AreFieldValuesUniform(world, entities, typeId, offset, static_cast<uint16_t>(field.m_size));

        switch (field.m_type)
        {
            case queen::FieldType::FLOAT32: {
                auto* spin = MakeDoubleSpinBox(uniform ? static_cast<double>(*static_cast<float*>(fieldData)) : 0.0);
                ApplyRange(spin, field);
                ApplyTooltip(spin, field);
                if (!uniform)
                {
                    spin->setSpecialValueText("--");
                    spin->setMinimum(-1e15);
                    spin->setValue(spin->minimum());
                }
                connect(spin, &QDoubleSpinBox::editingFinished, this,
                        [this, spin, &world, entities = entities, typeId, offset, editorUndo] {
                            ApplyMultiEdit<float>(world, entities, typeId, offset, static_cast<float>(spin->value()),
                                                  *editorUndo, this);
                        });
                form->addRow(label, spin);
                break;
            }

            case queen::FieldType::INT32: {
                auto* spin = new QSpinBox;
                spin->setRange(-999999, 999999);
                if (uniform)
                {
                    spin->setValue(*static_cast<int32_t*>(fieldData));
                }
                else
                {
                    spin->setSpecialValueText("--");
                    spin->setMinimum(-999999);
                    spin->setValue(spin->minimum());
                }
                ApplyTooltip(spin, field);
                connect(spin, &QSpinBox::editingFinished, this,
                        [this, spin, &world, entities = entities, typeId, offset, editorUndo] {
                            ApplyMultiEdit<int32_t>(world, entities, typeId, offset,
                                                    static_cast<int32_t>(spin->value()), *editorUndo, this);
                        });
                form->addRow(label, spin);
                break;
            }

            case queen::FieldType::UINT32: {
                auto* spin = new QSpinBox;
                spin->setRange(0, 999999);
                if (uniform)
                {
                    spin->setValue(static_cast<int>(*static_cast<uint32_t*>(fieldData)));
                }
                else
                {
                    spin->setSpecialValueText("--");
                    spin->setMinimum(-1);
                    spin->setValue(spin->minimum());
                }
                ApplyTooltip(spin, field);
                connect(spin, &QSpinBox::editingFinished, this,
                        [this, spin, &world, entities = entities, typeId, offset, editorUndo] {
                            ApplyMultiEdit<uint32_t>(world, entities, typeId, offset,
                                                     static_cast<uint32_t>(spin->value()), *editorUndo, this);
                        });
                form->addRow(label, spin);
                break;
            }

            case queen::FieldType::BOOL: {
                auto* check = new QCheckBox;
                check->setTristate(true);
                if (uniform)
                {
                    check->setCheckState(*static_cast<bool*>(fieldData) ? Qt::Checked : Qt::Unchecked);
                }
                else
                {
                    check->setCheckState(Qt::PartiallyChecked);
                }
                ApplyTooltip(check, field);
                connect(check, &QCheckBox::stateChanged, this,
                        [this, &world, entities = entities, typeId, offset, editorUndo](int state) {
                            if (state == Qt::PartiallyChecked)
                                return;
                            ApplyMultiEdit<bool>(world, entities, typeId, offset, state == Qt::Checked, *editorUndo,
                                                 this);
                        });
                form->addRow(label, check);
                break;
            }

            case queen::FieldType::STRUCT: {
                if (field.m_nestedTypeId == queen::TypeIdOf<hive::math::Float3>())
                {
                    auto* floats = static_cast<float*>(fieldData);
                    AxisSpinConfig configs[3];
                    for (int i = 0; i < 3; ++i)
                    {
                        bool axisUniform = AreFieldValuesUniform(world, entities, typeId,
                                                                 static_cast<uint16_t>(offset + i * sizeof(float)),
                                                                 static_cast<uint16_t>(sizeof(float)));

                        if (axisUniform)
                        {
                            configs[i].m_value = static_cast<double>(floats[i]);
                        }
                        else
                        {
                            configs[i].m_value = -1e15;
                            configs[i].m_min = -1e15;
                        }
                    }

                    auto* row = BuildAxisRow(
                        3, configs,
                        [this, &world, entities = entities, typeId, offset, editorUndo](int axis, float newVal) {
                            auto beforeSnapshots = std::make_shared<std::vector<std::pair<queen::Entity, float>>>();
                            uint16_t axisOffset = static_cast<uint16_t>(offset + axis * sizeof(float));
                            for (size_t i = 0; i < entities.Size(); ++i)
                            {
                                void* comp = world.GetComponentRaw(entities[i], typeId);
                                if (comp == nullptr)
                                    continue;
                                auto* ptr = reinterpret_cast<float*>(static_cast<std::byte*>(comp) + axisOffset);
                                beforeSnapshots->push_back({entities[i], *ptr});
                                *ptr = newVal;
                            }

                            auto capturedEntities = std::make_shared<wax::Vector<queen::Entity>>(entities);
                            editorUndo->Push(
                                [&world, typeId, axisOffset, beforeSnapshots] {
                                    for (auto& [e, before] : *beforeSnapshots)
                                    {
                                        void* comp = world.GetComponentRaw(e, typeId);
                                        if (comp != nullptr)
                                            *reinterpret_cast<float*>(static_cast<std::byte*>(comp) + axisOffset) =
                                                before;
                                    }
                                },
                                [&world, typeId, axisOffset, newVal, capturedEntities] {
                                    for (size_t i = 0; i < capturedEntities->Size(); ++i)
                                    {
                                        void* comp = world.GetComponentRaw((*capturedEntities)[i], typeId);
                                        if (comp != nullptr)
                                            *reinterpret_cast<float*>(static_cast<std::byte*>(comp) + axisOffset) =
                                                newVal;
                                    }
                                });

                            emit sceneModified();
                        });
                    form->addRow(label, row);
                }
                else if (field.m_nestedTypeId == queen::TypeIdOf<hive::math::Quat>())
                {
                    auto* primaryQ = static_cast<float*>(fieldData);

                    auto quatToEuler = [](const float* quat, float* euler) {
                        const float sinr = 2.f * (quat[3] * quat[0] + quat[1] * quat[2]);
                        const float cosr = 1.f - 2.f * (quat[0] * quat[0] + quat[1] * quat[1]);
                        euler[0] = std::atan2(sinr, cosr) * DEG_PER_RAD;

                        const float sinp = 2.f * (quat[3] * quat[1] - quat[2] * quat[0]);
                        euler[1] = std::fabs(sinp) >= 1.f ? std::copysign(90.f, sinp) : std::asin(sinp) * DEG_PER_RAD;

                        const float siny = 2.f * (quat[3] * quat[2] + quat[0] * quat[1]);
                        const float cosy = 1.f - 2.f * (quat[1] * quat[1] + quat[2] * quat[2]);
                        euler[2] = std::atan2(siny, cosy) * DEG_PER_RAD;
                    };

                    auto eulerToQuat = [](const float* euler, float* quat) {
                        const float p = euler[0] * RAD_PER_DEG * 0.5f;
                        const float y = euler[1] * RAD_PER_DEG * 0.5f;
                        const float r = euler[2] * RAD_PER_DEG * 0.5f;
                        const float cp = std::cos(p), sp = std::sin(p);
                        const float cy = std::cos(y), sy = std::sin(y);
                        const float cr = std::cos(r), sr = std::sin(r);
                        quat[0] = sr * cp * cy - cr * sp * sy;
                        quat[1] = cr * sp * cy + sr * cp * sy;
                        quat[2] = cr * cp * sy - sr * sp * cy;
                        quat[3] = cr * cp * cy + sr * sp * sy;
                    };

                    float euler[3];
                    quatToEuler(primaryQ, euler);

                    static constexpr const char* DEG_SUFFIX_MULTI = "\xC2\xB0";
                    AxisSpinConfig configs[3];
                    bool quatUniform = AreFieldValuesUniform(world, entities, typeId, offset, 16);
                    for (int i = 0; i < 3; ++i)
                    {
                        configs[i] = {quatUniform ? static_cast<double>(euler[i]) : -1e15, 0.5,
                                      quatUniform ? -360.0 : -1e15, 360.0, DEG_SUFFIX_MULTI};
                    }

                    auto* row = BuildAxisRow(
                        3, configs,
                        [this, &world, entities = entities, typeId, offset, editorUndo, eulerToQuat,
                         quatToEuler](int axis, float newDeg) {
                            struct QuatSnapshot
                            {
                                queen::Entity e;
                                float q[4];
                            };
                            auto before = std::make_shared<std::vector<QuatSnapshot>>();

                            for (size_t i = 0; i < entities.Size(); ++i)
                            {
                                void* comp = world.GetComponentRaw(entities[i], typeId);
                                if (comp == nullptr)
                                    continue;
                                auto* q = reinterpret_cast<float*>(static_cast<std::byte*>(comp) + offset);
                                QuatSnapshot snap;
                                snap.e = entities[i];
                                std::memcpy(snap.q, q, 16);
                                before->push_back(snap);

                                float curEuler[3];
                                quatToEuler(q, curEuler);
                                curEuler[axis] = newDeg;
                                eulerToQuat(curEuler, q);
                            }

                            auto capturedEntities = std::make_shared<wax::Vector<queen::Entity>>(entities);
                            auto afterEulerAxis = newDeg;
                            editorUndo->Push(
                                [&world, typeId, offset, before] {
                                    for (auto& snap : *before)
                                    {
                                        void* comp = world.GetComponentRaw(snap.e, typeId);
                                        if (comp != nullptr)
                                            std::memcpy(static_cast<std::byte*>(comp) + offset, snap.q, 16);
                                    }
                                },
                                [&world, typeId, offset, afterEulerAxis, axis = axis, capturedEntities, quatToEuler,
                                 eulerToQuat] {
                                    for (size_t i = 0; i < capturedEntities->Size(); ++i)
                                    {
                                        void* comp = world.GetComponentRaw((*capturedEntities)[i], typeId);
                                        if (comp == nullptr)
                                            continue;
                                        auto* q = reinterpret_cast<float*>(static_cast<std::byte*>(comp) + offset);
                                        float curEuler[3];
                                        quatToEuler(q, curEuler);
                                        curEuler[axis] = afterEulerAxis;
                                        eulerToQuat(curEuler, q);
                                    }
                                });

                            emit sceneModified();
                        });
                    form->addRow(label, row);
                }
                else if (field.m_nestedFields != nullptr)
                {
                    auto* group = new QGroupBox{QString::fromUtf8(label)};
                    auto* nestedForm = new QFormLayout{group};
                    nestedForm->setContentsMargins(4, 4, 4, 4);
                    nestedForm->setSpacing(3);
                    nestedForm->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);

                    MultiFieldContext nestedCtx{ctx.m_world, ctx.m_entities, ctx.m_typeId, offset, ctx.m_editorUndo};
                    for (size_t i = 0; i < field.m_nestedFieldCount; ++i)
                    {
                        BuildMultiFieldWidget(field.m_nestedFields[i], fieldData, nestedCtx, nestedForm);
                    }
                    form->addRow(group);
                }
                break;
            }

            default: {
                auto* lbl = new QLabel{uniform ? "--" : "(mixed)"};
                lbl->setStyleSheet("color: #555; font-style: italic;");
                form->addRow(label, lbl);
                break;
            }
        }
    }
} // namespace forge
