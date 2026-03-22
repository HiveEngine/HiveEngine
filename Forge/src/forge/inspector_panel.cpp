#include <forge/asset_browser.h>
#include <forge/asset_inspector.h>
#include <forge/entity_inspector.h>
#include <forge/inspector_panel.h>
#include <forge/selection.h>

#include <QLabel>
#include <QVBoxLayout>

namespace forge
{
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
                                 const queen::ComponentRegistry<256>& registry, UndoStack& undo,
                                 EditorUndoManager& editorUndo)
    {
        auto* content = new QWidget;
        content->setObjectName("inspectorContent");

        switch (selection.Kind())
        {
            case SelectionKind::ENTITY: {
                auto* inspector = new EntityInspector{world, selection, registry, undo, editorUndo, content};
                auto* layout = new QVBoxLayout{content};
                layout->setContentsMargins(0, 0, 0, 0);
                layout->addWidget(inspector);
                connect(inspector, &EntityInspector::sceneModified, this, &InspectorPanel::sceneModified);
                connect(inspector, &EntityInspector::entityLabelChanged, this, &InspectorPanel::entityLabelChanged);
                break;
            }
            case SelectionKind::ASSET: {
                auto* inspector =
                    new AssetInspector{selection.AssetPath(), selection.SelectedAssetType(), editorUndo, content};
                auto* layout = new QVBoxLayout{content};
                layout->setContentsMargins(0, 0, 0, 0);
                layout->addWidget(inspector);
                connect(inspector, &AssetInspector::browseToAsset, this, &InspectorPanel::browseToAsset);
                break;
            }
            case SelectionKind::NONE: {
                auto* layout = new QVBoxLayout{content};
                layout->setContentsMargins(4, 2, 4, 4);
                auto* label = new QLabel{"Nothing selected"};
                label->setStyleSheet("color: #555; font-size: 11px; padding: 12px;");
                layout->addWidget(label);
                layout->addStretch();
                break;
            }
        }

        setWidget(content);
    }
} // namespace forge
