#include <forge/asset_browser.h>

#include <QHeaderView>
#include <QVBoxLayout>

#include <algorithm>
#include <filesystem>
#include <string>
#include <vector>

namespace forge
{
    static const char* IconForExtension(const std::string& ext)
    {
        if (ext == ".gltf" || ext == ".glb" || ext == ".obj")
            return "[3D]";
        if (ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".tga" || ext == ".bmp")
            return "[Tex]";
        if (ext == ".hlsl" || ext == ".glsl" || ext == ".vert" || ext == ".frag")
            return "[Sh]";
        if (ext == ".hscene")
            return "[Scene]";
        return nullptr;
    }

    AssetBrowserPanel::AssetBrowserPanel(QWidget* parent)
        : QWidget{parent}
    {
        auto* layout = new QVBoxLayout{this};
        layout->setContentsMargins(0, 0, 0, 0);

        m_tree = new QTreeWidget{this};
        m_tree->setHeaderHidden(true);
        m_tree->setEditTriggers(QAbstractItemView::NoEditTriggers);
        m_tree->setDragDropMode(QAbstractItemView::NoDragDrop);
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
            return;

        PopulateDirectory(nullptr, m_root);
    }

    void AssetBrowserPanel::PopulateDirectory(QTreeWidgetItem* parent, const std::filesystem::path& dir)
    {
        std::vector<std::filesystem::directory_entry> dirs;
        std::vector<std::filesystem::directory_entry> files;

        std::error_code ec;
        for (const auto& entry : std::filesystem::directory_iterator(dir, ec))
        {
            if (entry.is_directory())
                dirs.push_back(entry);
            else
                files.push_back(entry);
        }

        std::sort(dirs.begin(), dirs.end());
        std::sort(files.begin(), files.end());

        for (const auto& d : dirs)
        {
            auto* item = parent ? new QTreeWidgetItem{parent} : new QTreeWidgetItem{m_tree};
            item->setText(0, QString::fromStdString(d.path().filename().string()));
            PopulateDirectory(item, d.path());
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
            item->setFlags(item->flags() & ~Qt::ItemIsDropEnabled);
        }
    }
} // namespace forge
