#include <hive/math/types.h>

#include <queen/core/type_id.h>
#include <queen/reflect/component_registry.h>
#include <queen/reflect/field_attributes.h>
#include <queen/reflect/field_info.h>
#include <queen/world/world.h>

#include <forge/inspector_panel.h>
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
#include <QPushButton>
#include <QSpinBox>
#include <QVBoxLayout>
#include <QWidget>

#include <cmath>
#include <cstring>
#include <memory>

namespace forge
{
    namespace
    {
        constexpr float kDeg = 180.f / hive::math::kPi;
        constexpr float kRad = hive::math::kPi / 180.f;

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

        [[nodiscard]] const char* GetDisplayName(const queen::FieldInfo& field) noexcept
        {
            if (field.m_attributes != nullptr && field.m_attributes->m_displayName != nullptr)
            {
                return field.m_attributes->m_displayName;
            }
            return field.m_name;
        }

        [[nodiscard]] bool HasFlag(const queen::FieldInfo& field, queen::FieldFlag flag) noexcept
        {
            return field.m_attributes != nullptr && field.m_attributes->HasFlag(flag);
        }

        void ApplyTooltip(QWidget* widget, const queen::FieldInfo& field)
        {
            if (field.m_attributes != nullptr && field.m_attributes->m_tooltip != nullptr)
            {
                widget->setToolTip(QString::fromUtf8(field.m_attributes->m_tooltip));
            }
        }

        void ApplyRange(QDoubleSpinBox* spin, const queen::FieldInfo& field)
        {
            if (field.m_attributes != nullptr && field.m_attributes->HasRange())
            {
                spin->setMinimum(static_cast<double>(field.m_attributes->m_min));
                spin->setMaximum(static_cast<double>(field.m_attributes->m_max));
            }
            else
            {
                spin->setMinimum(-1e9);
                spin->setMaximum(1e9);
            }
        }

        QDoubleSpinBox* MakeDoubleSpinBox(double value, double step = 0.01)
        {
            auto* spin = new QDoubleSpinBox;
            spin->setDecimals(4);
            spin->setSingleStep(step);
            spin->setMinimum(-1e9);
            spin->setMaximum(1e9);
            spin->setValue(value);
            return spin;
        }
    } // namespace

    InspectorPanel::InspectorPanel(QWidget* parent)
        : QScrollArea{parent}
    {
        setWidgetResizable(true);
    }

    void InspectorPanel::Refresh(queen::World& world, EditorSelection& selection,
                                 const queen::ComponentRegistry<256>& registry, UndoStack& undo)
    {
        auto* content = new QWidget;
        auto* rootLayout = new QVBoxLayout{content};
        rootLayout->setContentsMargins(4, 4, 4, 4);

        const queen::Entity entity = selection.Primary();
        if (entity.IsNull() || !world.IsAlive(entity))
        {
            auto* label = new QLabel{"No entity selected"};
            label->setEnabled(false);
            rootLayout->addWidget(label);
            rootLayout->addStretch();
            setWidget(content);
            return;
        }

        auto* header = new QLabel{QString{"Entity %1"}.arg(entity.Index())};
        rootLayout->addWidget(header);

        world.ForEachComponentType(entity, [&](queen::TypeId typeId) {
            const auto* reg = registry.Find(typeId);
            if (reg == nullptr || !reg->HasReflection())
            {
                return;
            }

            void* comp = world.GetComponentRaw(entity, typeId);
            if (comp == nullptr)
            {
                return;
            }

            const auto& reflection = reg->m_reflection;
            const char* name = reflection.m_name != nullptr ? reflection.m_name : "Component";

            auto* group = new QGroupBox{QString::fromUtf8(name)};
            group->setCheckable(true);
            group->setChecked(true);
            auto* form = new QFormLayout{group};

            FieldContext ctx{&world, entity, typeId, 0, &undo};

            for (size_t i = 0; i < reflection.m_fieldCount; ++i)
            {
                BuildFieldWidget(reflection.m_fields[i], comp, ctx, form);
            }

            rootLayout->addWidget(group);
        });

        rootLayout->addStretch();
        setWidget(content);
    }

    void InspectorPanel::BuildFieldWidget(const queen::FieldInfo& field, void* data, const FieldContext& ctx,
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
                    spin->setValue(static_cast<double>(*value * kDeg));
                }

                auto snapshot = std::make_shared<SnapshotState>();

                QObject::connect(spin, &QDoubleSpinBox::editingFinished, this,
                                 [this, spin, value, snapshot, entity, typeId, offset, &undo,
                                  isAngle = HasFlag(field, queen::FieldFlag::ANGLE)]() {
                                     float newVal = static_cast<float>(spin->value());
                                     if (isAngle)
                                     {
                                         newVal *= kRad;
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
                        auto* container = new QWidget;
                        auto* hbox = new QHBoxLayout{container};
                        hbox->setContentsMargins(0, 0, 0, 0);

                        auto snapshot = std::make_shared<SnapshotState>();

                        for (int axis = 0; axis < 3; ++axis)
                        {
                            auto* spin = MakeDoubleSpinBox(static_cast<double>(floats[axis]));
                            ApplyRange(spin, field);
                            hbox->addWidget(spin);

                            QObject::connect(spin, &QDoubleSpinBox::editingFinished, this,
                                             [this, spin, floats, axis, snapshot, entity, typeId, offset, &undo]() {
                                                 float newVal = static_cast<float>(spin->value());
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

                        widget = container;
                    }
                }
                else if (field.m_nestedTypeId == queen::TypeIdOf<hive::math::Quat>())
                {
                    auto* q = static_cast<float*>(fieldData);

                    // Quat to Euler (same math as ImGui version)
                    auto quatToEuler = [](const float* quat, float* euler) {
                        const float sinr = 2.f * (quat[3] * quat[0] + quat[1] * quat[2]);
                        const float cosr = 1.f - 2.f * (quat[0] * quat[0] + quat[1] * quat[1]);
                        euler[0] = std::atan2(sinr, cosr) * kDeg;

                        const float sinp = 2.f * (quat[3] * quat[1] - quat[2] * quat[0]);
                        if (std::fabs(sinp) >= 1.f)
                        {
                            euler[1] = std::copysign(90.f, sinp);
                        }
                        else
                        {
                            euler[1] = std::asin(sinp) * kDeg;
                        }

                        const float siny = 2.f * (quat[3] * quat[2] + quat[0] * quat[1]);
                        const float cosy = 1.f - 2.f * (quat[1] * quat[1] + quat[2] * quat[2]);
                        euler[2] = std::atan2(siny, cosy) * kDeg;
                    };

                    auto eulerToQuat = [](const float* euler, float* quat) {
                        const float p = euler[0] * kRad * 0.5f;
                        const float y = euler[1] * kRad * 0.5f;
                        const float r = euler[2] * kRad * 0.5f;

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

                    auto* container = new QWidget;
                    auto* hbox = new QHBoxLayout{container};
                    hbox->setContentsMargins(0, 0, 0, 0);

                    auto snapshot = std::make_shared<SnapshotState>();

                    for (int axis = 0; axis < 3; ++axis)
                    {
                        auto* spin = MakeDoubleSpinBox(static_cast<double>(euler[axis]), 0.5);
                        spin->setSuffix(QString::fromUtf8("\xC2\xB0"));
                        spin->setMinimum(-360.0);
                        spin->setMaximum(360.0);
                        hbox->addWidget(spin);

                        QObject::connect(
                            spin, &QDoubleSpinBox::editingFinished, this,
                            [this, spin, q, axis, snapshot, entity, typeId, offset, &undo, quatToEuler, eulerToQuat]() {
                                float curEuler[3];
                                quatToEuler(q, curEuler);
                                float newDeg = static_cast<float>(spin->value());
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

                    widget = container;
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
        form->addRow(QString::fromUtf8(label), widget);
    }
} // namespace forge
