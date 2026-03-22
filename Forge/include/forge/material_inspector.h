#pragma once

#include <QWidget>

#include <filesystem>
#include <functional>
#include <map>
#include <memory>

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

    static constexpr int MATERIAL_THUMB_SIZE = 48;
    [[nodiscard]] bool WriteMatFile(const std::filesystem::path& path, const MatFields& mat);

    class MaterialInspector : public QWidget
    {
        Q_OBJECT

    public:
        MaterialInspector(const std::filesystem::path& path, EditorUndoManager& editorUndo,
                          QWidget* parent = nullptr);

    protected:
        bool eventFilter(QObject* obj, QEvent* event) override;

    signals:
        void materialModified();
        void browseToAsset(const QString& path);

    private:
        std::shared_ptr<MatFields> m_matState;
        std::shared_ptr<std::filesystem::path> m_matPath;
        std::function<void()> m_saveMat;
    };
} // namespace forge
