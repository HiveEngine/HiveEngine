#include <forge/asset_browser.h>

#include <QApplication>
#include <QDesktopServices>
#include <QDrag>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QMouseEvent>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QKeyEvent>
#include <QLabel>
#include <QMenu>
#include <QMessageBox>
#include <QMimeData>
#include <QMutex>
#include <QMutexLocker>
#include <QPainter>
#include <QScrollBar>
#include <QSlider>
#include <QStyledItemDelegate>
#include <QThread>
#include <QToolButton>
#include <QUrl>
#include <QVBoxLayout>

#include <forge/forge_module.h>

#include <hive/core/log.h>

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <string>
#include <unordered_map>
#include <unordered_set>

static const hive::LogCategory LOG_ASSETS{"AssetBrowser"};

namespace forge
{
    static const char* kAssetPathMime = "application/x-hive-asset-paths";
    static constexpr int kThumbnailMaxSize = 160;

    class ThumbnailLoader : public QObject
    {
        Q_OBJECT
    public:
        void Request(const QString& path)
        {
            {
                QMutexLocker lock{&m_mutex};
                auto key = path.toStdString();
                if (m_queued.count(key))
                    return;
                m_queued.insert(key);
                m_queue.push_back(path);
            }
            QMetaObject::invokeMethod(this, &ThumbnailLoader::ProcessNext, Qt::QueuedConnection);
        }

    signals:
        void Ready(const QString& path, const QImage& image);
        void Failed(const QString& path);

    private:
        void ProcessNext()
        {
            QString path;
            {
                QMutexLocker lock{&m_mutex};
                if (m_queue.empty())
                    return;
                path = m_queue.back();
                m_queue.pop_back();
            }

            QImage img;
            if (img.load(path))
            {
                QImage scaled = img.scaled(kThumbnailMaxSize, kThumbnailMaxSize,
                                           Qt::KeepAspectRatio, Qt::SmoothTransformation);
                emit Ready(path, scaled);
            }
            else
            {
                emit Failed(path);
            }

            QMutexLocker lock{&m_mutex};
            if (!m_queue.empty())
                QMetaObject::invokeMethod(this, &ThumbnailLoader::ProcessNext, Qt::QueuedConnection);
        }

        QMutex m_mutex;
        std::vector<QString> m_queue;
        std::unordered_set<std::string> m_queued;
    };

    class ThumbnailCache : public QObject
    {
        Q_OBJECT
    public:
        explicit ThumbnailCache(QObject* parent = nullptr)
            : QObject{parent}
        {
            m_loader = new ThumbnailLoader{};
            m_loader->moveToThread(&m_thread);
            connect(&m_thread, &QThread::finished, m_loader, &QObject::deleteLater);
            connect(m_loader, &ThumbnailLoader::Ready, this, &ThumbnailCache::OnReady);
            connect(m_loader, &ThumbnailLoader::Failed, this, &ThumbnailCache::OnFailed);
            m_thread.start();
        }

        ~ThumbnailCache() override
        {
            m_thread.quit();
            m_thread.wait();
        }

        QPixmap Get(const QString& path)
        {
            auto key = path.toStdString();
            auto it = m_cache.find(key);
            if (it != m_cache.end())
                return it->second;
            if (m_failed.count(key))
                return {};
            m_loader->Request(path);
            return {};
        }

        void Clear()
        {
            m_cache.clear();
            m_failed.clear();
        }

    signals:
        void ThumbnailLoaded();

    private:
        void OnReady(const QString& path, const QImage& image)
        {
            m_cache[path.toStdString()] = QPixmap::fromImage(image);
            emit ThumbnailLoaded();
        }

        void OnFailed(const QString& path)
        {
            m_failed.insert(path.toStdString());
        }

        std::unordered_map<std::string, QPixmap> m_cache;
        std::unordered_set<std::string> m_failed;
        QThread m_thread;
        ThumbnailLoader* m_loader{};
    };

    DropFolderTree::DropFolderTree(AssetBrowserPanel* browser, QWidget* parent)
        : QTreeWidget{parent}
        , m_browser{browser}
    {
        setAcceptDrops(true);
    }

    void DropFolderTree::dragEnterEvent(QDragEnterEvent* event)
    {
        if (event->mimeData()->hasFormat(kAssetPathMime))
            event->acceptProposedAction();
        else
            event->ignore();
    }

    void DropFolderTree::dragMoveEvent(QDragMoveEvent* event)
    {
        auto* item = itemAt(event->position().toPoint());
        if (item != nullptr && event->mimeData()->hasFormat(kAssetPathMime))
        {
            setCurrentItem(item);
            event->acceptProposedAction();
        }
        else
        {
            event->ignore();
        }
    }

    void DropFolderTree::dropEvent(QDropEvent* event)
    {
        auto* item = itemAt(event->position().toPoint());
        if (item == nullptr || !event->mimeData()->hasFormat(kAssetPathMime))
        {
            event->ignore();
            return;
        }

        auto dstDir = std::filesystem::path{item->data(0, Qt::UserRole).toString().toStdString()};
        auto paths = QString::fromUtf8(event->mimeData()->data(kAssetPathMime)).split('\n', Qt::SkipEmptyParts);

        for (const auto& p : paths)
        {
            auto src = std::filesystem::path{p.toStdString()};
            if (src.parent_path() != dstDir)
                m_browser->MoveAsset(src, dstDir);
        }

        event->acceptProposedAction();
    }

    DropContentList::DropContentList(AssetBrowserPanel* browser, QWidget* parent)
        : QListWidget{parent}
        , m_browser{browser}
    {
        setAcceptDrops(true);
        setDragEnabled(false);
        setSelectionMode(QAbstractItemView::ExtendedSelection);
    }

    void DropContentList::mousePressEvent(QMouseEvent* event)
    {
        m_didDrag = false;

        if (event->button() == Qt::LeftButton && itemAt(event->pos()) != nullptr)
        {
            m_dragStartPos = event->pos();
            m_dragPending = true;
            auto* hit = itemAt(event->pos());
            if (!(event->modifiers() & (Qt::ControlModifier | Qt::ShiftModifier)))
                clearSelection();
            if (hit != nullptr)
                hit->setSelected(true);
            setCurrentItem(hit);
            return;
        }

        m_dragPending = false;
        QListWidget::mousePressEvent(event);
    }

    void DropContentList::mouseMoveEvent(QMouseEvent* event)
    {
        if (m_dragPending && (event->buttons() & Qt::LeftButton))
        {
            int distance = (event->pos() - m_dragStartPos).manhattanLength();
            if (distance >= QApplication::startDragDistance())
            {
                m_dragPending = false;
                m_didDrag = true;
                StartDrag();
                return;
            }
            return;
        }
        QListWidget::mouseMoveEvent(event);
    }

    void DropContentList::mouseReleaseEvent(QMouseEvent* event)
    {
        m_dragPending = false;
        if (m_didDrag)
        {
            m_didDrag = false;
            return;
        }
        QListWidget::mouseReleaseEvent(event);
    }

    void DropContentList::mouseDoubleClickEvent(QMouseEvent* event)
    {
        m_dragPending = false;
        auto* hit = itemAt(event->pos());
        if (hit != nullptr)
            emit itemDoubleClicked(hit);
    }

    static constexpr int kScrollStep = 40;

    void DropContentList::wheelEvent(QWheelEvent* event)
    {
        int delta = event->angleDelta().y();
        verticalScrollBar()->setValue(verticalScrollBar()->value() - (delta > 0 ? kScrollStep : -kScrollStep));
        event->accept();
    }

    void DropContentList::StartDrag()
    {
        auto items = selectedItems();
        if (items.isEmpty())
            return;

        QStringList paths;
        for (auto* item : items)
        {
            auto path = item->data(Qt::UserRole).toString();
            if (!path.isEmpty())
                paths.append(path);
        }

        if (paths.isEmpty())
            return;

        auto* data = new QMimeData{};
        data->setData(kAssetPathMime, paths.join('\n').toUtf8());

        auto* drag = new QDrag{this};
        drag->setMimeData(data);
        drag->exec(Qt::MoveAction);
    }

    void DropContentList::dragEnterEvent(QDragEnterEvent* event)
    {
        if (event->mimeData()->hasFormat(kAssetPathMime))
            event->acceptProposedAction();
        else
            event->ignore();
    }

    void DropContentList::dragMoveEvent(QDragMoveEvent* event)
    {
        if (!event->mimeData()->hasFormat(kAssetPathMime))
        {
            event->ignore();
            return;
        }

        auto* item = itemAt(event->position().toPoint());
        if (item != nullptr && static_cast<AssetType>(item->data(Qt::UserRole + 1).toInt()) == AssetType::Folder)
            event->acceptProposedAction();
        else
            event->ignore();
    }

    void DropContentList::dropEvent(QDropEvent* event)
    {
        if (!event->mimeData()->hasFormat(kAssetPathMime))
        {
            event->ignore();
            return;
        }

        auto* targetItem = itemAt(event->position().toPoint());
        if (targetItem == nullptr ||
            static_cast<AssetType>(targetItem->data(Qt::UserRole + 1).toInt()) != AssetType::Folder)
        {
            event->ignore();
            return;
        }

        auto dstDir = std::filesystem::path{targetItem->data(Qt::UserRole).toString().toStdString()};
        auto paths = QString::fromUtf8(event->mimeData()->data(kAssetPathMime)).split('\n', Qt::SkipEmptyParts);
        for (const auto& p : paths)
        {
            auto src = std::filesystem::path{p.toStdString()};
            if (src.parent_path() != dstDir && src != dstDir)
                m_browser->MoveAsset(src, dstDir);
        }

        event->acceptProposedAction();
    }

    AssetType ClassifyExtension(const std::string& ext)
    {
        if (ext == ".nmsh")
            return AssetType::Mesh;
        if (ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".tga" || ext == ".bmp" || ext == ".hdr")
            return AssetType::Texture;
        if (ext == ".hlsl" || ext == ".glsl" || ext == ".vert" || ext == ".frag")
            return AssetType::Shader;
        if (ext == ".hscene")
            return AssetType::Scene;
        if (ext == ".hmat")
            return AssetType::Material;
        if (ext == ".wav" || ext == ".ogg" || ext == ".flac" || ext == ".mp3")
            return AssetType::Audio;
        return AssetType::Unknown;
    }

    static bool IsSupportedExtension(const std::string& ext)
    {
        return ClassifyExtension(ext) != AssetType::Unknown;
    }

    static QColor IconColor(AssetType type)
    {
        switch (type)
        {
        case AssetType::Folder:
            return QColor{0xf0, 0xa5, 0x00};
        case AssetType::Mesh:
            return QColor{0x4f, 0xc3, 0xf7};
        case AssetType::Texture:
            return QColor{0x81, 0xc7, 0x84};
        case AssetType::Shader:
            return QColor{0xce, 0x93, 0xd8};
        case AssetType::Scene:
            return QColor{0xff, 0xb7, 0x4d};
        case AssetType::Material:
            return QColor{0xef, 0x53, 0x50};
        case AssetType::Audio:
            return QColor{0xff, 0xf1, 0x76};
        default:
            return QColor{0x90, 0x90, 0x90};
        }
    }

    static const char* TypeLabel(AssetType type)
    {
        switch (type)
        {
        case AssetType::Mesh:
            return "MESH";
        case AssetType::Texture:
            return "TEX";
        case AssetType::Shader:
            return "SHADER";
        case AssetType::Scene:
            return "SCENE";
        case AssetType::Material:
            return "MAT";
        case AssetType::Audio:
            return "AUDIO";
        default:
            return "FILE";
        }
    }

    static void PaintFolderIcon(QPainter& p, const QRect& rect, const QColor& color)
    {
        p.setRenderHint(QPainter::Antialiasing);

        int x = rect.x();
        int y = rect.y();
        int w = rect.width();
        int h = rect.height();
        int margin = w / 6;

        QRect body{x + margin, y + h / 4, w - 2 * margin, h / 2};
        int tabW = body.width() * 2 / 5;
        int tabH = body.height() / 6;

        QColor darkColor = color.darker(140);
        QColor bodyColor = color.darker(120);

        p.setPen(Qt::NoPen);
        p.setBrush(darkColor);
        p.drawRoundedRect(body.x(), body.y() - tabH, tabW, tabH + 4, 3, 3);

        p.setBrush(bodyColor);
        p.drawRoundedRect(body, 4, 4);

        QRect front{body.x(), body.y() + body.height() / 5, body.width(), body.height() * 4 / 5};
        p.setBrush(color);
        p.drawRoundedRect(front, 4, 4);
    }

    static void PaintFileIcon(QPainter& p, const QRect& rect, const QColor& color, const char* label)
    {
        p.setRenderHint(QPainter::Antialiasing);

        int x = rect.x();
        int y = rect.y();
        int w = rect.width();
        int h = rect.height();
        int margin = w / 5;

        QRect doc{x + margin, y + h / 8, w - 2 * margin, h * 3 / 4};

        p.setPen(Qt::NoPen);
        p.setBrush(QColor{0, 0, 0, 40});
        p.drawRoundedRect(doc.adjusted(2, 2, 2, 2), 4, 4);

        QColor bgColor = color.darker(300);
        bgColor.setAlpha(200);
        p.setBrush(bgColor);
        p.setPen(QPen{color.darker(150), 1.5});
        p.drawRoundedRect(doc, 4, 4);

        int foldSize = doc.width() / 4;
        QPointF foldPoints[3] = {
            QPointF{doc.right() - foldSize + 0.5, static_cast<double>(doc.top())},
            QPointF{static_cast<double>(doc.right()) + 0.5, static_cast<double>(doc.top()) + foldSize},
            QPointF{doc.right() - foldSize + 0.5, static_cast<double>(doc.top()) + foldSize},
        };
        p.setBrush(color.darker(200));
        p.setPen(Qt::NoPen);
        p.drawPolygon(foldPoints, 3);

        if (label != nullptr)
        {
            QFont badgeFont{"Segoe UI", 7, QFont::Bold};
            p.setFont(badgeFont);
            QFontMetrics fm{badgeFont};
            int textW = fm.horizontalAdvance(label);
            int badgeH = fm.height() + 2;
            int badgeW = textW + 8;
            int badgeX = doc.center().x() - badgeW / 2;
            int badgeY = doc.center().y() - badgeH / 2 + doc.height() / 8;

            p.setBrush(color);
            p.setPen(Qt::NoPen);
            p.drawRoundedRect(badgeX, badgeY, badgeW, badgeH, 3, 3);

            p.setPen(QColor{0x0d, 0x0d, 0x0d});
            p.drawText(QRect{badgeX, badgeY, badgeW, badgeH}, Qt::AlignCenter, label);
        }
    }

    class ContentItemDelegate : public QStyledItemDelegate
    {
    public:
        explicit ContentItemDelegate(ThumbnailCache* cache, QObject* parent = nullptr)
            : QStyledItemDelegate{parent}
            , m_cache{cache}
        {
        }

        void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override
        {
            painter->save();

            if (option.state & QStyle::State_Selected)
            {
                painter->fillRect(option.rect, QColor{0x3d, 0x2e, 0x0a});
            }
            else if (option.state & QStyle::State_MouseOver)
            {
                painter->fillRect(option.rect, QColor{0x1e, 0x1e, 0x1e});
            }

            QRect iconRect = option.rect;
            iconRect.setHeight(iconRect.width());

            auto type = static_cast<AssetType>(index.data(Qt::UserRole + 1).toInt());
            QColor color = IconColor(type);

            if (type == AssetType::Folder)
            {
                PaintFolderIcon(*painter, iconRect, color);
            }
            else if (type == AssetType::Texture && m_cache != nullptr)
            {
                QString filePath = index.data(Qt::UserRole).toString();
                QPixmap thumb = m_cache->Get(filePath);
                if (!thumb.isNull())
                {
                    QPixmap scaled = thumb.scaled(iconRect.size(), Qt::KeepAspectRatio, Qt::SmoothTransformation);
                    int dx = (iconRect.width() - scaled.width()) / 2;
                    int dy = (iconRect.height() - scaled.height()) / 2;
                    painter->drawPixmap(iconRect.x() + dx, iconRect.y() + dy, scaled);

                    QFont badgeFont{"Segoe UI", 6, QFont::Bold};
                    painter->setFont(badgeFont);
                    QFontMetrics bfm{badgeFont};
                    int bw = bfm.horizontalAdvance("TEX") + 6;
                    int bh = bfm.height() + 2;
                    QRect badgeRect{iconRect.right() - bw - 2, iconRect.bottom() - bh - 2, bw, bh};
                    painter->setPen(Qt::NoPen);
                    painter->setBrush(QColor{0x81, 0xc7, 0x84, 200});
                    painter->setRenderHint(QPainter::Antialiasing);
                    painter->drawRoundedRect(badgeRect, 3, 3);
                    painter->setPen(QColor{0x0d, 0x0d, 0x0d});
                    painter->drawText(badgeRect, Qt::AlignCenter, "TEX");
                }
                else
                {
                    PaintFileIcon(*painter, iconRect, color, TypeLabel(type));
                }
            }
            else
            {
                PaintFileIcon(*painter, iconRect, color, TypeLabel(type));
            }

            QRect textRect = option.rect;
            textRect.setTop(iconRect.bottom() + 2);
            textRect.adjust(2, 0, -2, 0);

            QString name = index.data(Qt::DisplayRole).toString();
            QFont font{"Segoe UI", 8};
            painter->setFont(font);

            QColor textColor =
                (option.state & QStyle::State_Selected) ? QColor{0xf0, 0xa5, 0x00} : QColor{0xcc, 0xcc, 0xcc};
            painter->setPen(textColor);

            QFontMetrics fm{font};
            QString elided = fm.elidedText(name, Qt::ElideMiddle, textRect.width());
            painter->drawText(textRect, Qt::AlignHCenter | Qt::AlignTop | Qt::TextWordWrap, elided);

            painter->restore();
        }

        QSize sizeHint(const QStyleOptionViewItem& option, const QModelIndex&) const override
        {
            int size = option.decorationSize.width();
            if (size <= 0)
                size = 80;
            return QSize{size + 8, size + 28};
        }

    private:
        ThumbnailCache* m_cache{};
    };

    void FileUndoStack::PushCreateFolder(const std::filesystem::path& created)
    {
        m_redoStack.clear();
        m_undoStack.push_back({FileAction::CreateFolder, created, {}});
    }

    void FileUndoStack::PushDelete(const std::filesystem::path& original, const std::filesystem::path& trashPath)
    {
        m_redoStack.clear();
        m_undoStack.push_back({FileAction::Delete, original, trashPath});
    }

    void FileUndoStack::PushRename(const std::filesystem::path& newPath, const std::filesystem::path& oldPath)
    {
        m_redoStack.clear();
        m_undoStack.push_back({FileAction::Rename, newPath, oldPath});
    }

    void FileUndoStack::PushMove(const std::filesystem::path& newPath, const std::filesystem::path& oldPath)
    {
        m_redoStack.clear();
        m_undoStack.push_back({FileAction::Move, newPath, oldPath});
    }

    bool FileUndoStack::Undo()
    {
        if (m_undoStack.empty())
            return false;

        auto entry = std::move(m_undoStack.back());
        m_undoStack.pop_back();

        if (!Execute(entry, true))
            return false;

        m_redoStack.push_back(std::move(entry));
        return true;
    }

    bool FileUndoStack::Redo()
    {
        if (m_redoStack.empty())
            return false;

        auto entry = std::move(m_redoStack.back());
        m_redoStack.pop_back();

        if (!Execute(entry, false))
            return false;

        m_undoStack.push_back(std::move(entry));
        return true;
    }

    bool FileUndoStack::Execute(const FileUndoEntry& entry, bool undo)
    {
        std::error_code ec;
        switch (entry.m_action)
        {
        case FileAction::CreateFolder:
            if (undo)
                std::filesystem::remove_all(entry.m_path, ec);
            else
                std::filesystem::create_directory(entry.m_path, ec);
            break;

        case FileAction::Delete:
            if (undo)
                std::filesystem::rename(entry.m_auxPath, entry.m_path, ec);
            else
                std::filesystem::rename(entry.m_path, entry.m_auxPath, ec);
            break;

        case FileAction::Rename:
            if (undo)
                std::filesystem::rename(entry.m_path, entry.m_auxPath, ec);
            else
                std::filesystem::rename(entry.m_auxPath, entry.m_path, ec);
            break;

        case FileAction::Move:
            if (undo)
                std::filesystem::rename(entry.m_path, entry.m_auxPath, ec);
            else
                std::filesystem::rename(entry.m_auxPath, entry.m_path, ec);
            break;
        }

        if (ec)
        {
            hive::LogWarning(LOG_ASSETS, "File undo/redo failed: {}", ec.message());
            return false;
        }
        return true;
    }

    static QToolButton* MakeToolBtn(QWidget* parent, const QString& text, const QString& tooltip)
    {
        auto* btn = new QToolButton{parent};
        btn->setText(text);
        btn->setToolTip(tooltip);
        btn->setCursor(Qt::PointingHandCursor);
        btn->setStyleSheet("QToolButton {"
                           "  background: transparent; color: #aaa; border: none; border-radius: 3px;"
                           "  padding: 2px 6px; font-size: 13px; font-family: 'Segoe UI', sans-serif;"
                           "}"
                           "QToolButton:hover { background: #222; color: #e8e8e8; }"
                           "QToolButton:disabled { color: #444; }");
        return btn;
    }

    AssetBrowserPanel::AssetBrowserPanel(QWidget* parent)
        : QWidget{parent}
        , m_historyBack{forge::GetAllocator()}
        , m_historyForward{forge::GetAllocator()}
    {
        setFocusPolicy(Qt::StrongFocus);

        m_thumbnailCache = new ThumbnailCache{this};
        connect(m_thumbnailCache, &ThumbnailCache::ThumbnailLoaded, this, [this] {
            m_contentList->viewport()->update();
        });

        auto* mainLayout = new QVBoxLayout{this};
        mainLayout->setContentsMargins(0, 0, 0, 0);
        mainLayout->setSpacing(0);

        BuildToolbar();
        mainLayout->addWidget(m_breadcrumbBar);

        m_splitter = new QSplitter{Qt::Horizontal, this};
        m_splitter->setStyleSheet("QSplitter::handle { background: #2a2a2a; width: 1px; }");

        BuildFolderTree();
        BuildContentArea();

        m_splitter->addWidget(m_folderTree);
        m_splitter->addWidget(m_contentList);
        m_splitter->setStretchFactor(0, 0);
        m_splitter->setStretchFactor(1, 1);
        m_splitter->setSizes({180, 600});

        mainLayout->addWidget(m_splitter);

        BuildStatusBar();
        mainLayout->addWidget(m_statusBar);
    }

    void AssetBrowserPanel::BuildToolbar()
    {
        m_breadcrumbBar = new QWidget{this};
        m_breadcrumbBar->setFixedHeight(32);
        m_breadcrumbBar->setStyleSheet("QWidget { background: #141414; border-bottom: 1px solid #2a2a2a; }");

        auto* layout = new QHBoxLayout{m_breadcrumbBar};
        layout->setContentsMargins(4, 0, 4, 0);
        layout->setSpacing(2);

        m_backBtn = MakeToolBtn(m_breadcrumbBar, "\xe2\x86\x90", "Back (Backspace)");
        m_forwardBtn = MakeToolBtn(m_breadcrumbBar, "\xe2\x86\x92", "Forward");
        m_upBtn = MakeToolBtn(m_breadcrumbBar, "\xe2\x86\x91", "Up");

        m_backBtn->setEnabled(false);
        m_forwardBtn->setEnabled(false);
        m_upBtn->setEnabled(false);

        connect(m_backBtn, &QToolButton::clicked, this, &AssetBrowserPanel::NavigateBack);
        connect(m_forwardBtn, &QToolButton::clicked, this, &AssetBrowserPanel::NavigateForward);
        connect(m_upBtn, &QToolButton::clicked, this, &AssetBrowserPanel::NavigateUp);

        layout->addWidget(m_backBtn);
        layout->addWidget(m_forwardBtn);
        layout->addWidget(m_upBtn);

        auto* sep1 = new QWidget{m_breadcrumbBar};
        sep1->setFixedWidth(1);
        sep1->setStyleSheet("background: #2a2a2a;");
        layout->addWidget(sep1);

        m_undoBtn = MakeToolBtn(m_breadcrumbBar, "\xe2\x86\xb6", "Undo (Ctrl+Z)");
        m_redoBtn = MakeToolBtn(m_breadcrumbBar, "\xe2\x86\xb7", "Redo (Ctrl+Y)");
        m_undoBtn->setEnabled(false);
        m_redoBtn->setEnabled(false);
        connect(m_undoBtn, &QToolButton::clicked, this, &AssetBrowserPanel::UndoFileAction);
        connect(m_redoBtn, &QToolButton::clicked, this, &AssetBrowserPanel::RedoFileAction);
        layout->addWidget(m_undoBtn);
        layout->addWidget(m_redoBtn);

        auto* sep2 = new QWidget{m_breadcrumbBar};
        sep2->setFixedWidth(1);
        sep2->setStyleSheet("background: #2a2a2a;");
        layout->addWidget(sep2);

        layout->addSpacing(6);
        layout->addStretch();

        m_searchField = new QLineEdit{m_breadcrumbBar};
        m_searchField->setPlaceholderText("Search...");
        m_searchField->setFixedWidth(180);
        m_searchField->setFixedHeight(22);
        m_searchField->setStyleSheet("QLineEdit {"
                                     "  background: #1a1a1a; color: #ccc; border: 1px solid #333; border-radius: 11px;"
                                     "  padding: 0 10px; font-size: 11px; font-family: 'Segoe UI', sans-serif;"
                                     "}"
                                     "QLineEdit:focus { border-color: #f0a500; }");
        connect(m_searchField, &QLineEdit::textChanged, this, &AssetBrowserPanel::ApplySearchFilter);
        layout->addWidget(m_searchField);

        layout->addSpacing(6);

        auto* newFolderBtn = MakeToolBtn(m_breadcrumbBar, "+", "New Folder");
        newFolderBtn->setStyleSheet("QToolButton {"
                                    "  background: transparent; color: #aaa; border: 1px solid #333; border-radius: 3px;"
                                    "  padding: 2px 8px; font-size: 14px; font-weight: bold;"
                                    "  font-family: 'Segoe UI', sans-serif;"
                                    "}"
                                    "QToolButton:hover { border-color: #f0a500; color: #f0a500; }");
        connect(newFolderBtn, &QToolButton::clicked, this, &AssetBrowserPanel::OnNewFolderClicked);
        layout->addWidget(newFolderBtn);

        auto* importBtn = MakeToolBtn(m_breadcrumbBar, "Import", "Import Assets");
        importBtn->setStyleSheet("QToolButton {"
                                 "  background: #1a3a1a; color: #6abf6a; border: none; border-radius: 3px;"
                                 "  padding: 2px 12px; font-size: 11px; font-weight: bold;"
                                 "  font-family: 'Segoe UI', sans-serif;"
                                 "}"
                                 "QToolButton:hover { background: #2a4a2a; color: #8adf8a; }");
        connect(importBtn, &QToolButton::clicked, this, &AssetBrowserPanel::OnImportClicked);
        layout->addWidget(importBtn);
    }

    void AssetBrowserPanel::BuildFolderTree()
    {
        m_folderTree = new DropFolderTree{this, m_splitter};
        m_folderTree->setHeaderHidden(true);
        m_folderTree->setEditTriggers(QAbstractItemView::NoEditTriggers);
        m_folderTree->setIndentation(14);
        m_folderTree->setContextMenuPolicy(Qt::CustomContextMenu);
        m_folderTree->setStyleSheet("QTreeWidget {"
                                    "  background: #111; border: none; border-right: 1px solid #2a2a2a;"
                                    "}"
                                    "QTreeWidget::item { padding: 3px 0; }"
                                    "QTreeWidget::item:hover { background: #1e1e1e; }"
                                    "QTreeWidget::item:selected { background: #3d2e0a; color: #f0a500; }");

        connect(m_folderTree, &QTreeWidget::itemClicked, this, &AssetBrowserPanel::OnFolderTreeClicked);
        connect(m_folderTree, &QTreeWidget::customContextMenuRequested, this,
                &AssetBrowserPanel::OnFolderTreeContextMenu);
    }

    void AssetBrowserPanel::BuildContentArea()
    {
        m_contentList = new DropContentList{this, m_splitter};
        m_contentList->setViewMode(QListView::IconMode);
        m_contentList->setResizeMode(QListView::Adjust);
        m_contentList->setMovement(QListView::Static);
        m_contentList->setIconSize(QSize{m_iconSize, m_iconSize});
        m_contentList->setGridSize(QSize{m_iconSize + 8, m_iconSize + 28});
        m_contentList->setSpacing(4);
        m_contentList->setUniformItemSizes(true);
        m_contentList->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
        m_contentList->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        m_contentList->setContextMenuPolicy(Qt::CustomContextMenu);
        m_contentList->setStyleSheet("QListWidget {"
                                     "  background: #111; border: none; outline: none;"
                                     "}"
                                     "QListWidget::item { border-radius: 4px; }"
                                     "QListWidget::item:hover { background: #1e1e1e; }"
                                     "QListWidget::item:selected { background: #3d2e0a; }");

        m_contentList->setItemDelegate(new ContentItemDelegate{m_thumbnailCache, m_contentList});

        connect(m_contentList, &QListWidget::itemDoubleClicked, this, &AssetBrowserPanel::OnContentDoubleClicked);
        connect(m_contentList, &QListWidget::customContextMenuRequested, this,
                &AssetBrowserPanel::OnContentContextMenu);
    }

    void AssetBrowserPanel::BuildStatusBar()
    {
        m_statusBar = new QWidget{this};
        m_statusBar->setFixedHeight(24);
        m_statusBar->setStyleSheet("QWidget { background: #141414; border-top: 1px solid #2a2a2a; }");

        auto* layout = new QHBoxLayout{m_statusBar};
        layout->setContentsMargins(8, 0, 8, 0);
        layout->setSpacing(4);

        m_statusLabel = new QLabel{"0 items", m_statusBar};
        m_statusLabel->setStyleSheet("color: #666; font-size: 10px; font-family: 'Segoe UI', sans-serif;");
        layout->addWidget(m_statusLabel);

        layout->addStretch();

        auto* sizeIcon = new QLabel{m_statusBar};
        sizeIcon->setText("\xe2\x96\xa3");
        sizeIcon->setStyleSheet("color: #555; font-size: 10px;");
        layout->addWidget(sizeIcon);

        m_sizeSlider = new QSlider{Qt::Horizontal, m_statusBar};
        m_sizeSlider->setRange(48, 160);
        m_sizeSlider->setValue(m_iconSize);
        m_sizeSlider->setFixedWidth(100);
        m_sizeSlider->setFixedHeight(16);
        m_sizeSlider->setStyleSheet("QSlider::groove:horizontal {"
                                    "  background: #222; height: 4px; border-radius: 2px;"
                                    "}"
                                    "QSlider::handle:horizontal {"
                                    "  background: #888; width: 10px; height: 10px; margin: -3px 0;"
                                    "  border-radius: 5px;"
                                    "}"
                                    "QSlider::handle:horizontal:hover { background: #f0a500; }");
        connect(m_sizeSlider, &QSlider::valueChanged, this, &AssetBrowserPanel::UpdateIconSize);
        layout->addWidget(m_sizeSlider);
    }

    void AssetBrowserPanel::SetAssetsRoot(const char* path)
    {
        m_root = std::filesystem::path{path};
        m_currentDir = m_root;
        m_historyBack.Clear();
        m_historyForward.Clear();
        Refresh();
    }

    void AssetBrowserPanel::Refresh()
    {
        m_folderTree->clear();

        if (m_root.empty() || !std::filesystem::exists(m_root))
        {
            m_contentList->clear();
            auto* item = new QListWidgetItem{"No assets directory"};
            item->setFlags(item->flags() & ~Qt::ItemIsSelectable);
            m_contentList->addItem(item);
            UpdateStatusBar();
            return;
        }

        auto* rootItem = new QTreeWidgetItem{m_folderTree};
        rootItem->setText(0, QString::fromStdString(m_root.filename().string()));
        rootItem->setData(0, Qt::UserRole, QString::fromStdString(m_root.generic_string()));
        rootItem->setForeground(0, QColor{0xf0, 0xa5, 0x00});
        PopulateFolderTree(rootItem, m_root);
        rootItem->setExpanded(true);

        if (!std::filesystem::exists(m_currentDir) ||
            m_currentDir.generic_string().find(m_root.generic_string()) == std::string::npos)
        {
            m_currentDir = m_root;
        }

        SyncTreeSelection();
        PopulateContent();
        UpdateBreadcrumb();
        UpdateStatusBar();

        m_undoBtn->setEnabled(m_fileUndo.CanUndo());
        m_redoBtn->setEnabled(m_fileUndo.CanRedo());
    }

    void AssetBrowserPanel::NavigateTo(const std::filesystem::path& dir)
    {
        if (dir == m_currentDir)
            return;

        m_historyBack.PushBack(m_currentDir);
        m_historyForward.Clear();
        m_currentDir = dir;

        SyncTreeSelection();
        PopulateContent();
        UpdateBreadcrumb();
        UpdateStatusBar();
    }

    void AssetBrowserPanel::NavigateBack()
    {
        if (m_historyBack.IsEmpty())
            return;

        m_historyForward.PushBack(m_currentDir);
        m_currentDir = m_historyBack.Back();
        m_historyBack.PopBack();

        SyncTreeSelection();
        PopulateContent();
        UpdateBreadcrumb();
        UpdateStatusBar();
    }

    void AssetBrowserPanel::NavigateForward()
    {
        if (m_historyForward.IsEmpty())
            return;

        m_historyBack.PushBack(m_currentDir);
        m_currentDir = m_historyForward.Back();
        m_historyForward.PopBack();

        SyncTreeSelection();
        PopulateContent();
        UpdateBreadcrumb();
        UpdateStatusBar();
    }

    void AssetBrowserPanel::NavigateUp()
    {
        if (m_currentDir == m_root)
            return;

        auto parent = m_currentDir.parent_path();
        if (parent.generic_string().find(m_root.generic_string()) != std::string::npos)
            NavigateTo(parent);
    }

    void AssetBrowserPanel::UpdateBreadcrumb()
    {
        m_backBtn->setEnabled(!m_historyBack.IsEmpty());
        m_forwardBtn->setEnabled(!m_historyForward.IsEmpty());
        m_upBtn->setEnabled(m_currentDir != m_root);

        auto* layout = m_breadcrumbBar->layout();
        QList<QWidget*> toRemove;
        for (int i = 0; i < layout->count(); ++i)
        {
            auto* w = layout->itemAt(i)->widget();
            if (w != nullptr && w->property("breadcrumb").toBool())
                toRemove.append(w);
        }
        for (auto* w : toRemove)
        {
            layout->removeWidget(w);
            delete w;
        }

        std::filesystem::path relative = std::filesystem::relative(m_currentDir, m_root);
        std::filesystem::path accumulated = m_root;

        // insert after: back, fwd, up, sep1, undo, redo, sep2, spacing
        auto insertPos = 9;

        auto addSegment = [&](const QString& name, const std::filesystem::path& target) {
            auto* btn = new QToolButton{m_breadcrumbBar};
            btn->setText(name);
            btn->setCursor(Qt::PointingHandCursor);
            btn->setProperty("breadcrumb", true);
            btn->setStyleSheet("QToolButton {"
                               "  background: transparent; color: #999; border: none; padding: 2px 4px;"
                               "  font-size: 11px; font-family: 'Segoe UI', sans-serif;"
                               "}"
                               "QToolButton:hover { color: #f0a500; }");

            std::filesystem::path capturedPath = target;
            connect(btn, &QToolButton::clicked, this, [this, capturedPath] { NavigateTo(capturedPath); });
            static_cast<QHBoxLayout*>(layout)->insertWidget(insertPos++, btn);

            if (target != m_currentDir)
            {
                auto* arrow = new QLabel{"\xe2\x96\xb8", m_breadcrumbBar};
                arrow->setProperty("breadcrumb", true);
                arrow->setStyleSheet("color: #555; font-size: 9px;");
                static_cast<QHBoxLayout*>(layout)->insertWidget(insertPos++, arrow);
            }
        };

        addSegment(QString::fromStdString(m_root.filename().string()), m_root);

        if (relative != ".")
        {
            for (const auto& part : relative)
            {
                accumulated /= part;
                addSegment(QString::fromStdString(part.string()), accumulated);
            }
        }
    }

    void AssetBrowserPanel::UpdateStatusBar()
    {
        int total = m_contentList->count();
        int selected = m_contentList->selectedItems().size();

        QString text;
        if (selected > 0)
            text = QString{"%1 items, %2 selected"}.arg(total).arg(selected);
        else
            text = QString{"%1 items"}.arg(total);

        m_statusLabel->setText(text);
    }

    void AssetBrowserPanel::PopulateFolderTree(QTreeWidgetItem* parent, const std::filesystem::path& dir)
    {
        wax::Vector<std::filesystem::directory_entry> dirs{forge::GetAllocator()};

        std::error_code ec;
        for (const auto& entry : std::filesystem::directory_iterator(dir, ec))
        {
            auto name = entry.path().filename().string();
            if (!name.empty() && name[0] == '.')
                continue;

            if (entry.is_directory())
                dirs.PushBack(entry);
        }

        std::sort(dirs.begin(), dirs.end());

        for (const auto& d : dirs)
        {
            auto* item = new QTreeWidgetItem{parent};
            item->setText(0, QString::fromStdString(d.path().filename().string()));
            item->setData(0, Qt::UserRole, QString::fromStdString(d.path().generic_string()));
            item->setForeground(0, QColor{0xcc, 0xcc, 0xcc});
            PopulateFolderTree(item, d.path());
        }
    }

    void AssetBrowserPanel::SyncTreeSelection()
    {
        QString target = QString::fromStdString(m_currentDir.generic_string());

        std::function<bool(QTreeWidgetItem*)> findAndSelect = [&](QTreeWidgetItem* item) -> bool {
            if (item->data(0, Qt::UserRole).toString() == target)
            {
                m_folderTree->blockSignals(true);
                m_folderTree->setCurrentItem(item);
                m_folderTree->scrollToItem(item);
                m_folderTree->blockSignals(false);

                auto* p = item->parent();
                while (p != nullptr)
                {
                    p->setExpanded(true);
                    p = p->parent();
                }
                return true;
            }
            for (int i = 0; i < item->childCount(); ++i)
            {
                if (findAndSelect(item->child(i)))
                    return true;
            }
            return false;
        };

        for (int i = 0; i < m_folderTree->topLevelItemCount(); ++i)
            findAndSelect(m_folderTree->topLevelItem(i));
    }

    void AssetBrowserPanel::PopulateContent()
    {
        m_contentList->clear();

        if (!std::filesystem::exists(m_currentDir))
            return;

        wax::Vector<std::filesystem::directory_entry> dirs{forge::GetAllocator()};
        wax::Vector<std::filesystem::directory_entry> files{forge::GetAllocator()};

        std::error_code ec;
        for (const auto& entry : std::filesystem::directory_iterator(m_currentDir, ec))
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
            auto* item = new QListWidgetItem{};
            item->setText(QString::fromStdString(d.path().filename().string()));
            item->setData(Qt::UserRole, QString::fromStdString(d.path().generic_string()));
            item->setData(Qt::UserRole + 1, static_cast<int>(AssetType::Folder));
            item->setToolTip(QString::fromStdString(d.path().string()));
            item->setFlags((item->flags() | Qt::ItemIsDragEnabled) & ~Qt::ItemIsDropEnabled);
            m_contentList->addItem(item);
        }

        for (const auto& f : files)
        {
            auto ext = f.path().extension().string();
            auto type = ClassifyExtension(ext);

            auto* item = new QListWidgetItem{};
            item->setText(QString::fromStdString(f.path().stem().string()));
            item->setData(Qt::UserRole, QString::fromStdString(f.path().generic_string()));
            item->setData(Qt::UserRole + 1, static_cast<int>(type));
            item->setToolTip(QString::fromStdString(f.path().string()));
            item->setFlags((item->flags() | Qt::ItemIsDragEnabled) & ~Qt::ItemIsDropEnabled);
            m_contentList->addItem(item);
        }

        ApplySearchFilter();
    }

    void AssetBrowserPanel::ApplySearchFilter()
    {
        QString filter = m_searchField->text().trimmed();
        for (int i = 0; i < m_contentList->count(); ++i)
        {
            auto* item = m_contentList->item(i);
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

    void AssetBrowserPanel::OnFolderTreeClicked(QTreeWidgetItem* item)
    {
        auto path = item->data(0, Qt::UserRole).toString().toStdString();
        if (!path.empty())
            NavigateTo(std::filesystem::path{path});
    }

    void AssetBrowserPanel::OnContentDoubleClicked(QListWidgetItem* item)
    {
        auto type = static_cast<AssetType>(item->data(Qt::UserRole + 1).toInt());
        if (type == AssetType::Folder)
        {
            auto path = item->data(Qt::UserRole).toString().toStdString();
            NavigateTo(std::filesystem::path{path});
        }
    }

    void AssetBrowserPanel::OnContentContextMenu(const QPoint& pos)
    {
        auto* item = m_contentList->itemAt(pos);
        QMenu menu{this};
        menu.setStyleSheet("QMenu { background: #1a1a1a; color: #e8e8e8; border: 1px solid #2a2a2a; }"
                           "QMenu::item:selected { background: #3d2e0a; color: #f0a500; }"
                           "QMenu::separator { background: #2a2a2a; height: 1px; margin: 4px 8px; }");

        menu.addAction("New Folder", this, &AssetBrowserPanel::OnNewFolderClicked);
        menu.addAction("Import...", this, &AssetBrowserPanel::OnImportClicked);
        menu.addSeparator();

        if (item != nullptr)
        {
            menu.addAction("Rename\tF2", this, &AssetBrowserPanel::RenameSelected);
            menu.addAction("Delete\tDel", this, &AssetBrowserPanel::DeleteSelected);
            menu.addSeparator();

            auto fsPath = std::filesystem::path{item->data(Qt::UserRole).toString().toStdString()};
            menu.addAction("Show in Explorer", this, [this, fsPath] { ShowInExplorer(fsPath); });
        }
        else
        {
            menu.addAction("Show in Explorer", this, [this] { ShowInExplorer(m_currentDir); });
        }

        menu.addSeparator();

        auto* undoAction = menu.addAction("Undo\tCtrl+Z", this, &AssetBrowserPanel::UndoFileAction);
        undoAction->setEnabled(m_fileUndo.CanUndo());
        auto* redoAction = menu.addAction("Redo\tCtrl+Y", this, &AssetBrowserPanel::RedoFileAction);
        redoAction->setEnabled(m_fileUndo.CanRedo());

        menu.exec(m_contentList->mapToGlobal(pos));
    }

    void AssetBrowserPanel::OnFolderTreeContextMenu(const QPoint& pos)
    {
        auto* item = m_folderTree->itemAt(pos);
        QMenu menu{this};
        menu.setStyleSheet("QMenu { background: #1a1a1a; color: #e8e8e8; border: 1px solid #2a2a2a; }"
                           "QMenu::item:selected { background: #3d2e0a; color: #f0a500; }"
                           "QMenu::separator { background: #2a2a2a; height: 1px; margin: 4px 8px; }");

        if (item != nullptr)
        {
            auto fsPath = std::filesystem::path{item->data(0, Qt::UserRole).toString().toStdString()};

            menu.addAction("New Folder", this, [this, fsPath] {
                NavigateTo(fsPath);
                OnNewFolderClicked();
            });
            menu.addSeparator();

            if (fsPath != m_root)
            {
                menu.addAction("Rename\tF2", this, [this, fsPath] { RenamePath(fsPath); });
                menu.addAction("Delete\tDel", this, [this, fsPath] { DeletePath(fsPath); });
                menu.addSeparator();
            }

            menu.addAction("Show in Explorer", this, [this, fsPath] { ShowInExplorer(fsPath); });
        }

        menu.exec(m_folderTree->mapToGlobal(pos));
    }

    void AssetBrowserPanel::keyPressEvent(QKeyEvent* event)
    {
        if (event->key() == Qt::Key_Backspace)
        {
            NavigateBack();
            return;
        }

        if (event->key() == Qt::Key_F2)
        {
            RenameSelected();
            return;
        }

        if (event->key() == Qt::Key_Delete)
        {
            DeleteSelected();
            return;
        }

        if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter)
        {
            auto items = m_contentList->selectedItems();
            if (items.size() == 1)
                OnContentDoubleClicked(items.first());
            return;
        }

        if (event->modifiers() & Qt::ControlModifier)
        {
            if (event->key() == Qt::Key_Z)
            {
                UndoFileAction();
                return;
            }
            if (event->key() == Qt::Key_Y)
            {
                RedoFileAction();
                return;
            }
        }

        QWidget::keyPressEvent(event);
    }

    std::filesystem::path AssetBrowserPanel::TrashPathFor(const std::filesystem::path& path)
    {
        auto trashDir = m_root / ".hive-trash";
        std::error_code ec;
        std::filesystem::create_directories(trashDir, ec);

        auto now = std::chrono::steady_clock::now().time_since_epoch().count();
        auto trashName = path.filename().string() + "." + std::to_string(now);
        return trashDir / trashName;
    }

    void AssetBrowserPanel::OnImportClicked()
    {
        if (m_root.empty())
            return;

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
            std::filesystem::path dstPath = m_currentDir / srcPath.filename();

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

    void AssetBrowserPanel::OnNewFolderClicked()
    {
        if (m_currentDir.empty())
            return;

        bool ok = false;
        auto name = QInputDialog::getText(this, "New Folder", "Folder name:", QLineEdit::Normal, "New Folder", &ok);
        if (!ok || name.isEmpty())
            return;

        auto newPath = m_currentDir / name.toStdString();
        std::error_code ec;
        std::filesystem::create_directory(newPath, ec);
        if (ec)
        {
            QMessageBox::warning(this, "Error",
                                 QString{"Failed to create folder: %1"}.arg(QString::fromStdString(ec.message())));
            return;
        }

        m_fileUndo.PushCreateFolder(newPath);
        Refresh();
    }

    void AssetBrowserPanel::RenamePath(const std::filesystem::path& fsPath)
    {
        bool ok = false;
        auto newName = QInputDialog::getText(this, "Rename", "New name:", QLineEdit::Normal,
                                             QString::fromStdString(fsPath.filename().string()), &ok);
        if (!ok || newName.isEmpty())
            return;

        auto newPath = fsPath.parent_path() / newName.toStdString();
        std::error_code ec;
        std::filesystem::rename(fsPath, newPath, ec);
        if (ec)
        {
            QMessageBox::warning(this, "Error",
                                 QString{"Failed to rename: %1"}.arg(QString::fromStdString(ec.message())));
            return;
        }

        m_fileUndo.PushRename(newPath, fsPath);

        if (m_currentDir.generic_string().find(fsPath.generic_string()) != std::string::npos)
            m_currentDir = newPath;

        Refresh();
    }

    void AssetBrowserPanel::DeletePath(const std::filesystem::path& fsPath)
    {
        auto answer = QMessageBox::question(
            this, "Delete",
            QString{"Delete '%1'?"}.arg(QString::fromStdString(fsPath.filename().string())));
        if (answer != QMessageBox::Yes)
            return;

        auto trashPath = TrashPathFor(fsPath);
        std::error_code ec;
        std::filesystem::rename(fsPath, trashPath, ec);
        if (ec)
        {
            QMessageBox::warning(this, "Error",
                                 QString{"Failed to delete: %1"}.arg(QString::fromStdString(ec.message())));
            return;
        }

        m_fileUndo.PushDelete(fsPath, trashPath);

        if (m_currentDir.generic_string().find(fsPath.generic_string()) != std::string::npos)
            m_currentDir = fsPath.parent_path();

        Refresh();
    }

    void AssetBrowserPanel::RenameSelected()
    {
        auto items = m_contentList->selectedItems();
        if (items.isEmpty())
            return;

        auto fsPath = std::filesystem::path{items.first()->data(Qt::UserRole).toString().toStdString()};
        RenamePath(fsPath);
    }

    void AssetBrowserPanel::DeleteSelected()
    {
        auto items = m_contentList->selectedItems();
        if (items.isEmpty())
            return;

        if (items.size() == 1)
        {
            auto fsPath = std::filesystem::path{items.first()->data(Qt::UserRole).toString().toStdString()};
            DeletePath(fsPath);
            return;
        }

        QString msg = QString{"Delete %1 items?"}.arg(items.size());
        if (QMessageBox::question(this, "Delete", msg) != QMessageBox::Yes)
            return;

        for (auto* item : items)
        {
            auto fsPath = std::filesystem::path{item->data(Qt::UserRole).toString().toStdString()};
            auto trashPath = TrashPathFor(fsPath);
            std::error_code ec;
            std::filesystem::rename(fsPath, trashPath, ec);
            if (!ec)
                m_fileUndo.PushDelete(fsPath, trashPath);
        }

        Refresh();
    }

    void AssetBrowserPanel::ShowInExplorer(const std::filesystem::path& path)
    {
        std::filesystem::path target = std::filesystem::is_directory(path) ? path : path.parent_path();
        QDesktopServices::openUrl(QUrl::fromLocalFile(QString::fromStdString(target.string())));
    }

    void AssetBrowserPanel::MoveAsset(const std::filesystem::path& src, const std::filesystem::path& dstDir)
    {
        auto dstPath = dstDir / src.filename();
        if (src == dstPath)
            return;

        std::error_code ec;
        std::filesystem::rename(src, dstPath, ec);
        if (ec)
        {
            QMessageBox::warning(this, "Error",
                                 QString{"Failed to move: %1"}.arg(QString::fromStdString(ec.message())));
            return;
        }

        m_fileUndo.PushMove(dstPath, src);
        Refresh();
    }

    void AssetBrowserPanel::UndoFileAction()
    {
        if (m_fileUndo.Undo())
            Refresh();
    }

    void AssetBrowserPanel::RedoFileAction()
    {
        if (m_fileUndo.Redo())
            Refresh();
    }

    void AssetBrowserPanel::UpdateIconSize(int size)
    {
        m_iconSize = size;
        m_contentList->setIconSize(QSize{size, size});
        m_contentList->setGridSize(QSize{size + 8, size + 28});
        PopulateContent();
    }
} // namespace forge

#include "asset_browser.moc"
