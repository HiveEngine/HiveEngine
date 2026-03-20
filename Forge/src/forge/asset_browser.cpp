#include <forge/asset_browser.h>

#include <QFileDialog>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QToolButton>
#include <QVBoxLayout>

#include <forge/forge_module.h>

#include <wax/containers/vector.h>

#include <algorithm>
#include <filesystem>
#include <string>

namespace forge
{
    static const char* IconForExtension(const std::string& ext)
    {
        if (ext == ".nmsh")
            return "[3D]";
        if (ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".tga" || ext == ".bmp" || ext == ".hdr")
            return "[Tex]";
        if (ext == ".hlsl" || ext == ".glsl" || ext == ".vert" || ext == ".frag")
            return "[Sh]";
        if (ext == ".hscene")
            return "[Scene]";
        if (ext == ".hmat")
            return "[Mat]";
        if (ext == ".wav" || ext == ".ogg" || ext == ".flac" || ext == ".mp3")
            return "[Snd]";
        return nullptr;
    }

    static bool IsSupportedExtension(const std::string& ext)
    {
        return IconForExtension(ext) != nullptr;
    }

    AssetBrowserPanel::AssetBrowserPanel(QWidget* parent)
        : QWidget{parent}
    {
        auto* layout = new QVBoxLayout{this};
        layout->setContentsMargins(0, 0, 0, 0);
        layout->setSpacing(0);

        auto* toolbar = new QWidget{this};
        toolbar->setFixedHeight(28);
        toolbar->setStyleSheet("QWidget { background: #141414; border-bottom: 1px solid #2a2a2a; }");

        auto* toolLayout = new QHBoxLayout{toolbar};
        toolLayout->setContentsMargins(6, 0, 6, 0);
        toolLayout->setSpacing(4);

        auto* importBtn = new QToolButton{toolbar};
        importBtn->setText("Import");
        importBtn->setCursor(Qt::PointingHandCursor);
        importBtn->setStyleSheet("QToolButton {"
                                 "  background: #1a3a1a; color: #6abf6a; border: none; border-radius: 3px;"
                                 "  padding: 2px 12px; font-size: 11px; font-weight: bold;"
                                 "  font-family: 'Segoe UI', sans-serif;"
                                 "}"
                                 "QToolButton:hover { background: #2a4a2a; color: #8adf8a; }");
        connect(importBtn, &QToolButton::clicked, this, &AssetBrowserPanel::OnImportClicked);
        toolLayout->addWidget(importBtn);

        toolLayout->addStretch();
        layout->addWidget(toolbar);

        m_tree = new QTreeWidget{this};
        m_tree->setHeaderHidden(true);
        m_tree->setEditTriggers(QAbstractItemView::NoEditTriggers);
        m_tree->setDragDropMode(QAbstractItemView::DragOnly);
        layout->addWidget(m_tree);
    }

    void AssetBrowserPanel::SetAssetsRoot(const char* path)
    {
        m_root = std::filesystem::path{path};
        Refresh();
    }

    void AssetBrowserPanel::Refresh()
    {
        m_tree->clear();

        if (m_root.empty() || !std::filesystem::exists(m_root))
        {
            auto* item = new QTreeWidgetItem{m_tree};
            item->setText(0, "No assets directory");
            item->setForeground(0, QColor{0x66, 0x66, 0x66});
            item->setFlags(item->flags() & ~Qt::ItemIsSelectable);
            return;
        }

        PopulateDirectory(nullptr, m_root);
    }

    std::filesystem::path AssetBrowserPanel::GetSelectedDirectory() const
    {
        auto* item = m_tree->currentItem();
        if (item == nullptr)
            return m_root;

        auto path = item->data(0, Qt::UserRole).toString();
        if (!path.isEmpty())
        {
            std::filesystem::path fsPath{path.toStdString()};
            if (std::filesystem::is_directory(fsPath))
                return fsPath;
            return fsPath.parent_path();
        }

        return m_root;
    }

    void AssetBrowserPanel::OnImportClicked()
    {
        if (m_root.empty())
            return;

        auto destination = GetSelectedDirectory();

        QStringList files =
            QFileDialog::getOpenFileNames(this, "Import Assets", QDir::homePath(),
                                          "3D Models (*.gltf *.glb *.obj);;"
                                          "Textures (*.png *.jpg *.jpeg *.tga *.bmp *.hdr);;"
                                          "Shaders (*.hlsl *.glsl);;"
                                          "Audio (*.wav *.ogg *.flac *.mp3);;"
                                          "All Files (*)");

        if (files.isEmpty())
            return;

        std::error_code ec;
        for (const auto& file : files)
        {
            std::filesystem::path srcPath{file.toStdString()};
            std::filesystem::path dstPath = destination / srcPath.filename();

            if (srcPath.extension() == ".gltf" || srcPath.extension() == ".glb")
            {
                emit gltfImportRequested(file);
            }
            else
            {
                std::filesystem::copy_file(srcPath, dstPath, std::filesystem::copy_options::skip_existing, ec);
                emit assetImported(QString::fromStdString(dstPath.generic_string()));
            }
        }

        Refresh();
    }

    void AssetBrowserPanel::PopulateDirectory(QTreeWidgetItem* parent, const std::filesystem::path& dir)
    {
        wax::Vector<std::filesystem::directory_entry> dirs{forge::GetAllocator()};
        wax::Vector<std::filesystem::directory_entry> files{forge::GetAllocator()};

        std::error_code ec;
        for (const auto& entry : std::filesystem::directory_iterator(dir, ec))
        {
            auto name = entry.path().filename().string();
            if (!name.empty() && name[0] == '.')
                continue;

            if (entry.is_directory())
                dirs.PushBack(entry);
            else if (IsSupportedExtension(entry.path().extension().string()))
                files.PushBack(entry);
        }

        std::sort(dirs.begin(), dirs.end());
        std::sort(files.begin(), files.end());

        for (const auto& d : dirs)
        {
            auto* item = parent ? new QTreeWidgetItem{parent} : new QTreeWidgetItem{m_tree};
            item->setText(0, QString::fromStdString(d.path().filename().string()));
            item->setData(0, Qt::UserRole, QString::fromStdString(d.path().generic_string()));
            PopulateDirectory(item, d.path());
            if (item->childCount() == 0)
                delete item;
        }

        for (const auto& f : files)
        {
            std::string name = f.path().filename().string();
            std::string ext = f.path().extension().string();
            const char* icon = IconForExtension(ext);

            QString label =
                icon ? QString{"%1 %2"}.arg(icon, QString::fromStdString(name)) : QString::fromStdString(name);

            auto* item = parent ? new QTreeWidgetItem{parent} : new QTreeWidgetItem{m_tree};
            item->setText(0, label);
            item->setToolTip(0, QString::fromStdString(f.path().string()));
            item->setData(0, Qt::UserRole, QString::fromStdString(f.path().generic_string()));
            item->setFlags((item->flags() | Qt::ItemIsDragEnabled) & ~Qt::ItemIsDropEnabled);
        }
    }
} // namespace forge
