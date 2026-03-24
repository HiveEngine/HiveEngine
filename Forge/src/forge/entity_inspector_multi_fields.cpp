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
    QWidget* EntityInspector::BuildMultiEnumFieldWidget(const queen::FieldInfo& field,
                                                        void* fieldData,
                                                        const MultiFieldContext& ctx)
    {
        if (field.m_enumInfo == nullptr || !field.m_enumInfo->IsValid())
            return nullptr;

        auto& world = *ctx.m_world;
        const auto& entities = *ctx.m_entities;
        auto typeId = ctx.m_typeId;
        const uint16_t offset = static_cast<uint16_t>(ctx.m_baseOffset + field.m_offset);
        auto* editorUndo = ctx.m_editorUndo;
        bool uniform = AreFieldValuesUniform(
            world, entities, typeId, offset, static_cast<uint16_t>(field.m_size));

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

        if (!uniform)
        {
            combo->insertItem(0, "--", QVariant{qlonglong{-1}});
            combo->setCurrentIndex(0);
        }
        else if (currentIndex >= 0)
        {
            combo->setCurrentIndex(currentIndex);
        }

        ApplyTooltip(combo, field);
        connect(
            combo, &QComboBox::currentIndexChanged, this,
            [this, combo, &world, entities = entities, typeId, offset,
             fieldSize = field.m_size, editorUndo](int index) {
                if (index < 0)
                    return;
                int64_t newValue = combo->itemData(index).toLongLong();
                if (newValue == -1)
                    return;

                struct EnumSnap
                {
                    queen::Entity e;
                    int64_t val;
                };
                auto before = std::make_shared<std::vector<EnumSnap>>();
                for (size_t i = 0; i < entities.Size(); ++i)
                {
                    void* comp = world.GetComponentRaw(entities[i], typeId);
                    if (comp == nullptr)
                        continue;
                    auto* ptr = static_cast<std::byte*>(comp) + offset;
                    int64_t old = 0;
                    std::memcpy(&old, ptr, fieldSize);
                    before->push_back({entities[i], old});
                    std::memcpy(ptr, &newValue, fieldSize);
                }

                auto captured =
                    std::make_shared<wax::Vector<queen::Entity>>(entities);
                editorUndo->Push(
                    [&world, typeId, offset, fieldSize, before] {
                        for (auto& snap : *before)
                        {
                            void* comp = world.GetComponentRaw(snap.e, typeId);
                            if (comp != nullptr)
                                std::memcpy(static_cast<std::byte*>(comp) + offset,
                                            &snap.val, fieldSize);
                        }
                    },
                    [&world, typeId, offset, fieldSize, newValue, captured] {
                        for (size_t i = 0; i < captured->Size(); ++i)
                        {
                            void* comp =
                                world.GetComponentRaw((*captured)[i], typeId);
                            if (comp != nullptr)
                                std::memcpy(static_cast<std::byte*>(comp) + offset,
                                            &newValue, fieldSize);
                        }
                    });
                emit sceneModified();
            });
        return combo;
    }

    QWidget* EntityInspector::BuildMultiStructFieldWidget(
        const queen::FieldInfo& field, void* fieldData,
        const MultiFieldContext& ctx, QFormLayout* form)
    {
        auto& world = *ctx.m_world;
        const auto& entities = *ctx.m_entities;
        auto typeId = ctx.m_typeId;
        const uint16_t offset = static_cast<uint16_t>(ctx.m_baseOffset + field.m_offset);
        auto* editorUndo = ctx.m_editorUndo;

        if (field.m_nestedTypeId == queen::TypeIdOf<hive::math::Float3>())
        {
            auto* floats = static_cast<float*>(fieldData);

            if (HasFlag(field, queen::FieldFlag::COLOR))
            {
                bool uniform = AreFieldValuesUniform(world, entities, typeId, offset, 12);
                QColor initial = uniform
                    ? QColor::fromRgbF(static_cast<double>(floats[0]),
                                       static_cast<double>(floats[1]),
                                       static_cast<double>(floats[2]))
                    : QColor{0x33, 0x33, 0x33};

                auto* btn = new QPushButton{uniform ? initial.name() : "--"};
                btn->setStyleSheet(
                    QString{"background-color: %1;"}.arg(initial.name()));
                ApplyTooltip(btn, field);

                connect(
                    btn, &QPushButton::clicked, this,
                    [this, btn, &world, entities = entities, typeId, offset, editorUndo] {
                        void* first = world.GetComponentRaw(entities[0], typeId);
                        if (first == nullptr)
                            return;
                        auto* f = reinterpret_cast<float*>(
                            static_cast<std::byte*>(first) + offset);
                        QColor cur = QColor::fromRgbF(
                            static_cast<double>(f[0]), static_cast<double>(f[1]),
                            static_cast<double>(f[2]));

                        QColor picked = QColorDialog::getColor(cur, btn);
                        if (!picked.isValid())
                            return;

                        struct ColorSnap
                        {
                            queen::Entity e;
                            float rgb[3];
                        };
                        auto before = std::make_shared<std::vector<ColorSnap>>();
                        float nr = static_cast<float>(picked.redF());
                        float ng = static_cast<float>(picked.greenF());
                        float nb = static_cast<float>(picked.blueF());

                        for (size_t i = 0; i < entities.Size(); ++i)
                        {
                            void* comp = world.GetComponentRaw(entities[i], typeId);
                            if (comp == nullptr)
                                continue;
                            auto* rgb = reinterpret_cast<float*>(
                                static_cast<std::byte*>(comp) + offset);
                            before->push_back({entities[i], {rgb[0], rgb[1], rgb[2]}});
                            rgb[0] = nr;
                            rgb[1] = ng;
                            rgb[2] = nb;
                        }

                        btn->setText(picked.name());
                        btn->setStyleSheet(
                            QString{"background-color: %1;"}.arg(picked.name()));

                        auto captured =
                            std::make_shared<wax::Vector<queen::Entity>>(entities);
                        editorUndo->Push(
                            [&world, typeId, offset, before] {
                                for (auto& snap : *before)
                                {
                                    void* comp = world.GetComponentRaw(snap.e, typeId);
                                    if (comp == nullptr)
                                        continue;
                                    auto* rgb = reinterpret_cast<float*>(
                                        static_cast<std::byte*>(comp) + offset);
                                    rgb[0] = snap.rgb[0];
                                    rgb[1] = snap.rgb[1];
                                    rgb[2] = snap.rgb[2];
                                }
                            },
                            [&world, typeId, offset, nr, ng, nb, captured] {
                                for (size_t i = 0; i < captured->Size(); ++i)
                                {
                                    void* comp =
                                        world.GetComponentRaw((*captured)[i], typeId);
                                    if (comp == nullptr)
                                        continue;
                                    auto* rgb = reinterpret_cast<float*>(
                                        static_cast<std::byte*>(comp) + offset);
                                    rgb[0] = nr;
                                    rgb[1] = ng;
                                    rgb[2] = nb;
                                }
                            });
                        emit sceneModified();
                    });
                return btn;
            }

            AxisSpinConfig configs[3];
            for (int i = 0; i < 3; ++i)
            {
                bool axisUniform = AreFieldValuesUniform(
                    world, entities, typeId,
                    static_cast<uint16_t>(offset + i * sizeof(float)),
                    static_cast<uint16_t>(sizeof(float)));
                configs[i].m_value = axisUniform ? static_cast<double>(floats[i]) : -1e15;
                if (!axisUniform)
                    configs[i].m_min = -1e15;
            }

            return BuildAxisRow(
                3, configs,
                [this, &world, entities = entities, typeId, offset, editorUndo](
                    int axis, float newVal) {
                    auto before =
                        std::make_shared<std::vector<std::pair<queen::Entity, float>>>();
                    uint16_t axisOff =
                        static_cast<uint16_t>(offset + axis * sizeof(float));
                    for (size_t i = 0; i < entities.Size(); ++i)
                    {
                        void* comp = world.GetComponentRaw(entities[i], typeId);
                        if (comp == nullptr)
                            continue;
                        auto* ptr = reinterpret_cast<float*>(
                            static_cast<std::byte*>(comp) + axisOff);
                        before->push_back({entities[i], *ptr});
                        *ptr = newVal;
                    }

                    auto captured =
                        std::make_shared<wax::Vector<queen::Entity>>(entities);
                    editorUndo->Push(
                        [&world, typeId, axisOff, before] {
                            for (auto& [e, val] : *before)
                            {
                                void* comp = world.GetComponentRaw(e, typeId);
                                if (comp != nullptr)
                                    *reinterpret_cast<float*>(
                                        static_cast<std::byte*>(comp) + axisOff) = val;
                            }
                        },
                        [&world, typeId, axisOff, newVal, captured] {
                            for (size_t i = 0; i < captured->Size(); ++i)
                            {
                                void* comp =
                                    world.GetComponentRaw((*captured)[i], typeId);
                                if (comp != nullptr)
                                    *reinterpret_cast<float*>(
                                        static_cast<std::byte*>(comp) + axisOff) = newVal;
                            }
                        });
                    emit sceneModified();
                });
        }

        if (field.m_nestedTypeId == queen::TypeIdOf<hive::math::Quat>())
        {
            auto* primaryQ = static_cast<float*>(fieldData);
            float euler[3];
            QuatToEuler(primaryQ, euler);

            bool quatUniform = AreFieldValuesUniform(world, entities, typeId, offset, 16);
            AxisSpinConfig configs[3];
            for (int i = 0; i < 3; ++i)
            {
                configs[i] = {quatUniform ? static_cast<double>(euler[i]) : -1e15, 0.5,
                              quatUniform ? -360.0 : -1e15, 360.0, DEG_SUFFIX};
            }

            return BuildAxisRow(
                3, configs,
                [this, &world, entities = entities, typeId, offset, editorUndo](
                    int axis, float newDeg) {
                    struct QuatSnap
                    {
                        queen::Entity e;
                        float q[4];
                    };
                    auto before = std::make_shared<std::vector<QuatSnap>>();

                    for (size_t i = 0; i < entities.Size(); ++i)
                    {
                        void* comp = world.GetComponentRaw(entities[i], typeId);
                        if (comp == nullptr)
                            continue;
                        auto* q = reinterpret_cast<float*>(
                            static_cast<std::byte*>(comp) + offset);
                        QuatSnap snap;
                        snap.e = entities[i];
                        std::memcpy(snap.q, q, 16);
                        before->push_back(snap);

                        float curEuler[3];
                        QuatToEuler(q, curEuler);
                        curEuler[axis] = newDeg;
                        EulerToQuat(curEuler, q);
                    }

                    auto captured =
                        std::make_shared<wax::Vector<queen::Entity>>(entities);
                    editorUndo->Push(
                        [&world, typeId, offset, before] {
                            for (auto& snap : *before)
                            {
                                void* comp = world.GetComponentRaw(snap.e, typeId);
                                if (comp != nullptr)
                                    std::memcpy(static_cast<std::byte*>(comp) + offset,
                                                snap.q, 16);
                            }
                        },
                        [&world, typeId, offset, newDeg, axis = axis, captured] {
                            for (size_t i = 0; i < captured->Size(); ++i)
                            {
                                void* comp =
                                    world.GetComponentRaw((*captured)[i], typeId);
                                if (comp == nullptr)
                                    continue;
                                auto* q = reinterpret_cast<float*>(
                                    static_cast<std::byte*>(comp) + offset);
                                float curEuler[3];
                                QuatToEuler(q, curEuler);
                                curEuler[axis] = newDeg;
                                EulerToQuat(curEuler, q);
                            }
                        });
                    emit sceneModified();
                });
        }

        if (field.m_nestedFields != nullptr)
        {
            const char* label = GetDisplayName(field);
            auto* group = new QGroupBox{QString::fromUtf8(label)};
            auto* nestedForm = new QFormLayout{group};
            nestedForm->setContentsMargins(4, 4, 4, 4);
            nestedForm->setSpacing(3);
            nestedForm->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);

            MultiFieldContext nestedCtx{ctx.m_world, ctx.m_entities, ctx.m_typeId, offset,
                                         ctx.m_editorUndo};
            for (size_t i = 0; i < field.m_nestedFieldCount; ++i)
                BuildMultiFieldWidget(field.m_nestedFields[i], fieldData, nestedCtx, nestedForm);
            form->addRow(group);
            return nullptr;
        }

        return nullptr;
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

        bool uniform = AreFieldValuesUniform(
            world, entities, typeId, offset, static_cast<uint16_t>(field.m_size));

        switch (field.m_type)
        {
            case queen::FieldType::FLOAT32: {
                auto* spin = MakeDoubleSpinBox(
                    uniform ? static_cast<double>(*static_cast<float*>(fieldData)) : 0.0);
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
                            ApplyMultiEdit<float>(world, entities, typeId, offset,
                                                  static_cast<float>(spin->value()), *editorUndo);
                            emit sceneModified();
                        });
                form->addRow(label, spin);
                break;
            }

            case queen::FieldType::INT32: {
                auto* spin = new QSpinBox;
                spin->setRange(-999999, 999999);
                if (uniform)
                    spin->setValue(*static_cast<int32_t*>(fieldData));
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
                                                    static_cast<int32_t>(spin->value()),
                                                    *editorUndo);
                            emit sceneModified();
                        });
                form->addRow(label, spin);
                break;
            }

            case queen::FieldType::UINT32: {
                auto* spin = new QSpinBox;
                spin->setRange(0, 999999);
                if (uniform)
                    spin->setValue(static_cast<int>(*static_cast<uint32_t*>(fieldData)));
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
                                                     static_cast<uint32_t>(spin->value()),
                                                     *editorUndo);
                            emit sceneModified();
                        });
                form->addRow(label, spin);
                break;
            }

            case queen::FieldType::BOOL: {
                auto* check = new QCheckBox;
                check->setTristate(true);
                if (uniform)
                    check->setCheckState(
                        *static_cast<bool*>(fieldData) ? Qt::Checked : Qt::Unchecked);
                else
                    check->setCheckState(Qt::PartiallyChecked);
                ApplyTooltip(check, field);
                connect(check, &QCheckBox::stateChanged, this,
                        [this, &world, entities = entities, typeId, offset, editorUndo](int state) {
                            if (state == Qt::PartiallyChecked)
                                return;
                            ApplyMultiEdit<bool>(world, entities, typeId, offset,
                                                 state == Qt::Checked, *editorUndo);
                            emit sceneModified();
                        });
                form->addRow(label, check);
                break;
            }

            case queen::FieldType::FLOAT64: {
                auto* spin = MakeDoubleSpinBox(
                    uniform ? *static_cast<double*>(fieldData) : 0.0);
                ApplyTooltip(spin, field);
                if (!uniform)
                {
                    spin->setSpecialValueText("--");
                    spin->setMinimum(-1e15);
                    spin->setValue(spin->minimum());
                }
                connect(spin, &QDoubleSpinBox::editingFinished, this,
                        [this, spin, &world, entities = entities, typeId, offset, editorUndo] {
                            ApplyMultiEdit<double>(world, entities, typeId, offset,
                                                   spin->value(), *editorUndo);
                            emit sceneModified();
                        });
                form->addRow(label, spin);
                break;
            }

            case queen::FieldType::ENUM: {
                auto* w = BuildMultiEnumFieldWidget(field, fieldData, ctx);
                if (w != nullptr)
                    form->addRow(label, w);
                break;
            }

            case queen::FieldType::STRING: {
                auto* value = static_cast<wax::FixedString*>(fieldData);
                auto* lineEdit = new QLineEdit;
                if (uniform)
                    lineEdit->setText(QString::fromUtf8(
                        value->CStr(), static_cast<int>(value->Size())));
                else
                    lineEdit->setPlaceholderText("--");
                lineEdit->setMaxLength(static_cast<int>(wax::FixedString::MaxCapacity));
                ApplyTooltip(lineEdit, field);
                connect(lineEdit, &QLineEdit::editingFinished, this,
                        [this, lineEdit, &world, entities = entities, typeId, offset, editorUndo] {
                            QByteArray utf8 = lineEdit->text().toUtf8();
                            wax::FixedString newVal{
                                utf8.constData(), static_cast<size_t>(utf8.size())};
                            ApplyMultiEdit<wax::FixedString>(
                                world, entities, typeId, offset, newVal, *editorUndo);
                            emit sceneModified();
                        });
                form->addRow(label, lineEdit);
                break;
            }

            case queen::FieldType::STRUCT: {
                auto* w = BuildMultiStructFieldWidget(field, fieldData, ctx, form);
                if (w != nullptr)
                    form->addRow(label, w);
                break;
            }

            default: {
                auto* lbl = new QLabel{"--"};
                lbl->setStyleSheet("color: #555; font-style: italic;");
                form->addRow(label, lbl);
                break;
            }
        }
    }
} // namespace forge
