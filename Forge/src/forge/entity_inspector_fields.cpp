#include <hive/math/types.h>

#include <wax/containers/fixed_string.h>

#include <queen/core/type_id.h>
#include <queen/reflect/enum_reflection.h>
#include <queen/reflect/field_info.h>
#include <queen/world/world.h>

#include <forge/editor_undo.h>
#include <forge/entity_inspector.h>
#include <forge/entity_inspector_helpers.h>
#include <forge/inspector_widgets.h>

#include <QCheckBox>
#include <QColorDialog>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSpinBox>

#include <cmath>
#include <cstring>
#include <limits>
#include <memory>

namespace forge
{
    void EntityInspector::BuildFieldWidget(const queen::FieldInfo& field, void* data,
                                           const FieldContext& ctx, QFormLayout* form)
    {
        if (HasFlag(field, queen::FieldFlag::HIDDEN))
            return;

        const char* label = GetDisplayName(field);
        void* fieldData = static_cast<std::byte*>(data) + field.m_offset;
        const uint16_t offset = static_cast<uint16_t>(ctx.m_baseOffset + field.m_offset);
        const bool readOnly = HasFlag(field, queen::FieldFlag::READ_ONLY);
        auto& undo = *ctx.m_undo;
        auto& world = *ctx.m_world;
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
                    spin->setSuffix(QString::fromUtf8(DEG_SUFFIX));
                    spin->setValue(static_cast<double>(*value * DEG_PER_RAD));
                }

                auto snapshot = std::make_shared<SnapshotState>();

                QObject::connect(
                    spin, &QDoubleSpinBox::editingFinished, this,
                    [this, spin, value, snapshot, entity, typeId, offset, &undo, &world,
                     isAngle = HasFlag(field, queen::FieldFlag::ANGLE)]() {
                        float newVal = static_cast<float>(spin->value());
                        if (isAngle)
                            newVal *= RAD_PER_DEG;
                        if (newVal != *value)
                        {
                            CommitIfChanged(*snapshot, undo, world, value);
                            Snapshot(*snapshot, entity, typeId, offset, sizeof(float), value);
                            *value = newVal;
                            CommitIfChanged(*snapshot, undo, world, value);
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

                QObject::connect(
                    spin, &QDoubleSpinBox::editingFinished, this,
                    [this, spin, value, snapshot, entity, typeId, offset, &undo, &world]() {
                        double newVal = spin->value();
                        if (newVal != *value)
                        {
                            Snapshot(*snapshot, entity, typeId, offset,
                                     static_cast<uint16_t>(sizeof(double)), value);
                            *value = newVal;
                            CommitIfChanged(*snapshot, undo, world, value);
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

                QObject::connect(
                    spin, &QSpinBox::editingFinished, this,
                    [this, spin, value, snapshot, entity, typeId, offset, &undo, &world]() {
                        int32_t newVal = spin->value();
                        if (newVal != *value)
                        {
                            Snapshot(*snapshot, entity, typeId, offset, sizeof(int32_t), value);
                            *value = newVal;
                            CommitIfChanged(*snapshot, undo, world, value);
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

                QObject::connect(
                    spin, &QSpinBox::editingFinished, this,
                    [this, spin, value, snapshot, entity, typeId, offset, &undo, &world]() {
                        auto newVal = static_cast<uint32_t>(spin->value());
                        if (newVal != *value)
                        {
                            Snapshot(*snapshot, entity, typeId, offset, sizeof(uint32_t), value);
                            *value = newVal;
                            CommitIfChanged(*snapshot, undo, world, value);
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
                    check, &QCheckBox::toggled, this,
                    [this, value, entity, typeId, offset, &undo, &world](bool checked) {
                        bool before = *value;
                        *value = checked;
                        undo.Push(
                            [&world, entity, typeId, offset, before]() {
                                void* comp = world.GetComponentRaw(entity, typeId);
                                if (comp != nullptr)
                                    *reinterpret_cast<bool*>(
                                        static_cast<std::byte*>(comp) + offset) = before;
                            },
                            [&world, entity, typeId, offset, checked]() {
                                void* comp = world.GetComponentRaw(entity, typeId);
                                if (comp != nullptr)
                                    *reinterpret_cast<bool*>(
                                        static_cast<std::byte*>(comp) + offset) = checked;
                            });
                        emit sceneModified();
                    });

                widget = check;
                break;
            }

            case queen::FieldType::STRUCT: {
                widget = BuildStructFieldWidget(field, fieldData, ctx, form);
                if (widget == nullptr)
                    return;
                break;
            }

            case queen::FieldType::ENUM: {
                widget = BuildEnumFieldWidget(field, fieldData, ctx);
                break;
            }

            case queen::FieldType::STRING: {
                auto* value = static_cast<wax::FixedString*>(fieldData);
                auto* lineEdit = new QLineEdit{
                    QString::fromUtf8(value->CStr(), static_cast<int>(value->Size()))};
                lineEdit->setMaxLength(static_cast<int>(wax::FixedString::MaxCapacity));

                auto snapshot = std::make_shared<SnapshotState>();
                auto snapshotTaken = std::make_shared<bool>(false);

                QObject::connect(
                    lineEdit, &QLineEdit::textEdited, this,
                    [value, snapshot, snapshotTaken, entity, typeId, offset](const QString& text) {
                        if (!*snapshotTaken)
                        {
                            Snapshot(*snapshot, entity, typeId, offset,
                                     static_cast<uint16_t>(sizeof(wax::FixedString)), value);
                            *snapshotTaken = true;
                        }
                        QByteArray utf8 = text.toUtf8();
                        *value = wax::FixedString{utf8.constData(),
                                                   static_cast<size_t>(utf8.size())};
                    });

                QObject::connect(
                    lineEdit, &QLineEdit::editingFinished, this,
                    [this, value, snapshot, snapshotTaken, &undo, &world]() {
                        if (*snapshotTaken)
                        {
                            CommitIfChanged(*snapshot, undo, world, value);
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
            return;

        if (readOnly)
            widget->setEnabled(false);

        ApplyTooltip(widget, field);
        form->addRow(label, widget);
    }

    QWidget* EntityInspector::BuildStructFieldWidget(const queen::FieldInfo& field, void* fieldData,
                                                      const FieldContext& ctx, QFormLayout* form)
    {
        auto& undo = *ctx.m_undo;
        auto& world = *ctx.m_world;
        const auto entity = ctx.m_entity;
        const auto typeId = ctx.m_typeId;
        const uint16_t offset = static_cast<uint16_t>(ctx.m_baseOffset + field.m_offset);

        if (field.m_nestedTypeId == queen::TypeIdOf<hive::math::Float3>())
        {
            auto* floats = static_cast<float*>(fieldData);

            if (HasFlag(field, queen::FieldFlag::COLOR))
            {
                QColor initial = QColor::fromRgbF(
                    static_cast<double>(floats[0]), static_cast<double>(floats[1]),
                    static_cast<double>(floats[2]));

                auto* btn = new QPushButton{initial.name()};
                btn->setStyleSheet(QString{"background-color: %1;"}.arg(initial.name()));

                auto snapshot = std::make_shared<SnapshotState>();

                QObject::connect(
                    btn, &QPushButton::clicked, this,
                    [this, btn, floats, snapshot, entity, typeId, offset, &undo, &world]() {
                        QColor cur = QColor::fromRgbF(
                            static_cast<double>(floats[0]), static_cast<double>(floats[1]),
                            static_cast<double>(floats[2]));
                        Snapshot(*snapshot, entity, typeId, offset, 12, floats);
                        QColor picked = QColorDialog::getColor(cur, btn);
                        if (picked.isValid())
                        {
                            floats[0] = static_cast<float>(picked.redF());
                            floats[1] = static_cast<float>(picked.greenF());
                            floats[2] = static_cast<float>(picked.blueF());
                            btn->setText(picked.name());
                            btn->setStyleSheet(
                                QString{"background-color: %1;"}.arg(picked.name()));
                            CommitIfChanged(*snapshot, undo, world, floats);
                            emit sceneModified();
                        }
                        else
                        {
                            snapshot->m_size = 0;
                        }
                    });

                return btn;
            }

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
            return BuildAxisRow(
                3, configs,
                [this, floats, snapshot, entity, typeId, offset, &undo, &world](
                    int axis, float newVal) {
                    if (newVal != floats[axis])
                    {
                        if (snapshot->m_size == 0)
                            Snapshot(*snapshot, entity, typeId, offset, 12, floats);
                        floats[axis] = newVal;
                        CommitIfChanged(*snapshot, undo, world, floats);
                        emit sceneModified();
                    }
                });
        }

        if (field.m_nestedTypeId == queen::TypeIdOf<hive::math::Quat>())
        {
            auto* q = static_cast<float*>(fieldData);
            float euler[3];
            QuatToEuler(q, euler);

            AxisSpinConfig configs[3];
            for (int i = 0; i < 3; ++i)
                configs[i] = {static_cast<double>(euler[i]), 0.5, -360.0, 360.0, DEG_SUFFIX};

            auto snapshot = std::make_shared<SnapshotState>();
            return BuildAxisRow(
                3, configs,
                [this, q, snapshot, entity, typeId, offset, &undo, &world](
                    int axis, float newDeg) {
                    float curEuler[3];
                    QuatToEuler(q, curEuler);
                    if (newDeg != curEuler[axis])
                    {
                        if (snapshot->m_size == 0)
                            Snapshot(*snapshot, entity, typeId, offset, 16, q);
                        curEuler[axis] = newDeg;
                        EulerToQuat(curEuler, q);
                        CommitIfChanged(*snapshot, undo, world, q);
                        emit sceneModified();
                    }
                });
        }

        if (field.m_nestedFields != nullptr && field.m_nestedFieldCount > 0)
        {
            const char* label = GetDisplayName(field);
            auto* nestedGroup = new QGroupBox{QString::fromUtf8(label)};
            auto* nestedForm = new QFormLayout{nestedGroup};

            FieldContext nestedCtx{ctx.m_world, ctx.m_entity, ctx.m_typeId,
                                   static_cast<uint16_t>(ctx.m_baseOffset + field.m_offset),
                                   ctx.m_undo};
            for (size_t i = 0; i < field.m_nestedFieldCount; ++i)
                BuildFieldWidget(field.m_nestedFields[i], fieldData, nestedCtx, nestedForm);
            form->addRow(nestedGroup);
            return nullptr;
        }

        auto* lbl = new QLabel{"(unsupported)"};
        lbl->setEnabled(false);
        return lbl;
    }

    QWidget* EntityInspector::BuildEnumFieldWidget(const queen::FieldInfo& field, void* fieldData,
                                                    const FieldContext& ctx)
    {
        if (field.m_enumInfo == nullptr || !field.m_enumInfo->IsValid())
        {
            auto* lbl = new QLabel{"(unsupported)"};
            lbl->setEnabled(false);
            return lbl;
        }

        auto& undo = *ctx.m_undo;
        auto& world = *ctx.m_world;
        const auto entity = ctx.m_entity;
        const auto typeId = ctx.m_typeId;
        const uint16_t offset = static_cast<uint16_t>(ctx.m_baseOffset + field.m_offset);

        auto* combo = new QComboBox;

        int64_t currentValue = 0;
        std::memcpy(&currentValue, fieldData,
                     field.m_size <= 8 ? field.m_size : size_t{8});

        int currentIndex = -1;
        for (size_t i = 0; i < field.m_enumInfo->m_entryCount; ++i)
        {
            const auto& entry = field.m_enumInfo->m_entries[i];
            combo->addItem(QString::fromUtf8(entry.m_name),
                           QVariant{static_cast<qlonglong>(entry.m_value)});
            if (entry.m_value == currentValue)
                currentIndex = static_cast<int>(i);
        }
        if (currentIndex >= 0)
            combo->setCurrentIndex(currentIndex);

        QObject::connect(
            combo, &QComboBox::currentIndexChanged, this,
            [this, combo, fieldData, fieldSize = field.m_size, entity, typeId,
             offset, &undo, &world](int index) {
                if (index < 0)
                    return;
                int64_t newValue = combo->itemData(index).toLongLong();
                auto beforeData = std::make_shared<std::vector<std::byte>>(
                    static_cast<std::byte*>(fieldData),
                    static_cast<std::byte*>(fieldData) + fieldSize);
                std::memcpy(fieldData, &newValue, fieldSize);
                auto afterData = std::make_shared<std::vector<std::byte>>(
                    static_cast<std::byte*>(fieldData),
                    static_cast<std::byte*>(fieldData) + fieldSize);
                undo.Push(
                    [&world, entity, typeId, offset, fieldSize, beforeData]() {
                        void* comp = world.GetComponentRaw(entity, typeId);
                        if (comp != nullptr)
                            std::memcpy(static_cast<std::byte*>(comp) + offset,
                                        beforeData->data(), fieldSize);
                    },
                    [&world, entity, typeId, offset, fieldSize, afterData]() {
                        void* comp = world.GetComponentRaw(entity, typeId);
                        if (comp != nullptr)
                            std::memcpy(static_cast<std::byte*>(comp) + offset,
                                        afterData->data(), fieldSize);
                    });
                emit sceneModified();
            });

        return combo;
    }
} // namespace forge
