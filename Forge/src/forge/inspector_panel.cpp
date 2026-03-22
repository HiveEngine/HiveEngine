#include <forge/inspector_panel.h>

#include <hive/math/types.h>

#include <wax/containers/fixed_string.h>

#include <queen/core/type_id.h>
#include <queen/reflect/component_registry.h>
#include <queen/reflect/field_attributes.h>
#include <queen/reflect/field_info.h>
#include <queen/world/world.h>
#include <nectar/mesh/mesh_data.h>

#include <forge/asset_browser.h>
#include <forge/selection.h>
#include <forge/undo.h>

#include <QCheckBox>
#include <QColorDialog>
#include <QFile>
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
#include <QWidget>

#include <cmath>
#include <cstdio>
#include <cstring>
#include <filesystem>
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

        constexpr const char* kAxisLabels[] = {"X", "Y", "Z", "W"};
        constexpr const char* kAxisColors[] = {"#e05555", "#55b855", "#5580e0", "#c0c0c0"};

        struct AxisSpinConfig
        {
            double m_value;
            double m_step{0.01};
            double m_min{-1e9};
            double m_max{1e9};
            const char* m_suffix{nullptr};
        };

        // Build a horizontal row of N labeled spin boxes (e.g. X/Y/Z or X/Y/Z/W).
        // onChanged is called with (axisIndex, newFloatValue) when any spin box finishes editing.
        template <typename Callback>
        QWidget* BuildAxisRow(int axisCount, const AxisSpinConfig* configs, Callback&& onChanged)
        {
            auto* container = new QWidget;
            auto* hbox = new QHBoxLayout{container};
            hbox->setContentsMargins(0, 0, 0, 0);
            hbox->setSpacing(2);

            for (int axis = 0; axis < axisCount; ++axis)
            {
                auto* axisLabel = new QLabel{kAxisLabels[axis]};
                axisLabel->setFixedWidth(12);
                axisLabel->setAlignment(Qt::AlignCenter);
                axisLabel->setStyleSheet(
                    QString{"color: %1; font-size: 10px; font-weight: bold;"}.arg(kAxisColors[axis]));
                hbox->addWidget(axisLabel);

                auto* spin = MakeDoubleSpinBox(configs[axis].m_value, configs[axis].m_step);
                spin->setMinimum(configs[axis].m_min);
                spin->setMaximum(configs[axis].m_max);
                if (configs[axis].m_suffix != nullptr)
                {
                    spin->setSuffix(QString::fromUtf8(configs[axis].m_suffix));
                }
                hbox->addWidget(spin, 1);

                QObject::connect(spin, &QDoubleSpinBox::editingFinished, container,
                                 [spin, axis, cb = std::forward<Callback>(onChanged)]() {
                                     cb(axis, static_cast<float>(spin->value()));
                                 });
            }

            return container;
        }
    } // namespace

    InspectorPanel::InspectorPanel(QWidget* parent)
        : QScrollArea{parent}
    {
        setWidgetResizable(true);
        setStyleSheet("QScrollArea { background: #0d0d0d; border: none; }"
                      "QWidget#inspectorContent { background: #0d0d0d; }"
                      "QGroupBox {"
                      "  background: #141414; border: 1px solid #2a2a2a; border-radius: 4px;"
                      "  margin-top: 6px; padding: 8px 6px 6px 6px; font-size: 11px;"
                      "  font-weight: bold; color: #f0a500;"
                      "}"
                      "QGroupBox::title {"
                      "  subcontrol-origin: margin; left: 8px; padding: 0 4px;"
                      "}"
                      "QGroupBox::indicator { width: 12px; height: 12px; }"
                      "QGroupBox::indicator:checked { background: #f0a500; border-radius: 2px; }"
                      "QGroupBox::indicator:unchecked { background: #333; border-radius: 2px; }"
                      "QLabel { color: #999; font-size: 11px; }"
                      "QLabel#inspectorHeader { color: #e8e8e8; font-size: 12px; font-weight: bold; }"
                      "QDoubleSpinBox, QSpinBox {"
                      "  background: #1a1a1a; color: #e8e8e8; border: 1px solid #2a2a2a;"
                      "  border-radius: 3px; padding: 2px 4px; font-size: 11px;"
                      "  min-width: 48px; max-height: 20px;"
                      "}"
                      "QDoubleSpinBox:focus, QSpinBox:focus { border-color: #f0a500; }"
                      "QDoubleSpinBox::up-button, QDoubleSpinBox::down-button,"
                      "QSpinBox::up-button, QSpinBox::down-button { width: 0; }"
                      "QLineEdit {"
                      "  background: #1a1a1a; color: #e8e8e8; border: 1px solid #2a2a2a;"
                      "  border-radius: 3px; padding: 2px 4px; font-size: 11px; max-height: 20px;"
                      "}"
                      "QLineEdit:focus { border-color: #f0a500; }"
                      "QCheckBox { color: #e8e8e8; font-size: 11px; }"
                      "QCheckBox::indicator { width: 14px; height: 14px; border-radius: 2px; }"
                      "QCheckBox::indicator:unchecked { background: #333; border: 1px solid #2a2a2a; }"
                      "QCheckBox::indicator:checked { background: #f0a500; border: none; }"
                      "QComboBox {"
                      "  background: #1a1a1a; color: #e8e8e8; border: 1px solid #2a2a2a;"
                      "  border-radius: 3px; padding: 2px 4px; font-size: 11px; max-height: 20px;"
                      "}"
                      "QPushButton {"
                      "  background: #1a1a1a; color: #e8e8e8; border: 1px solid #2a2a2a;"
                      "  border-radius: 3px; padding: 2px 8px; font-size: 11px; max-height: 20px;"
                      "}"
                      "QPushButton:hover { border-color: #f0a500; }");
    }

    void InspectorPanel::Refresh(queen::World& world, EditorSelection& selection,
                                 const queen::ComponentRegistry<256>& registry, UndoStack& undo)
    {
        switch (selection.Kind())
        {
        case SelectionKind::ENTITY:
            RefreshEntity(world, selection, registry, undo);
            return;
        case SelectionKind::ASSET:
            RefreshAsset(selection.AssetPath(), selection.SelectedAssetType());
            return;
        case SelectionKind::NONE:
            ShowEmpty();
            return;
        }
    }

    void InspectorPanel::ShowEmpty()
    {
        auto* content = new QWidget;
        content->setObjectName("inspectorContent");
        auto* rootLayout = new QVBoxLayout{content};
        rootLayout->setContentsMargins(4, 2, 4, 4);
        rootLayout->setSpacing(1);

        auto* label = new QLabel{"Nothing selected"};
        label->setStyleSheet("color: #555; font-size: 11px; padding: 12px;");
        rootLayout->addWidget(label);
        rootLayout->addStretch();
        setWidget(content);
    }

    static constexpr int ASSET_PREVIEW_SIZE = 256;

    void InspectorPanel::RefreshAsset(const std::filesystem::path& path, AssetType type)
    {
        auto* content = new QWidget;
        content->setObjectName("inspectorContent");
        auto* rootLayout = new QVBoxLayout{content};
        rootLayout->setContentsMargins(4, 2, 4, 4);
        rootLayout->setSpacing(4);

        auto* header = new QLabel{"Asset"};
        header->setObjectName("inspectorHeader");
        header->setContentsMargins(2, 0, 0, 0);
        rootLayout->addWidget(header);

        auto addRow = [&](const char* label, const QString& value) {
            auto* row = new QWidget{content};
            auto* hbox = new QHBoxLayout{row};
            hbox->setContentsMargins(4, 0, 4, 0);
            hbox->setSpacing(8);
            auto* lbl = new QLabel{label};
            lbl->setStyleSheet("color: #888; font-size: 12px;");
            lbl->setFixedWidth(70);
            hbox->addWidget(lbl);
            auto* val = new QLabel{value};
            val->setStyleSheet("color: #e8e8e8; font-size: 12px;");
            val->setTextInteractionFlags(Qt::TextSelectableByMouse);
            val->setWordWrap(true);
            hbox->addWidget(val, 1);
            rootLayout->addWidget(row);
        };

        addRow("Name", QString::fromStdString(path.stem().string()));
        addRow("Type", QString::fromStdString(path.extension().string()));
        addRow("Path", QString::fromStdString(path.generic_string()));

        std::error_code ec;
        if (std::filesystem::exists(path, ec))
        {
            auto fileSize = std::filesystem::file_size(path, ec);
            if (!ec)
            {
                QString sizeStr;
                if (fileSize < 1024)
                    sizeStr = QString{"%1 B"}.arg(fileSize);
                else if (fileSize < 1024 * 1024)
                    sizeStr = QString{"%1 KB"}.arg(fileSize / 1024);
                else
                    sizeStr = QString{"%1 MB"}.arg(fileSize / (1024 * 1024));
                addRow("Size", sizeStr);
            }
        }

        if (type == AssetType::TEXTURE)
            RefreshTexture(path, rootLayout, addRow);
        else if (type == AssetType::MESH)
            RefreshMesh(path, addRow);
        else if (type == AssetType::MATERIAL)
            RefreshMaterial(path, addRow);

        rootLayout->addStretch();
        setWidget(content);
    }

    void InspectorPanel::RefreshTexture(const std::filesystem::path& path, QVBoxLayout* layout,
                                        const std::function<void(const char*, const QString&)>& addRow)
    {
        QImage img;
        if (!img.load(QString::fromStdString(path.string())))
            return;

        addRow("Dims", QString{"%1 x %2"}.arg(img.width()).arg(img.height()));
        addRow("Format", img.isGrayscale() ? "Grayscale" : (img.hasAlphaChannel() ? "RGBA" : "RGB"));

        auto* preview = new QLabel;
        QPixmap pix = QPixmap::fromImage(
            img.scaled(ASSET_PREVIEW_SIZE, ASSET_PREVIEW_SIZE, Qt::KeepAspectRatio, Qt::SmoothTransformation));
        preview->setPixmap(pix);
        preview->setAlignment(Qt::AlignCenter);
        preview->setStyleSheet("margin: 8px; border: 1px solid #2a2a2a; border-radius: 4px;");
        layout->addWidget(preview);
    }

    void InspectorPanel::RefreshMesh(const std::filesystem::path& path,
                                     const std::function<void(const char*, const QString&)>& addRow)
    {
        nectar::NmshHeader header{};
        FILE* f{nullptr};
#ifdef _WIN32
        fopen_s(&f, path.string().c_str(), "rb");
#else
        f = fopen(path.string().c_str(), "rb");
#endif
        if (!f)
            return;

        if (std::fread(&header, sizeof(header), 1, f) == 1 && header.m_magic == nectar::kNmshMagic)
        {
            addRow("Vertices", QString::number(header.m_vertexCount));
            addRow("Indices", QString::number(header.m_indexCount));
            addRow("Submeshes", QString::number(header.m_submeshCount));
        }
        std::fclose(f);
    }

    void InspectorPanel::RefreshMaterial(const std::filesystem::path& path,
                                         const std::function<void(const char*, const QString&)>& addRow)
    {
        QFile matFile{QString::fromStdString(path.string())};
        if (!matFile.open(QIODevice::ReadOnly | QIODevice::Text))
            return;

        QString currentSection;
        while (!matFile.atEnd())
        {
            QString line = matFile.readLine().trimmed();
            if (line.isEmpty() || line.startsWith('#'))
                continue;

            if (line.startsWith('[') && line.endsWith(']'))
            {
                currentSection = line.mid(1, line.size() - 2);
                continue;
            }

            int eq = line.indexOf('=');
            if (eq < 0)
                continue;

            QString key = line.left(eq).trimmed();
            QString value = line.mid(eq + 1).trimmed();
            if (value.startsWith('"') && value.endsWith('"'))
                value = value.mid(1, value.size() - 2);

            if (currentSection == "material")
            {
                if (key == "name" && !value.isEmpty())
                    addRow("Mat Name", value);
                else if (key == "shader")
                    addRow("Shader", value);
                else if (key == "blend")
                    addRow("Blend", value);
                else if (key == "double_sided")
                    addRow("2-Sided", value);
                else if (key == "alpha_cutoff")
                    addRow("Alpha Cut", value);
            }
            else if (currentSection == "factors")
            {
                if (key == "base_color")
                    addRow("Base Color", value);
                else if (key == "metallic")
                    addRow("Metallic", value);
                else if (key == "roughness")
                    addRow("Roughness", value);
            }
            else if (currentSection == "textures")
            {
                addRow(key.toUtf8().constData(), value);
            }
        }
    }

    void InspectorPanel::RefreshEntity(queen::World& world, EditorSelection& selection,
                                       const queen::ComponentRegistry<256>& registry, UndoStack& undo)
    {
        auto* content = new QWidget;
        content->setObjectName("inspectorContent");
        auto* rootLayout = new QVBoxLayout{content};
        rootLayout->setContentsMargins(4, 2, 4, 4);
        rootLayout->setSpacing(1);

        const queen::Entity entity = selection.Primary();
        if (entity.IsNull() || !world.IsAlive(entity))
        {
            ShowEmpty();
            return;
        }

        auto* header = new QLabel{QString{"Entity %1"}.arg(entity.Index())};
        header->setObjectName("inspectorHeader");
        header->setContentsMargins(2, 0, 0, 0);
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
            {
                BuildFieldWidget(reflection.m_fields[i], comp, ctx, form);
            }

            rootLayout->addWidget(group);
        });

        rootLayout->addStretch(1);
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

                        widget = BuildAxisRow(3, configs,
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

                    static constexpr const char* kDegSuffix = "\xC2\xB0";
                    AxisSpinConfig configs[3];
                    for (int i = 0; i < 3; ++i)
                    {
                        configs[i] = {static_cast<double>(euler[i]), 0.5, -360.0, 360.0, kDegSuffix};
                    }

                    auto snapshot = std::make_shared<SnapshotState>();

                    widget = BuildAxisRow(3, configs,
                                          [this, q, snapshot, entity, typeId, offset, &undo, quatToEuler, eulerToQuat](int axis, float newDeg) {
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

                QObject::connect(lineEdit, &QLineEdit::editingFinished, this,
                                 [this, lineEdit, value, snapshot, entity, typeId, offset, &undo]() {
                                     QByteArray utf8 = lineEdit->text().toUtf8();
                                     wax::FixedString newVal{utf8.constData(), static_cast<size_t>(utf8.size())};
                                     if (newVal != *value)
                                     {
                                         Snapshot(*snapshot, entity, typeId, offset,
                                                  static_cast<uint16_t>(sizeof(wax::FixedString)), value);
                                         *value = newVal;
                                         CommitIfChanged(*snapshot, undo, value);
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
        form->addRow(QString::fromUtf8(label), widget);
    }
} // namespace forge
