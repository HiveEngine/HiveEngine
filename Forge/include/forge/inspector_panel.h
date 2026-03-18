#pragma once

#include <queen/core/entity.h>
#include <queen/core/type_id.h>

#include <QFormLayout>
#include <QScrollArea>

#include <cstdint>

namespace queen
{
    class World;
    struct FieldInfo;
    template <size_t> class ComponentRegistry;
} // namespace queen

namespace forge
{
    class EditorSelection;
    class UndoStack;

    class InspectorPanel : public QScrollArea
    {
        Q_OBJECT

    public:
        explicit InspectorPanel(QWidget* parent = nullptr);

        void Refresh(queen::World& world, EditorSelection& selection, const queen::ComponentRegistry<256>& registry,
                     UndoStack& undo);

    signals:
        void sceneModified();

    private:
        struct FieldContext
        {
            queen::World* m_world;
            queen::Entity m_entity;
            queen::TypeId m_typeId;
            uint16_t m_baseOffset;
            UndoStack* m_undo;
        };

        void BuildFieldWidget(const queen::FieldInfo& field, void* data, const FieldContext& ctx, QFormLayout* form);
    };
} // namespace forge
