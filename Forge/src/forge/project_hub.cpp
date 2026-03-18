#include <forge/project_hub.h>

#include <QFileDialog>
#include <QGraphicsDropShadowEffect>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QStackedWidget>
#include <QStyle>
#include <QVBoxLayout>

namespace forge
{
    static const char* kHubStyleSheet = R"(
        QWidget#hubRoot {
            background-color: #0d0d0d;
        }
        QWidget#sidebar {
            background-color: #141414;
            border-right: 1px solid #2a2a2a;
        }
        QLabel#brandTitle {
            color: #f0a500;
            font-size: 22px;
            font-weight: bold;
            letter-spacing: 2px;
        }
        QLabel#brandSub {
            color: #666;
            font-size: 11px;
        }
        QPushButton#sidebarBtn {
            background: transparent;
            color: #999;
            border: none;
            border-radius: 6px;
            padding: 10px 18px;
            text-align: left;
            font-size: 13px;
        }
        QPushButton#sidebarBtn:hover {
            background-color: #1e1e1e;
            color: #ddd;
        }
        QPushButton#sidebarBtn[active="true"] {
            background-color: #f0a500;
            color: #0d0d0d;
            font-weight: bold;
        }
        QWidget#projectCard {
            background-color: #1a1a1a;
            border: 1px solid #2a2a2a;
            border-radius: 8px;
        }
        QWidget#projectCard:hover {
            border-color: #f0a500;
            background-color: #1e1e1e;
        }
        QLabel#cardName {
            color: #e8e8e8;
            font-size: 15px;
            font-weight: bold;
        }
        QLabel#cardVersion {
            color: #888;
            font-size: 11px;
        }
        QLabel#cardPath {
            color: #555;
            font-size: 10px;
        }
        QLabel#cardBadge {
            background-color: #f0a500;
            color: #0d0d0d;
            border-radius: 3px;
            padding: 2px 8px;
            font-size: 10px;
            font-weight: bold;
        }
        QLabel#sectionTitle {
            color: #e0e0e0;
            font-size: 18px;
            font-weight: bold;
        }
        QLabel#sectionSub {
            color: #666;
            font-size: 12px;
        }
        QPushButton#accentBtn {
            background-color: #f0a500;
            color: #0d0d0d;
            border: none;
            border-radius: 6px;
            padding: 10px 28px;
            font-size: 13px;
            font-weight: bold;
        }
        QPushButton#accentBtn:hover {
            background-color: #ffb820;
        }
        QPushButton#accentBtn:pressed {
            background-color: #d09000;
        }
        QPushButton#ghostBtn {
            background: transparent;
            color: #999;
            border: 1px solid #333;
            border-radius: 6px;
            padding: 10px 28px;
            font-size: 13px;
        }
        QPushButton#ghostBtn:hover {
            border-color: #f0a500;
            color: #f0a500;
        }
        QLineEdit#formField {
            background-color: #1a1a1a;
            color: #ddd;
            border: 1px solid #333;
            border-radius: 6px;
            padding: 8px 12px;
            font-size: 13px;
        }
        QLineEdit#formField:focus {
            border-color: #f0a500;
        }
        QLabel#fieldLabel {
            color: #888;
            font-size: 12px;
        }
        QLabel#statusError {
            color: #e05252;
            font-size: 12px;
        }
        QLabel#statusOk {
            color: #4caf50;
            font-size: 12px;
        }
        QScrollArea {
            background: transparent;
            border: none;
        }
        QScrollBar:vertical {
            background: #141414;
            width: 8px;
            border-radius: 4px;
        }
        QScrollBar::handle:vertical {
            background: #333;
            border-radius: 4px;
            min-height: 30px;
        }
        QScrollBar::handle:vertical:hover {
            background: #f0a500;
        }
        QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {
            height: 0;
        }
    )";

    ProjectHub::ProjectHub(QWidget* parent)
        : QWidget{parent}
    {
        setObjectName("hubRoot");
        BuildUi();
    }

    void ProjectHub::SetProjects(const std::vector<DiscoveredProject>& projects)
    {
        m_projects = projects;
        BuildProjectCards();
    }

    void ProjectHub::BuildUi()
    {
        setStyleSheet(kHubStyleSheet);

        auto* root = new QHBoxLayout{this};
        root->setContentsMargins(0, 0, 0, 0);
        root->setSpacing(0);

        // ---- Sidebar ----
        auto* sidebar = new QWidget;
        sidebar->setObjectName("sidebar");
        sidebar->setFixedWidth(220);
        auto* sidebarLayout = new QVBoxLayout{sidebar};
        sidebarLayout->setContentsMargins(20, 28, 20, 20);
        sidebarLayout->setSpacing(4);

        auto* brandTitle = new QLabel{"HIVE ENGINE"};
        brandTitle->setObjectName("brandTitle");
        sidebarLayout->addWidget(brandTitle);

        auto* brandSub = new QLabel{"Project Hub"};
        brandSub->setObjectName("brandSub");
        sidebarLayout->addWidget(brandSub);

        sidebarLayout->addSpacing(32);

        auto* btnProjects = new QPushButton{"Projects"};
        btnProjects->setObjectName("sidebarBtn");
        btnProjects->setProperty("active", true);
        sidebarLayout->addWidget(btnProjects);

        auto* btnCreate = new QPushButton{"New Project"};
        btnCreate->setObjectName("sidebarBtn");
        sidebarLayout->addWidget(btnCreate);

        sidebarLayout->addStretch();

        auto* versionLabel = new QLabel{"v0.1.0"};
        versionLabel->setObjectName("brandSub");
        sidebarLayout->addWidget(versionLabel);

        root->addWidget(sidebar);

        // ---- Content area (stacked) ----
        m_stack = new QStackedWidget;
        root->addWidget(m_stack, 1);

        // Page 0: Projects list
        auto* projectsPage = new QWidget;
        auto* projectsLayout = new QVBoxLayout{projectsPage};
        projectsLayout->setContentsMargins(40, 36, 40, 20);
        projectsLayout->setSpacing(12);

        auto* headerRow = new QHBoxLayout;
        auto* titleLabel = new QLabel{"My Projects"};
        titleLabel->setObjectName("sectionTitle");
        headerRow->addWidget(titleLabel);
        headerRow->addStretch();

        auto* browseBtn = new QPushButton{"Browse..."};
        browseBtn->setObjectName("ghostBtn");
        headerRow->addWidget(browseBtn);

        auto* openBtn = new QPushButton{"Open Project"};
        openBtn->setObjectName("accentBtn");
        headerRow->addWidget(openBtn);

        projectsLayout->addLayout(headerRow);

        auto* subtitleLabel = new QLabel{"Select a project to open, or create a new one."};
        subtitleLabel->setObjectName("sectionSub");
        projectsLayout->addWidget(subtitleLabel);

        projectsLayout->addSpacing(8);

        auto* scrollArea = new QScrollArea;
        scrollArea->setWidgetResizable(true);
        scrollArea->setFrameShape(QFrame::NoFrame);

        m_projectList = new QWidget;
        m_cardLayout = new QVBoxLayout{m_projectList};
        m_cardLayout->setContentsMargins(0, 0, 12, 0);
        m_cardLayout->setSpacing(8);
        m_cardLayout->addStretch();

        scrollArea->setWidget(m_projectList);
        projectsLayout->addWidget(scrollArea, 1);

        m_statusLabel = new QLabel;
        m_statusLabel->setObjectName("statusError");
        m_statusLabel->hide();
        projectsLayout->addWidget(m_statusLabel);

        m_stack->addWidget(projectsPage);

        // Page 1: Create project
        auto* createPage = new QWidget;
        auto* createLayout = new QVBoxLayout{createPage};
        createLayout->setContentsMargins(40, 36, 40, 20);
        createLayout->setSpacing(16);

        auto* createTitle = new QLabel{"Create New Project"};
        createTitle->setObjectName("sectionTitle");
        createLayout->addWidget(createTitle);

        auto* createSub = new QLabel{"Configure your new Hive Engine project."};
        createSub->setObjectName("sectionSub");
        createLayout->addWidget(createSub);

        createLayout->addSpacing(12);

        auto* nameLabel = new QLabel{"Project Name"};
        nameLabel->setObjectName("fieldLabel");
        createLayout->addWidget(nameLabel);

        m_createName = new QLineEdit{"MyGame"};
        m_createName->setObjectName("formField");
        m_createName->setMaximumWidth(500);
        createLayout->addWidget(m_createName);

        auto* dirLabel = new QLabel{"Location"};
        dirLabel->setObjectName("fieldLabel");
        createLayout->addWidget(dirLabel);

        auto* dirRow = new QHBoxLayout;
        dirRow->setSpacing(8);
        m_createDir = new QLineEdit;
        m_createDir->setObjectName("formField");
        m_createDir->setPlaceholderText("Select a directory...");
        dirRow->addWidget(m_createDir, 1);

        auto* dirBrowse = new QPushButton{"Browse"};
        dirBrowse->setStyleSheet("QPushButton { background: transparent; color: #999; border: 1px solid #444; "
                                 "border-radius: 6px; padding: 8px 20px; font-size: 13px; }"
                                 "QPushButton:hover { border-color: #f0a500; color: #f0a500; }");
        dirRow->addWidget(dirBrowse);
        dirRow->addStretch();
        createLayout->addLayout(dirRow);

        auto* verLabel = new QLabel{"Version"};
        verLabel->setObjectName("fieldLabel");
        createLayout->addWidget(verLabel);

        m_createVersion = new QLineEdit{"0.1.0"};
        m_createVersion->setObjectName("formField");
        m_createVersion->setMaximumWidth(200);
        createLayout->addWidget(m_createVersion);

        createLayout->addSpacing(16);

        auto* createBtnRow = new QHBoxLayout;
        auto* createBtn = new QPushButton{"Create Project"};
        createBtn->setObjectName("accentBtn");
        createBtnRow->addWidget(createBtn);
        createBtnRow->addStretch();
        createLayout->addLayout(createBtnRow);

        createLayout->addStretch();

        m_stack->addWidget(createPage);

        // ---- Connections ----
        connect(btnProjects, &QPushButton::clicked, this, [this, btnProjects, btnCreate] {
            m_stack->setCurrentIndex(0);
            btnProjects->setProperty("active", true);
            btnCreate->setProperty("active", false);
            btnProjects->style()->unpolish(btnProjects);
            btnProjects->style()->polish(btnProjects);
            btnCreate->style()->unpolish(btnCreate);
            btnCreate->style()->polish(btnCreate);
        });

        connect(btnCreate, &QPushButton::clicked, this, [this, btnProjects, btnCreate] {
            m_stack->setCurrentIndex(1);
            btnProjects->setProperty("active", false);
            btnCreate->setProperty("active", true);
            btnProjects->style()->unpolish(btnProjects);
            btnProjects->style()->polish(btnProjects);
            btnCreate->style()->unpolish(btnCreate);
            btnCreate->style()->polish(btnCreate);
        });

        connect(browseBtn, &QPushButton::clicked, this, &ProjectHub::browseProjectRequested);

        connect(openBtn, &QPushButton::clicked, this, [this] {
            // Open selected project from first card if any
            if (!m_projects.empty())
                emit projectSelected(QString::fromStdString(m_projects.front().m_path));
        });

        connect(createBtn, &QPushButton::clicked, this, [this] {
            emit createProjectRequested(m_createName->text(), m_createDir->text(), m_createVersion->text());
        });

        connect(dirBrowse, &QPushButton::clicked, this, [this] {
            QString dir = QFileDialog::getExistingDirectory(this, "Select Project Location", m_createDir->text());
            if (!dir.isEmpty())
                m_createDir->setText(dir);
        });
    }

    void ProjectHub::BuildProjectCards()
    {
        // Clear existing cards
        while (m_cardLayout->count() > 1)
        {
            auto* item = m_cardLayout->takeAt(0);
            if (item->widget())
                delete item->widget();
            delete item;
        }

        for (const auto& project : m_projects)
        {
            auto* card = new QWidget;
            card->setObjectName("projectCard");
            card->setFixedHeight(80);
            card->setCursor(Qt::PointingHandCursor);

            auto* cardLayout = new QHBoxLayout{card};
            cardLayout->setContentsMargins(20, 14, 20, 14);
            cardLayout->setSpacing(12);

            // Hex icon
            auto* icon = new QLabel{QString::fromUtf8("\u2B22")};
            icon->setStyleSheet("color: #f0a500; font-size: 28px;");
            icon->setFixedWidth(36);
            icon->setAlignment(Qt::AlignCenter);
            cardLayout->addWidget(icon);

            // Info column
            auto* infoLayout = new QVBoxLayout;
            infoLayout->setSpacing(2);

            auto* nameRow = new QHBoxLayout;
            auto* nameLabel = new QLabel{QString::fromStdString(project.m_name)};
            nameLabel->setObjectName("cardName");
            nameRow->addWidget(nameLabel);

            if (project.m_isCurrentDirectory)
            {
                auto* badge = new QLabel{"CWD"};
                badge->setObjectName("cardBadge");
                nameRow->addWidget(badge);
            }

            nameRow->addStretch();
            infoLayout->addLayout(nameRow);

            auto* pathLabel = new QLabel{QString::fromStdString(project.m_path)};
            pathLabel->setObjectName("cardPath");
            pathLabel->setTextInteractionFlags(Qt::NoTextInteraction);
            infoLayout->addWidget(pathLabel);

            cardLayout->addLayout(infoLayout, 1);

            auto* versionLabel = new QLabel{QString::fromStdString(project.m_version)};
            versionLabel->setObjectName("cardVersion");
            cardLayout->addWidget(versionLabel);

            // Click to open
            QString path = QString::fromStdString(project.m_path);
            card->installEventFilter(this);
            connect(card, &QWidget::destroyed, this, [] {});

            // Use a mouse press event via a transparent button overlay
            auto* clickBtn = new QPushButton{card};
            clickBtn->setStyleSheet("background: transparent; border: none;");
            clickBtn->setGeometry(0, 0, 9999, 9999);
            connect(clickBtn, &QPushButton::clicked, this, [this, path] { emit projectSelected(path); });

            m_cardLayout->insertWidget(m_cardLayout->count() - 1, card);
        }
    }
} // namespace forge
