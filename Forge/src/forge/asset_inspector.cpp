#include <forge/asset_inspector.h>

#include <nectar/mesh/mesh_data.h>

#include <forge/asset_browser.h>
#include <forge/editor_undo.h>
#include <forge/material_inspector.h>

#include <QHBoxLayout>
#include <QLabel>
#include <QPainter>
#include <QVBoxLayout>

#include <cstdio>
#include <filesystem>

namespace forge
{
    static constexpr int ASSET_PREVIEW_SIZE = 256;

    AssetInspector::AssetInspector(const std::filesystem::path& path, AssetType type,
                                    EditorUndoManager& editorUndo, QWidget* parent)
        : QWidget{parent}
    {
        auto* rootLayout = new QVBoxLayout{this};
        rootLayout->setContentsMargins(0, 0, 0, 0);
        rootLayout->setSpacing(4);

        auto* header = new QLabel{"Asset"};
        header->setObjectName("inspectorHeader");
        header->setContentsMargins(2, 0, 0, 0);
        rootLayout->addWidget(header);

        auto addRow = [rootLayout](const char* label, const QString& value) {
            auto* row = new QWidget;
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
            BuildTexture(path, rootLayout, addRow);
        else if (type == AssetType::MESH)
            BuildMesh(path, addRow);
        else if (type == AssetType::MATERIAL)
        {
            auto* matInspector = new MaterialInspector{path, editorUndo, this};
            connect(matInspector, &MaterialInspector::browseToAsset, this, &AssetInspector::browseToAsset);
            rootLayout->addWidget(matInspector);
        }

        rootLayout->addStretch();
    }

    void AssetInspector::BuildTexture(const std::filesystem::path& path, QVBoxLayout* layout,
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

    void AssetInspector::BuildMesh(const std::filesystem::path& path,
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
} // namespace forge
