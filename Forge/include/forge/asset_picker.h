#pragma once

#include <QDialog>
#include <QLineEdit>
#include <QListWidget>

#include <filesystem>

namespace forge
{
    enum class AssetType : uint8_t;

    class AssetPickerPopup : public QDialog
    {
        Q_OBJECT

    public:
        AssetPickerPopup(const std::filesystem::path& assetsRoot, AssetType filterType,
                         QWidget* parent = nullptr);

        [[nodiscard]] std::filesystem::path SelectedPath() const { return m_selectedPath; }

    private:
        void PopulateGrid();
        void ApplyFilter();
        void LoadThumbnailsBatched(int startIndex);

        QLineEdit* m_searchField{};
        QListWidget* m_grid{};
        std::filesystem::path m_assetsRoot;
        std::filesystem::path m_selectedPath;
        AssetType m_filterType;
    };
} // namespace forge
