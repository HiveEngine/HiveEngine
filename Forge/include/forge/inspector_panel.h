#pragma once

#include <queen/core/entity.h>
#include <queen/core/type_id.h>

#include <QFormLayout>
#include <QScrollArea>

#include <cstdint>
#include <filesystem>
#include <functional>

namespace queen
{
    class World;
    struct FieldInfo;
    template <size_t> class ComponentRegistry;
} // namespace queen

namespace forge
{
    enum class AssetType : uint8_t;
    class EditorSelection;
    class EditorUndoManager;
    class UndoStack;

    class InspectorPanel : public QScrollArea
    {
        Q_OBJECT

    public:
        explicit InspectorPanel(QWidget* parent = nullptr);

        void Refresh(queen::World& world, EditorSelection& selection, const queen::ComponentRegistry<256>& registry,
                     UndoStack& undo, EditorUndoManager& editorUndo);

    signals:
        void sceneModified();

    private:
        void RefreshEntity(queen::World& world, EditorSelection& selection,
                           const queen::ComponentRegistry<256>& registry, UndoStack& undo,
                           EditorUndoManager& editorUndo);
        void RefreshAsset(const std::filesystem::path& path, AssetType type, EditorUndoManager& editorUndo);
        void RefreshTexture(const std::filesystem::path& path, QVBoxLayout* layout,
                            const std::function<void(const char*, const QString&)>& addRow);
        void RefreshMesh(const std::filesystem::path& path,
                         const std::function<void(const char*, const QString&)>& addRow);
        void RefreshMaterial(const std::filesystem::path& path, QVBoxLayout* layout,
                             EditorUndoManager& editorUndo);
        void ShowEmpty();

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
