#include <forge/asset_browser.h>

#include <QApplication>
#include <QDesktopServices>
#include <QDrag>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QMouseEvent>
#include <QFile>
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

#include <forge/editor_undo.h>
#include <forge/forge_module.h>

#include <nectar/registry/hiveid_file.h>

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <string>
#include <unordered_map>
#include <unordered_set>

namespace forge
{
    static const char* ASSET_PATH_MIME = "application/x-hive-asset-paths";
    static constexpr int THUMBNAIL_MAX_SIZE = 160;
    static constexpr uint64_t FNV_OFFSET_BASIS = 0xcbf29ce484222325ULL;
    static constexpr uint64_t FNV_PRIME = 0x100000001b3ULL;

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

        void InvalidatePath(const QString& path)
        {
            QMutexLocker lock{&m_mutex};
            m_queued.erase(path.toStdString());
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
                QImage scaled = img.scaled(THUMBNAIL_MAX_SIZE, THUMBNAIL_MAX_SIZE,
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

        void Invalidate(const QString& path)
        {
            auto key = path.toStdString();
            m_cache.erase(key);
            m_failed.erase(key);
            m_loader->InvalidatePath(path);
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
        if (event->mimeData()->hasFormat(ASSET_PATH_MIME))
            event->acceptProposedAction();
        else
            event->ignore();
    }

    void DropFolderTree::dragMoveEvent(QDragMoveEvent* event)
    {
        auto* item = itemAt(event->position().toPoint());
        if (item != nullptr && event->mimeData()->hasFormat(ASSET_PATH_MIME))
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
        if (item == nullptr || !event->mimeData()->hasFormat(ASSET_PATH_MIME))
        {
            event->ignore();
            return;
        }

        auto dstDir = std::filesystem::path{item->data(0, Qt::UserRole).toString().toStdString()};
        auto paths = QString::fromUtf8(event->mimeData()->data(ASSET_PATH_MIME)).split('\n', Qt::SkipEmptyParts);

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
            {
                hit->setSelected(true);
                setCurrentItem(hit);
                emit itemClicked(hit);
            }
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

    static constexpr int SCROLL_STEP = 40;

    void DropContentList::wheelEvent(QWheelEvent* event)
    {
        int delta = event->angleDelta().y();
        verticalScrollBar()->setValue(verticalScrollBar()->value() - (delta > 0 ? SCROLL_STEP : -SCROLL_STEP));
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
        data->setData(ASSET_PATH_MIME, paths.join('\n').toUtf8());

        auto* drag = new QDrag{this};
        drag->setMimeData(data);
        drag->exec(Qt::MoveAction);
    }

    void DropContentList::dragEnterEvent(QDragEnterEvent* event)
    {
        if (event->mimeData()->hasFormat(ASSET_PATH_MIME))
            event->acceptProposedAction();
        else
            event->ignore();
    }

    void DropContentList::dragMoveEvent(QDragMoveEvent* event)
    {
        if (!event->mimeData()->hasFormat(ASSET_PATH_MIME))
        {
            event->ignore();
            return;
        }

        auto* item = itemAt(event->position().toPoint());
        if (item != nullptr && static_cast<AssetType>(item->data(Qt::UserRole + 1).toInt()) == AssetType::FOLDER)
            event->acceptProposedAction();
        else
            event->ignore();
    }

    void DropContentList::dropEvent(QDropEvent* event)
    {
        if (!event->mimeData()->hasFormat(ASSET_PATH_MIME))
        {
            event->ignore();
            return;
        }

        auto* targetItem = itemAt(event->position().toPoint());
        if (targetItem == nullptr ||
            static_cast<AssetType>(targetItem->data(Qt::UserRole + 1).toInt()) != AssetType::FOLDER)
        {
            event->ignore();
            return;
        }

        auto dstDir = std::filesystem::path{targetItem->data(Qt::UserRole).toString().toStdString()};
        auto paths = QString::fromUtf8(event->mimeData()->data(ASSET_PATH_MIME)).split('\n', Qt::SkipEmptyParts);
        for (const auto& p : paths)
        {
            auto src = std::filesystem::path{p.toStdString()};
            if (src.parent_path() != dstDir && src != dstDir)
                m_browser->MoveAsset(src, dstDir);
        }

        event->acceptProposedAction();
    }

    static AssetType ClassifyExtension(const std::string& ext)
    {
        if (ext == ".nmsh")
            return AssetType::MESH;
        if (ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".tga" || ext == ".bmp" || ext == ".hdr")
            return AssetType::TEXTURE;
        if (ext == ".hlsl" || ext == ".glsl" || ext == ".vert" || ext == ".frag")
            return AssetType::SHADER;
        if (ext == ".hscene")
            return AssetType::SCENE;
        if (ext == ".hmat")
            return AssetType::MATERIAL;
        if (ext == ".wav" || ext == ".ogg" || ext == ".flac" || ext == ".mp3")
            return AssetType::AUDIO;
        return AssetType::UNKNOWN;
    }

    static bool IsSupportedExtension(const std::string& ext)
    {
        return ClassifyExtension(ext) != AssetType::UNKNOWN;
    }

    static QColor IconColor(AssetType type)
    {
        switch (type)
        {
        case AssetType::FOLDER:
            return QColor{0xf0, 0xa5, 0x00};
        case AssetType::MESH:
            return QColor{0x4f, 0xc3, 0xf7};
        case AssetType::TEXTURE:
            return QColor{0x81, 0xc7, 0x84};
        case AssetType::SHADER:
            return QColor{0xce, 0x93, 0xd8};
        case AssetType::SCENE:
            return QColor{0xff, 0xb7, 0x4d};
        case AssetType::MATERIAL:
            return QColor{0xef, 0x53, 0x50};
        case AssetType::AUDIO:
            return QColor{0xff, 0xf1, 0x76};
        default:
            return QColor{0x90, 0x90, 0x90};
        }
    }

    static const char* TypeLabel(AssetType type)
    {
        switch (type)
        {
        case AssetType::MESH:
            return "MESH";
        case AssetType::TEXTURE:
            return "TEX";
        case AssetType::SHADER:
            return "SHADER";
        case AssetType::SCENE:
            return "SCENE";
        case AssetType::MATERIAL:
            return "MAT";
        case AssetType::AUDIO:
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

            bool isCut = index.data(Qt::UserRole + 2).toBool();
            if (isCut)
                painter->setOpacity(0.35);

            QRect iconRect = option.rect;
            iconRect.setHeight(iconRect.width());

            auto type = static_cast<AssetType>(index.data(Qt::UserRole + 1).toInt());
            QColor color = IconColor(type);

            if (type == AssetType::FOLDER)
            {
                PaintFolderIcon(*painter, iconRect, color);
            }
            else if (type == AssetType::TEXTURE && m_cache != nullptr)
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

            if (isCut)
            {
                painter->setOpacity(1.0);
                painter->setRenderHint(QPainter::Antialiasing);
                painter->setPen(Qt::NoPen);
                painter->setBrush(QColor{0xef, 0x53, 0x50, 220});
                int badgeSize = qMax(iconRect.width() / 5, 14);
                QRect cutBadge{iconRect.right() - badgeSize - 1, iconRect.top() + 1, badgeSize, badgeSize};
                painter->drawRoundedRect(cutBadge, badgeSize / 2, badgeSize / 2);
                QFont cutFont{"Segoe UI", badgeSize / 3, QFont::Bold};
                painter->setFont(cutFont);
                painter->setPen(Qt::white);
                painter->drawText(cutBadge, Qt::AlignCenter, "\xe2\x9c\x82");
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

    AssetBrowserPanel::AssetBrowserPanel(EditorUndoManager* undoManager, QWidget* parent)
        : QWidget{parent}
        , m_historyBack{forge::GetAllocator()}
        , m_historyForward{forge::GetAllocator()}
    {
        setFocusPolicy(Qt::StrongFocus);
        m_undoManager = undoManager;

        m_thumbnailCache = new ThumbnailCache{this};
        connect(m_thumbnailCache, &ThumbnailCache::ThumbnailLoaded, this, [this] {
            m_contentList->viewport()->update();
        });

        m_refreshDebounce.setSingleShot(true);
        m_refreshDebounce.setInterval(300);
        connect(&m_refreshDebounce, &QTimer::timeout, this, &AssetBrowserPanel::Refresh);

        connect(&m_fsWatcher, &QFileSystemWatcher::directoryChanged, this, &AssetBrowserPanel::OnDirectoryChanged);
        connect(&m_fsWatcher, &QFileSystemWatcher::fileChanged, this, &AssetBrowserPanel::OnFileChanged);

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

        BuildFilterBar();
        mainLayout->addWidget(m_filterBar);

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
        connect(m_contentList, &QListWidget::itemClicked, this, [this](QListWidgetItem* item) {
            auto type = static_cast<AssetType>(item->data(Qt::UserRole + 1).toInt());
            if (type != AssetType::FOLDER)
            {
                emit assetSelected(item->data(Qt::UserRole).toString(), type);
            }
        });
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

    void AssetBrowserPanel::BuildFilterBar()
    {
        m_filterBar = new QWidget{this};
        m_filterBar->setFixedHeight(26);
        m_filterBar->setStyleSheet("QWidget { background: #131313; border-bottom: 1px solid #222; }");

        auto* layout = new QHBoxLayout{m_filterBar};
        layout->setContentsMargins(6, 0, 6, 0);
        layout->setSpacing(2);

        struct FilterDef
        {
            const char* label;
            AssetType type;
            QColor color;
        };

        FilterDef filters[] = {
            {"All", AssetType::UNKNOWN, QColor{0xaa, 0xaa, 0xaa}},
            {"Mesh", AssetType::MESH, QColor{0x4f, 0xc3, 0xf7}},
            {"Tex", AssetType::TEXTURE, QColor{0x81, 0xc7, 0x84}},
            {"Shader", AssetType::SHADER, QColor{0xce, 0x93, 0xd8}},
            {"Scene", AssetType::SCENE, QColor{0xff, 0xb7, 0x4d}},
            {"Mat", AssetType::MATERIAL, QColor{0xef, 0x53, 0x50}},
            {"Audio", AssetType::AUDIO, QColor{0xff, 0xf1, 0x76}},
        };

        for (const auto& f : filters)
        {
            auto* btn = new QToolButton{m_filterBar};
            btn->setText(f.label);
            btn->setCheckable(true);
            btn->setChecked(f.type == m_typeFilter);
            btn->setCursor(Qt::PointingHandCursor);

            QString normalColor = f.color.name();
            QString dimColor = f.color.darker(200).name();
            btn->setStyleSheet(
                QString{"QToolButton {"
                        "  background: transparent; color: %1; border: none; border-radius: 3px;"
                        "  padding: 2px 8px; font-size: 10px; font-family: 'Segoe UI', sans-serif;"
                        "}"
                        "QToolButton:hover { background: #1e1e1e; }"
                        "QToolButton:checked { background: #222; color: %2; font-weight: bold; }"}
                    .arg(dimColor, normalColor));

            AssetType capturedType = f.type;
            connect(btn, &QToolButton::clicked, this, [this, capturedType, btn] {
                SetTypeFilter(capturedType);
                for (auto* child : m_filterBar->findChildren<QToolButton*>())
                {
                    child->setChecked(false);
                }
                btn->setChecked(true);
            });

            layout->addWidget(btn);
        }

        layout->addStretch();

        auto* sortLabel = new QLabel{"Sort:", m_filterBar};
        sortLabel->setStyleSheet("color: #666; font-size: 10px; font-family: 'Segoe UI', sans-serif;");
        layout->addWidget(sortLabel);

        m_sortCombo = new QComboBox{m_filterBar};
        m_sortCombo->addItems({"Name", "Type", "Date"});
        m_sortCombo->setCurrentIndex(static_cast<int>(m_sortMode));
        m_sortCombo->setFixedHeight(20);
        m_sortCombo->setStyleSheet(
            "QComboBox {"
            "  background: #1a1a1a; color: #ccc; border: 1px solid #333; border-radius: 3px;"
            "  padding: 0 6px; font-size: 10px; font-family: 'Segoe UI', sans-serif;"
            "}"
            "QComboBox:focus { border-color: #f0a500; }"
            "QComboBox::drop-down { border: none; width: 14px; }"
            "QComboBox QAbstractItemView {"
            "  background: #1a1a1a; color: #e8e8e8; border: 1px solid #2a2a2a;"
            "  selection-background-color: #3d2e0a; selection-color: #f0a500;"
            "}");
        connect(m_sortCombo, &QComboBox::currentIndexChanged, this,
                [this](int index) { SetSortMode(static_cast<SortMode>(index)); });
        layout->addWidget(m_sortCombo);
    }

    void AssetBrowserPanel::SetTypeFilter(AssetType type)
    {
        m_typeFilter = type;
        PopulateContent();
        UpdateStatusBar();
    }

    void AssetBrowserPanel::SetSortMode(SortMode mode)
    {
        m_sortMode = mode;
        PopulateContent();
        UpdateStatusBar();
    }

    void AssetBrowserPanel::SetAssetsRoot(const char* path)
    {
        m_root = std::filesystem::path{path};
        m_currentDir = m_root;
        m_historyBack.Clear();
        m_historyForward.Clear();
        CleanTrash();
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

        m_undoBtn->setEnabled(m_undoManager->CanUndo());
        m_redoBtn->setEnabled(m_undoManager->CanRedo());
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

        int insertPos = 0;
        int spacerCount = 0;
        for (int i = 0; i < layout->count(); ++i)
        {
            if (layout->itemAt(i)->spacerItem() != nullptr && ++spacerCount == 2)
            {
                insertPos = i;
                break;
            }
        }

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
        WatchCurrentDir();

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
            {
                dirs.PushBack(entry);
            }
            else if (IsSupportedExtension(entry.path().extension().string()))
            {
                if (m_typeFilter == AssetType::UNKNOWN ||
                    ClassifyExtension(entry.path().extension().string()) == m_typeFilter)
                {
                    files.PushBack(entry);
                }
            }
        }

        std::sort(dirs.begin(), dirs.end());

        switch (m_sortMode)
        {
        case SortMode::NAME:
            std::sort(files.begin(), files.end());
            break;
        case SortMode::TYPE:
            std::sort(files.begin(), files.end(), [](const auto& a, const auto& b) {
                auto ta = ClassifyExtension(a.path().extension().string());
                auto tb = ClassifyExtension(b.path().extension().string());
                if (ta != tb)
                    return static_cast<int>(ta) < static_cast<int>(tb);
                return a < b;
            });
            break;
        case SortMode::DATE:
            std::sort(files.begin(), files.end(), [](const auto& a, const auto& b) {
                std::error_code e;
                return a.last_write_time(e) > b.last_write_time(e);
            });
            break;
        }

        if (m_typeFilter == AssetType::UNKNOWN)
        {
            for (const auto& d : dirs)
            {
                auto* item = new QListWidgetItem{};
                item->setText(QString::fromStdString(d.path().filename().string()));
                item->setData(Qt::UserRole, QString::fromStdString(d.path().generic_string()));
                item->setData(Qt::UserRole + 1, static_cast<int>(AssetType::FOLDER));
                item->setToolTip(QString::fromStdString(d.path().string()));
                item->setFlags((item->flags() | Qt::ItemIsDragEnabled) & ~Qt::ItemIsDropEnabled);
                m_contentList->addItem(item);
            }
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
        MarkCutItems();
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
        auto path = item->data(Qt::UserRole).toString();

        if (type == AssetType::FOLDER)
        {
            NavigateTo(std::filesystem::path{path.toStdString()});
            return;
        }

        if (type == AssetType::SCENE)
        {
            emit sceneOpenRequested(path);
            return;
        }

        emit assetOpenRequested(path, type);
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
            menu.addAction("Copy\tCtrl+C", this, &AssetBrowserPanel::CopySelected);
            menu.addAction("Cut\tCtrl+X", this, &AssetBrowserPanel::CutSelected);
            menu.addAction("Duplicate\tCtrl+D", this, &AssetBrowserPanel::DuplicateSelected);
            menu.addSeparator();
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

        if (!m_clipboard.empty())
        {
            menu.addSeparator();
            QString pasteLabel = m_clipboardIsCut
                ? QString{"Paste (Move %1 items)\tCtrl+V"}.arg(m_clipboard.size())
                : QString{"Paste (Copy %1 items)\tCtrl+V"}.arg(m_clipboard.size());
            menu.addAction(pasteLabel, this, &AssetBrowserPanel::PasteClipboard);
        }

        menu.addSeparator();

        auto* undoAction = menu.addAction("Undo\tCtrl+Z", this, &AssetBrowserPanel::UndoFileAction);
        undoAction->setEnabled(m_undoManager->CanUndo());
        auto* redoAction = menu.addAction("Redo\tCtrl+Y", this, &AssetBrowserPanel::RedoFileAction);
        redoAction->setEnabled(m_undoManager->CanRedo());

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

        if (event->key() == Qt::Key_Escape)
        {
            CancelClipboard();
            return;
        }

        if (event->modifiers() & Qt::ControlModifier)
        {
            if (event->key() == Qt::Key_C)
            {
                CopySelected();
                return;
            }
            if (event->key() == Qt::Key_X)
            {
                CutSelected();
                return;
            }
            if (event->key() == Qt::Key_V)
            {
                PasteClipboard();
                return;
            }
            if (event->key() == Qt::Key_D)
            {
                DuplicateSelected();
                return;
            }
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

        m_undoManager->Push(
            [newPath] { std::error_code ec; std::filesystem::remove_all(newPath, ec); },
            [newPath] { std::error_code ec; std::filesystem::create_directory(newPath, ec); });
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

        auto oldHiveid = std::filesystem::path{fsPath}.concat(".hiveid");
        if (std::filesystem::exists(oldHiveid, ec))
        {
            auto newHiveid = std::filesystem::path{newPath}.concat(".hiveid");
            std::filesystem::rename(oldHiveid, newHiveid, ec);
        }

        m_undoManager->Push(
            [newPath, fsPath] { std::error_code ec; std::filesystem::rename(newPath, fsPath, ec); },
            [newPath, fsPath] { std::error_code ec; std::filesystem::rename(fsPath, newPath, ec); });

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

        m_undoManager->Push(
            [fsPath, trashPath] { std::error_code ec; std::filesystem::rename(trashPath, fsPath, ec); },
            [fsPath, trashPath] { std::error_code ec; std::filesystem::rename(fsPath, trashPath, ec); });

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
            {
                m_undoManager->Push(
                    [fsPath, trashPath] { std::error_code e; std::filesystem::rename(trashPath, fsPath, e); },
                    [fsPath, trashPath] { std::error_code e; std::filesystem::rename(fsPath, trashPath, e); });
            }
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

        auto srcHiveid = std::filesystem::path{src}.concat(".hiveid");
        if (std::filesystem::exists(srcHiveid, ec))
        {
            auto dstHiveid = std::filesystem::path{dstPath}.concat(".hiveid");
            std::filesystem::rename(srcHiveid, dstHiveid, ec);
        }

        m_undoManager->Push(
            [dstPath, src] { std::error_code e; std::filesystem::rename(dstPath, src, e); },
            [dstPath, src] { std::error_code e; std::filesystem::rename(src, dstPath, e); });
        Refresh();
    }

    void AssetBrowserPanel::CopySelected()
    {
        m_clipboard.clear();
        m_clipboardIsCut = false;
        for (auto* item : m_contentList->selectedItems())
        {
            auto path = std::filesystem::path{item->data(Qt::UserRole).toString().toStdString()};
            m_clipboard.push_back(path);
        }
        MarkCutItems();
    }

    void AssetBrowserPanel::CutSelected()
    {
        m_clipboard.clear();
        m_clipboardIsCut = true;
        for (auto* item : m_contentList->selectedItems())
        {
            auto path = std::filesystem::path{item->data(Qt::UserRole).toString().toStdString()};
            m_clipboard.push_back(path);
        }
        MarkCutItems();
    }

    static void CopyAssetFile(const std::filesystem::path& src, const std::filesystem::path& dst)
    {
        std::error_code ec;
        if (std::filesystem::is_directory(src))
        {
            std::filesystem::copy(src, dst, std::filesystem::copy_options::recursive, ec);
            return;
        }

        std::filesystem::copy_file(src, dst, std::filesystem::copy_options::skip_existing, ec);

        auto srcHiveid = std::filesystem::path{src}.concat(".hiveid");
        if (std::filesystem::exists(srcHiveid, ec))
        {
            comb::ModuleAllocator tmpAlloc{"TmpHiveId", 4096};
            nectar::HiveIdData hid{};
            if (nectar::ReadHiveId(src.string().c_str(), hid, tmpAlloc.Get()))
            {
                auto dstStr = dst.generic_string();
                uint64_t h = FNV_OFFSET_BASIS;
                for (char c : dstStr)
                {
                    h ^= static_cast<uint64_t>(c);
                    h *= FNV_PRIME;
                }
                auto now = std::chrono::steady_clock::now().time_since_epoch().count();
                nectar::AssetId newId{h, static_cast<uint64_t>(now)};
                nectar::HiveIdData newHid{newId, std::move(hid.m_type)};
                nectar::WriteHiveId(dst.string().c_str(), newHid);
            }
        }
    }

    void AssetBrowserPanel::PasteClipboard()
    {
        if (m_clipboard.empty())
            return;

        std::error_code ec;
        for (const auto& src : m_clipboard)
        {
            if (!std::filesystem::exists(src))
                continue;

            if (m_clipboardIsCut)
            {
                auto dst = m_currentDir / src.filename();
                if (src != dst)
                {
                    std::filesystem::rename(src, dst, ec);
                    if (!ec)
                    {
                        m_undoManager->Push(
                            [dst, src] { std::error_code e; std::filesystem::rename(dst, src, e); },
                            [dst, src] { std::error_code e; std::filesystem::rename(src, dst, e); });
                        auto srcHiveid = std::filesystem::path{src}.concat(".hiveid");
                        if (std::filesystem::exists(srcHiveid, ec))
                        {
                            auto dstHiveid = std::filesystem::path{dst}.concat(".hiveid");
                            std::filesystem::rename(srcHiveid, dstHiveid, ec);
                        }
                    }
                }
            }
            else
            {
                auto dst = m_currentDir / src.filename();
                if (dst == src)
                {
                    auto stem = src.stem().string();
                    auto ext = src.extension().string();
                    for (int i = 1; ; ++i)
                    {
                        dst = m_currentDir / (stem + "_copy" + (i > 1 ? std::to_string(i) : "") + ext);
                        if (!std::filesystem::exists(dst))
                            break;
                    }
                }
                CopyAssetFile(src, dst);
                auto trashPath = TrashPathFor(dst);
                m_undoManager->Push(
                    [dst, trashPath] { std::error_code e; std::filesystem::rename(dst, trashPath, e); },
                    [dst, trashPath] { std::error_code e; std::filesystem::rename(trashPath, dst, e); });
            }
        }

        if (m_clipboardIsCut)
        {
            m_clipboard.clear();
            m_clipboardIsCut = false;
        }

        Refresh();
    }

    void AssetBrowserPanel::DuplicateSelected()
    {
        auto items = m_contentList->selectedItems();
        if (items.isEmpty())
            return;

        std::error_code ec;
        for (auto* item : items)
        {
            auto src = std::filesystem::path{item->data(Qt::UserRole).toString().toStdString()};
            auto stem = src.stem().string();
            auto ext = src.extension().string();
            auto dir = src.parent_path();

            std::filesystem::path dst;
            for (int i = 1; ; ++i)
            {
                dst = dir / (stem + "_copy" + (i > 1 ? std::to_string(i) : "") + ext);
                if (!std::filesystem::exists(dst))
                    break;
            }

            CopyAssetFile(src, dst);
            auto trashPath = TrashPathFor(dst);
            m_undoManager->Push(
                [dst, trashPath] { std::error_code e; std::filesystem::rename(dst, trashPath, e); },
                [dst, trashPath] { std::error_code e; std::filesystem::rename(trashPath, dst, e); });
        }

        Refresh();
    }

    void AssetBrowserPanel::CancelClipboard()
    {
        if (m_clipboard.empty())
            return;
        m_clipboard.clear();
        m_clipboardIsCut = false;
        MarkCutItems();
    }

    void AssetBrowserPanel::MarkCutItems()
    {
        std::unordered_set<std::string> cutPaths;
        if (m_clipboardIsCut)
        {
            for (const auto& p : m_clipboard)
                cutPaths.insert(p.generic_string());
        }

        for (int i = 0; i < m_contentList->count(); ++i)
        {
            auto* item = m_contentList->item(i);
            auto path = item->data(Qt::UserRole).toString().toStdString();
            item->setData(Qt::UserRole + 2, cutPaths.count(path) > 0);
        }

        m_contentList->viewport()->update();
    }

    void AssetBrowserPanel::OnDirectoryChanged(const QString&)
    {
        m_refreshDebounce.start();
    }

    void AssetBrowserPanel::OnFileChanged(const QString& path)
    {
        // QFileSystemWatcher reports native paths (\), cache keys use generic paths (/)
        QString normalized = path;
        normalized.replace('\\', '/');
        m_thumbnailCache->Invalidate(normalized);

        // Qt removes files from the watch list after fileChanged fires
        if (QFile::exists(path))
            m_fsWatcher.addPath(path);

        m_refreshDebounce.start();
    }

    void AssetBrowserPanel::WatchCurrentDir()
    {
        auto watched = m_fsWatcher.directories();
        if (!watched.isEmpty())
            m_fsWatcher.removePaths(watched);
        auto watchedFiles = m_fsWatcher.files();
        if (!watchedFiles.isEmpty())
            m_fsWatcher.removePaths(watchedFiles);

        if (m_currentDir.empty() || !std::filesystem::exists(m_currentDir))
            return;

        m_fsWatcher.addPath(QString::fromStdString(m_currentDir.string()));

        std::error_code ec;
        for (const auto& entry : std::filesystem::directory_iterator(m_currentDir, ec))
        {
            if (entry.is_regular_file() && ClassifyExtension(entry.path().extension().string()) == AssetType::TEXTURE)
                m_fsWatcher.addPath(QString::fromStdString(entry.path().string()));
        }
    }

    void AssetBrowserPanel::CleanTrash()
    {
        auto trashDir = m_root / ".hive-trash";
        if (!std::filesystem::exists(trashDir))
            return;

        std::error_code ec;
        auto now = std::filesystem::file_time_type::clock::now();
        for (const auto& entry : std::filesystem::directory_iterator(trashDir, ec))
        {
            auto age = now - entry.last_write_time(ec);
            if (age > std::chrono::hours{24})
                std::filesystem::remove_all(entry.path(), ec);
        }
    }

    void AssetBrowserPanel::UndoFileAction()
    {
        if (m_undoManager->Undo())
            Refresh();
    }

    void AssetBrowserPanel::RedoFileAction()
    {
        if (m_undoManager->Redo())
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
