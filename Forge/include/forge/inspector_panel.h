#pragma once

#include <queen/core/entity.h>

#include <QScrollArea>

#include <cstddef>

namespace queen
{
    class World;
    template <size_t> class ComponentRegistry;
} // namespace queen

namespace forge
{
    class EditorSelection;
    class EditorUndoManager;

    class InspectorPanel : public QScrollArea
    {
        Q_OBJECT

    public:
        explicit InspectorPanel(QWidget* parent = nullptr);

        void Refresh(queen::World& world, EditorSelection& selection, const queen::ComponentRegistry<256>& registry,
                     EditorUndoManager& editorUndo);

    signals:
        void sceneModified();
        void entityLabelChanged(queen::Entity entity);
        void browseToAsset(const QString& path);
    };
} // namespace forge
