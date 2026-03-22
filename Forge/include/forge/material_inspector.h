#pragma once

#include <QWidget>

#include <filesystem>
#include <map>

namespace forge
{
    class EditorUndoManager;

    struct MatFields
    {
        QString shader{"standard_pbr"};
        QString blend{"alpha_test"};
        bool doubleSided{false};
        float alphaCutoff{0.5f};
        float baseColor[4]{1.f, 1.f, 1.f, 1.f};
        float metallic{1.f};
        float roughness{1.f};
        std::map<QString, QString> textures;
        QString name;
    };

    [[nodiscard]] MatFields ParseMatFile(const std::filesystem::path& path);
    void WriteMatFile(const std::filesystem::path& path, const MatFields& mat);

    class MaterialInspector : public QWidget
    {
        Q_OBJECT

    public:
        MaterialInspector(const std::filesystem::path& path, EditorUndoManager& editorUndo,
                          QWidget* parent = nullptr);

    signals:
        void materialModified();
    };
} // namespace forge
