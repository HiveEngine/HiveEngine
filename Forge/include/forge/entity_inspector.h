#pragma once

#include <wax/containers/vector.h>

#include <queen/core/entity.h>
#include <queen/core/type_id.h>

#include <QFormLayout>
#include <QWidget>

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
    class EditorUndoManager;
    class UndoStack;

    class EntityInspector : public QWidget
    {
        Q_OBJECT

    public:
        EntityInspector(queen::World& world, EditorSelection& selection, const queen::ComponentRegistry<256>& registry,
                        UndoStack& undo, EditorUndoManager& editorUndo, QWidget* parent = nullptr);

    signals:
        void sceneModified();
        void entityLabelChanged(queen::Entity entity);

    private:
        struct FieldContext
        {
            queen::World* m_world;
            queen::Entity m_entity;
            queen::TypeId m_typeId;
            uint16_t m_baseOffset;
            UndoStack* m_undo;
        };

        struct MultiFieldContext
        {
            queen::World* m_world;
            const wax::Vector<queen::Entity>* m_entities;
            queen::TypeId m_typeId;
            uint16_t m_baseOffset;
            EditorUndoManager* m_editorUndo;
        };

        void BuildSingleEntity(queen::World& world, queen::Entity entity, const queen::ComponentRegistry<256>& registry,
                               UndoStack& undo);
        void BuildMultiEntity(queen::World& world, const wax::Vector<queen::Entity>& entities,
                              const queen::ComponentRegistry<256>& registry, EditorUndoManager& editorUndo);

        void BuildFieldWidget(const queen::FieldInfo& field, void* data, const FieldContext& ctx, QFormLayout* form);
        void BuildMultiFieldWidget(const queen::FieldInfo& field, void* primaryData, const MultiFieldContext& ctx,
                                   QFormLayout* form);

        QVBoxLayout* m_rootLayout{};
    };
} // namespace forge
