#include <forge/asset_picker.h>

#include <forge/asset_browser.h>

#include <QHBoxLayout>
#include <QLabel>
#include <QPainter>
#include <QPushButton>
#include <QDir>
#include <QStyledItemDelegate>
#include <QTimer>
#include <QVBoxLayout>

#include <algorithm>
#include <filesystem>

namespace forge
{
    static constexpr int PICKER_THUMB_SIZE = 64;

    class PickerItemDelegate : public QStyledItemDelegate
    {
    public:
        using QStyledItemDelegate::QStyledItemDelegate;

        void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override
        {
            painter->save();

            if (option.state & QStyle::State_Selected)
            {
                painter->fillRect(option.rect, QColor{0x3d, 0x2e, 0x0a});
                painter->setPen(QPen{QColor{0xf0, 0xa5, 0x00}, 2});
                painter->drawRect(option.rect.adjusted(1, 1, -1, -1));
            }
            else if (option.state & QStyle::State_MouseOver)
            {
                painter->fillRect(option.rect, QColor{0x1e, 0x1e, 0x1e});
            }

            QRect iconRect = option.rect;
            iconRect.setHeight(iconRect.width());
            iconRect.adjust(4, 4, -4, 0);

            QPixmap thumb = index.data(Qt::DecorationRole).value<QPixmap>();
            if (!thumb.isNull())
            {
                QPixmap scaled = thumb.scaled(iconRect.size(), Qt::KeepAspectRatio, Qt::SmoothTransformation);
                int dx = (iconRect.width() - scaled.width()) / 2;
                int dy = (iconRect.height() - scaled.height()) / 2;
                painter->drawPixmap(iconRect.x() + dx, iconRect.y() + dy, scaled);
            }
            else
            {
                painter->setPen(Qt::NoPen);
                painter->setBrush(QColor{0x22, 0x22, 0x22});
                painter->drawRoundedRect(iconRect, 4, 4);

                auto type = static_cast<AssetType>(index.data(Qt::UserRole + 1).toInt());
                const char* label = "?";
                QColor color{0x90, 0x90, 0x90};
                switch (type)
                {
                case AssetType::TEXTURE:
                    label = "TEX";
                    color = QColor{0x81, 0xc7, 0x84};
                    break;
                case AssetType::MESH:
                    label = "MESH";
                    color = QColor{0x4f, 0xc3, 0xf7};
                    break;
                case AssetType::MATERIAL:
                    label = "MAT";
                    color = QColor{0xef, 0x53, 0x50};
                    break;
                case AssetType::SCENE:
                    label = "SCENE";
                    color = QColor{0xff, 0xb7, 0x4d};
                    break;
                case AssetType::SHADER:
                    label = "SHADER";
                    color = QColor{0xce, 0x93, 0xd8};
                    break;
                case AssetType::AUDIO:
                    label = "AUDIO";
                    color = QColor{0xff, 0xf1, 0x76};
                    break;
                default:
                    break;
                }

                QFont badgeFont{"Segoe UI", 8, QFont::Bold};
                painter->setFont(badgeFont);
                painter->setPen(color);
                painter->drawText(iconRect, Qt::AlignCenter, label);
            }

            QRect textRect = option.rect;
            textRect.setTop(iconRect.bottom() + 4);
            textRect.adjust(2, 0, -2, 0);

            QString name = index.data(Qt::DisplayRole).toString();
            QFont font{"Segoe UI", 8};
            painter->setFont(font);

            QColor textColor =
                (option.state & QStyle::State_Selected) ? QColor{0xf0, 0xa5, 0x00} : QColor{0xcc, 0xcc, 0xcc};
            painter->setPen(textColor);

            QFontMetrics fm{font};
            QString elided = fm.elidedText(name, Qt::ElideMiddle, textRect.width());
            painter->drawText(textRect, Qt::AlignHCenter | Qt::AlignTop, elided);

            painter->restore();
        }

        QSize sizeHint(const QStyleOptionViewItem&, const QModelIndex&) const override
        {
            return QSize{PICKER_THUMB_SIZE + 12, PICKER_THUMB_SIZE + 28};
        }
    };

    AssetPickerPopup::AssetPickerPopup(const std::filesystem::path& assetsRoot, AssetType filterType,
                                       QWidget* parent)
        : QDialog{parent}
        , m_assetsRoot{assetsRoot}
        , m_filterType{filterType}
    {
        setWindowTitle("Select Asset");
        setMinimumSize(480, 400);
        resize(520, 500);
        setStyleSheet("QDialog { background: #0d0d0d; }");

        auto* layout = new QVBoxLayout{this};
        layout->setContentsMargins(0, 0, 0, 0);
        layout->setSpacing(0);

        auto* headerBar = new QWidget{this};
        headerBar->setFixedHeight(44);
        headerBar->setStyleSheet("QWidget { background: #141414; border-bottom: 1px solid #2a2a2a; }");

        auto* headerLayout = new QHBoxLayout{headerBar};
        headerLayout->setContentsMargins(12, 0, 12, 0);
        headerLayout->setSpacing(8);

        auto* titleIcon = new QLabel{"\xe2\xac\xa1"};
        titleIcon->setStyleSheet("color: #f0a500; font-size: 18px;");
        headerLayout->addWidget(titleIcon);

        auto* title = new QLabel{"Select Asset"};
        title->setStyleSheet("color: #e8e8e8; font-size: 13px; font-weight: bold;"
                             " font-family: 'Segoe UI', sans-serif;");
        headerLayout->addWidget(title);
        headerLayout->addStretch();

        m_searchField = new QLineEdit{headerBar};
        m_searchField->setPlaceholderText("Search...");
        m_searchField->setFixedWidth(200);
        m_searchField->setFixedHeight(26);
        m_searchField->setStyleSheet(
            "QLineEdit {"
            "  background: #1a1a1a; color: #ccc; border: 1px solid #333; border-radius: 13px;"
            "  padding: 0 12px; font-size: 12px; font-family: 'Segoe UI', sans-serif;"
            "}"
            "QLineEdit:focus { border-color: #f0a500; }");
        connect(m_searchField, &QLineEdit::textChanged, this, &AssetPickerPopup::ApplyFilter);
        headerLayout->addWidget(m_searchField);

        layout->addWidget(headerBar);

        m_grid = new QListWidget{this};
        m_grid->setViewMode(QListView::IconMode);
        m_grid->setResizeMode(QListView::Adjust);
        m_grid->setMovement(QListView::Static);
        m_grid->setIconSize(QSize{PICKER_THUMB_SIZE, PICKER_THUMB_SIZE});
        m_grid->setGridSize(QSize{PICKER_THUMB_SIZE + 12, PICKER_THUMB_SIZE + 28});
        m_grid->setSpacing(6);
        m_grid->setUniformItemSizes(true);
        m_grid->setSelectionMode(QAbstractItemView::SingleSelection);
        m_grid->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
        m_grid->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        m_grid->setStyleSheet(
            "QListWidget {"
            "  background: #111; border: none; outline: none;"
            "  padding: 8px;"
            "}"
            "QListWidget::item { border-radius: 4px; }"
            "QListWidget::item:hover { background: #1e1e1e; }"
            "QListWidget::item:selected { background: #3d2e0a; }");

        m_grid->setItemDelegate(new PickerItemDelegate{m_grid});

        connect(m_grid, &QListWidget::itemDoubleClicked, this, [this](QListWidgetItem* item) {
            m_selectedPath = std::filesystem::path{item->data(Qt::UserRole).toString().toStdString()};
            accept();
        });

        layout->addWidget(m_grid);

        auto* footerBar = new QWidget{this};
        footerBar->setFixedHeight(44);
        footerBar->setStyleSheet("QWidget { background: #141414; border-top: 1px solid #2a2a2a; }");

        auto* footerLayout = new QHBoxLayout{footerBar};
        footerLayout->setContentsMargins(12, 0, 12, 0);
        footerLayout->setSpacing(8);
        footerLayout->addStretch();

        auto* cancelBtn = new QPushButton{"Cancel"};
        cancelBtn->setCursor(Qt::PointingHandCursor);
        cancelBtn->setStyleSheet(
            "QPushButton {"
            "  background: #222; color: #ccc; border: 1px solid #333; border-radius: 4px;"
            "  padding: 6px 20px; font-size: 12px; font-family: 'Segoe UI', sans-serif;"
            "}"
            "QPushButton:hover { border-color: #555; color: #e8e8e8; }");
        connect(cancelBtn, &QPushButton::clicked, this, &QDialog::reject);
        footerLayout->addWidget(cancelBtn);

        auto* selectBtn = new QPushButton{"Select"};
        selectBtn->setCursor(Qt::PointingHandCursor);
        selectBtn->setStyleSheet(
            "QPushButton {"
            "  background: #f0a500; color: #0d0d0d; border: none; border-radius: 4px;"
            "  padding: 6px 24px; font-size: 12px; font-weight: bold;"
            "  font-family: 'Segoe UI', sans-serif;"
            "}"
            "QPushButton:hover { background: #ffb820; }");
        connect(selectBtn, &QPushButton::clicked, this, [this] {
            auto items = m_grid->selectedItems();
            if (!items.isEmpty())
            {
                m_selectedPath = std::filesystem::path{items.first()->data(Qt::UserRole).toString().toStdString()};
                accept();
            }
        });
        footerLayout->addWidget(selectBtn);

        layout->addWidget(footerBar);

        PopulateGrid();
        m_searchField->setFocus();
    }

    void AssetPickerPopup::PopulateGrid()
    {
        m_grid->clear();

        if (!std::filesystem::exists(m_assetsRoot))
            return;

        struct AssetEntry
        {
            std::filesystem::path path;
            AssetType type;
            QString name;
        };

        std::vector<AssetEntry> entries;
        std::error_code ec;
        for (auto it = std::filesystem::recursive_directory_iterator{m_assetsRoot, ec};
             it != std::filesystem::recursive_directory_iterator{}; ++it)
        {
            if (!it->is_regular_file())
                continue;

            auto name = it->path().filename().string();
            if (!name.empty() && name[0] == '.')
                continue;

            auto ext = it->path().extension().string();
            auto type = ClassifyExtension(ext);
            if (type == AssetType::UNKNOWN)
                continue;

            if (m_filterType != AssetType::UNKNOWN && type != m_filterType)
                continue;

            entries.push_back({it->path(), type, QString::fromStdString(it->path().stem().string())});
        }

        std::sort(entries.begin(), entries.end(), [](const auto& a, const auto& b) {
            return a.name.toLower() < b.name.toLower();
        });

        for (const auto& entry : entries)
        {
            auto* item = new QListWidgetItem{};
            item->setText(entry.name);
            item->setData(Qt::UserRole, QString::fromStdString(entry.path.generic_string()));
            item->setData(Qt::UserRole + 1, static_cast<int>(entry.type));
            item->setToolTip(QString::fromStdString(entry.path.string()));
            m_grid->addItem(item);
        }

        LoadThumbnailsBatched(0);
    }

    static constexpr int THUMB_BATCH_SIZE = 4;

    void AssetPickerPopup::LoadThumbnailsBatched(int startIndex)
    {
        int loaded = 0;
        for (int i = startIndex; i < m_grid->count() && loaded < THUMB_BATCH_SIZE; ++i)
        {
            auto* item = m_grid->item(i);
            auto type = static_cast<AssetType>(item->data(Qt::UserRole + 1).toInt());
            if (type != AssetType::TEXTURE)
                continue;

            if (item->data(Qt::DecorationRole).isValid())
                continue;

            QString path = item->data(Qt::UserRole).toString();
            path.replace('/', QDir::separator());

            QImage img;
            if (img.load(path))
            {
                QPixmap pix = QPixmap::fromImage(
                    img.scaled(PICKER_THUMB_SIZE, PICKER_THUMB_SIZE,
                               Qt::KeepAspectRatio, Qt::SmoothTransformation));
                item->setData(Qt::DecorationRole, pix);
            }
            ++loaded;
        }

        int nextIndex = startIndex;
        for (int i = startIndex; i < m_grid->count(); ++i)
        {
            auto* item = m_grid->item(i);
            if (static_cast<AssetType>(item->data(Qt::UserRole + 1).toInt()) == AssetType::TEXTURE &&
                !item->data(Qt::DecorationRole).isValid())
            {
                nextIndex = i;
                break;
            }
            nextIndex = m_grid->count();
        }

        if (nextIndex < m_grid->count())
        {
            QTimer::singleShot(0, this, [this, nextIndex] { LoadThumbnailsBatched(nextIndex); });
        }
    }

    void AssetPickerPopup::ApplyFilter()
    {
        QString filter = m_searchField->text().trimmed();
        for (int i = 0; i < m_grid->count(); ++i)
        {
            auto* item = m_grid->item(i);
            if (filter.isEmpty())
            {
                item->setHidden(false);
            }
            else
            {
                item->setHidden(!item->text().contains(filter, Qt::CaseInsensitive));
            }
        }
    }
} // namespace forge
