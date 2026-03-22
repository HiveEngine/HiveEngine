#pragma once

#include <QVBoxLayout>
#include <QWidget>

#include <cstdint>
#include <filesystem>
#include <functional>

namespace forge
{
    enum class AssetType : uint8_t;
    class EditorUndoManager;

    class AssetInspector : public QWidget
    {
        Q_OBJECT

    public:
        AssetInspector(const std::filesystem::path& path, AssetType type, EditorUndoManager& editorUndo,
                       QWidget* parent = nullptr);

    signals:
        void browseToAsset(const QString& path);

    private:
        void BuildTexture(const std::filesystem::path& path, QVBoxLayout* layout,
                          const std::function<void(const char*, const QString&)>& addRow);
        void BuildMesh(const std::filesystem::path& path,
                       const std::function<void(const char*, const QString&)>& addRow);
    };
} // namespace forge
