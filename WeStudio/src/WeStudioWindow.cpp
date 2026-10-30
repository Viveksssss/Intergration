#include "WeStudioWindow.h"

#include "workspaces/board/BoardPage.h"
#include "workspaces/code/CodePage.h"
#include "workspaces/collab/CollabPage.h"
#include "workspaces/flow/FlowPage.h"
#include "workspaces/shot/ShotPage.h"
#include "workspaces/table/TablePage.h"
#include "workspaces/timeline/TimelinePage.h"

#include <QButtonGroup>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QShortcut>
#include <QStackedWidget>
#include <QVBoxLayout>

WeStudioWindow::WeStudioWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setObjectName(QStringLiteral("weStudioShell"));
    setWindowTitle(QStringLiteral("WeStudio"));
    setMinimumSize(1120, 720);
    resize(1440, 880);
    buildUi();
}

void WeStudioWindow::buildUi()
{
    setStyleSheet(QStringLiteral(R"(
        QMainWindow#weStudioShell, QWidget#weStudioRoot {
            background: #0d0f14;
            color: #e7eaf2;
        }
        QFrame#sideBar {
            background: #151821;
            border-right: 1px solid #252a36;
        }
        QLabel#wordmark {
            color: #f6f7fb;
            font-size: 20px;
            font-weight: 700;
        }
        QLabel#wordmarkMark {
            color: #091017;
            background: #62d2a2;
            border-radius: 8px;
            font-size: 16px;
            font-weight: 800;
        }
        QLabel#eyebrow {
            color: #71798c;
            font-size: 10px;
            font-weight: 700;
            letter-spacing: 1px;
        }
        QLabel#workspaceCard {
            color: #cdd4e4;
            background: #1c202a;
            border: 1px solid #2a3040;
            border-radius: 9px;
            padding: 10px 12px;
        }
        QPushButton[nav="true"] {
            color: #949caf;
            background: transparent;
            border: none;
            border-radius: 8px;
            padding: 11px 12px;
            text-align: left;
            font-size: 13px;
        }
        QPushButton[nav="true"]:hover {
            color: #e8edf7;
            background: #1d222d;
        }
        QPushButton[nav="true"]:checked {
            color: #f4f7fb;
            background: #26303a;
            border-left: 3px solid #62d2a2;
            padding-left: 9px;
        }
        QFrame#contentHeader {
            background: #10131a;
            border-bottom: 1px solid #252a36;
        }
        QLabel#pageTitle {
            color: #f3f5f9;
            font-size: 20px;
            font-weight: 700;
        }
        QLabel#pageSubtitle {
            color: #7f8799;
            font-size: 12px;
        }
        QLabel#pageIndex {
            color: #62d2a2;
            background: #17251f;
            border: 1px solid #294738;
            border-radius: 12px;
            padding: 4px 10px;
            font-size: 10px;
            font-weight: 700;
        }
        QLabel#sideFooter {
            color: #626a7c;
            font-size: 10px;
        }
    )"));

    auto *root = new QWidget(this);
    root->setObjectName(QStringLiteral("weStudioRoot"));
    auto *rootLayout = new QHBoxLayout(root);
    rootLayout->setContentsMargins(0, 0, 0, 0);
    rootLayout->setSpacing(0);

    auto *sidebar = new QFrame(root);
    sidebar->setObjectName(QStringLiteral("sideBar"));
    sidebar->setFixedWidth(226);
    auto *sideLayout = new QVBoxLayout(sidebar);
    sideLayout->setContentsMargins(16, 18, 16, 16);
    sideLayout->setSpacing(8);

    auto *brandRow = new QHBoxLayout;
    brandRow->setSpacing(10);
    auto *mark = new QLabel(QStringLiteral("W"), sidebar);
    mark->setObjectName(QStringLiteral("wordmarkMark"));
    mark->setAlignment(Qt::AlignCenter);
    mark->setFixedSize(36, 36);
    auto *wordmark = new QLabel(QStringLiteral("WeStudio"), sidebar);
    wordmark->setObjectName(QStringLiteral("wordmark"));
    brandRow->addWidget(mark);
    brandRow->addWidget(wordmark);
    brandRow->addStretch(1);
    sideLayout->addLayout(brandRow);

    auto *tagline = new QLabel(QStringLiteral("PROJECT WORKSPACE"), sidebar);
    tagline->setObjectName(QStringLiteral("eyebrow"));
    sideLayout->addWidget(tagline);

    auto *card = new QLabel(
        QStringLiteral("当前项目\nWeStudio Desktop"), sidebar);
    card->setObjectName(QStringLiteral("workspaceCard"));
    sideLayout->addWidget(card);
    sideLayout->addSpacing(10);

    auto *navLabel = new QLabel(QStringLiteral("工作区"), sidebar);
    navLabel->setObjectName(QStringLiteral("eyebrow"));
    sideLayout->addWidget(navLabel);

    m_navLayout = new QVBoxLayout;
    m_navLayout->setContentsMargins(0, 0, 0, 0);
    m_navLayout->setSpacing(5);
    sideLayout->addLayout(m_navLayout);

    m_navGroup = new QButtonGroup(this);
    m_navGroup->setExclusive(true);

    auto *content = new QWidget(root);
    auto *contentLayout = new QVBoxLayout(content);
    contentLayout->setContentsMargins(0, 0, 0, 0);
    contentLayout->setSpacing(0);

    auto *header = new QFrame(content);
    header->setObjectName(QStringLiteral("contentHeader"));
    header->setFixedHeight(78);
    auto *headerLayout = new QHBoxLayout(header);
    headerLayout->setContentsMargins(24, 12, 24, 12);

    auto *titleColumn = new QVBoxLayout;
    titleColumn->setSpacing(3);
    m_pageTitle = new QLabel(header);
    m_pageTitle->setObjectName(QStringLiteral("pageTitle"));
    m_pageSubtitle = new QLabel(header);
    m_pageSubtitle->setObjectName(QStringLiteral("pageSubtitle"));
    titleColumn->addWidget(m_pageTitle);
    titleColumn->addWidget(m_pageSubtitle);
    headerLayout->addLayout(titleColumn);
    headerLayout->addStretch(1);
    m_pageIndex = new QLabel(header);
    m_pageIndex->setObjectName(QStringLiteral("pageIndex"));
    headerLayout->addWidget(m_pageIndex);

    m_stack = new QStackedWidget(content);
    contentLayout->addWidget(header);
    contentLayout->addWidget(m_stack, 1);

    rootLayout->addWidget(sidebar);
    rootLayout->addWidget(content, 1);
    setCentralWidget(root);

    addPage(QStringLiteral("board"), QStringLiteral("01   项目看板"),
            QStringLiteral("项目看板"),
            QStringLiteral("拖动卡片安排进度，卡片会在列之间自动让位。"),
            new BoardPage);
    addPage(QStringLiteral("flow"), QStringLiteral("02   流程设计"),
            QStringLiteral("流程设计"),
            QStringLiteral("拖拽节点与端口整理方案，不合法的连接会被拦截。"),
            new FlowPage);
    addPage(QStringLiteral("table"), QStringLiteral("03   数据视图"),
            QStringLiteral("数据视图"),
            QStringLiteral("虚拟化表格只处理当前视口，可定位和过滤百万行数据。"),
            new TablePage);
    addPage(QStringLiteral("code"), QStringLiteral("04   代码编辑"),
            QStringLiteral("代码编辑"),
            QStringLiteral("打开源码查看行号、括号匹配、语法高亮和折叠结果。"),
            new CodePage);
    auto *shotPage = new ShotPage;
    addPage(QStringLiteral("shot"), QStringLiteral("05   截图批注"),
            QStringLiteral("截图批注"),
            QStringLiteral("框选屏幕后直接添加箭头、文字、画笔和马赛克。"),
            shotPage);
    connect(shotPage, &ShotPage::shotStarted, this, [this, shotPage] {
        m_stack->setCurrentWidget(shotPage);
    });
    addPage(QStringLiteral("timeline"), QStringLiteral("06   媒体审阅"),
            QStringLiteral("媒体审阅"),
            QStringLiteral("打开视频，在时间轴上框选片段并定位播放位置。"),
            new TimelinePage);
    addPage(QStringLiteral("collab"), QStringLiteral("07   协作房间"),
            QStringLiteral("协作房间"),
            QStringLiteral("通过局域网同步白板与聊天，并在断线后补回遗漏内容。"),
            new CollabPage);

    sideLayout->addStretch(1);
    auto *quick = new QLabel(
        QStringLiteral("Ctrl+1 … Ctrl+7 切换工作区\nCtrl+Shift+A 开始截图"),
        sidebar);
    quick->setObjectName(QStringLiteral("sideFooter"));
    quick->setWordWrap(true);
    sideLayout->addWidget(quick);
    auto *footer = new QLabel(
        QStringLiteral("Qt 6.9  ·  C++17  ·  LOCAL"),
        sidebar);
    footer->setObjectName(QStringLiteral("sideFooter"));
    sideLayout->addWidget(footer);

    connect(m_navGroup, &QButtonGroup::idClicked,
            this, &WeStudioWindow::setCurrentPage);

    for (int i = 0; i < m_pages.size(); ++i) {
        auto *shortcut = new QShortcut(
            QKeySequence(QStringLiteral("Ctrl+%1").arg(i + 1)), this);
        shortcut->setContext(Qt::ApplicationShortcut);
        connect(shortcut, &QShortcut::activated, this, [this, i] {
            setCurrentPage(i);
        });
    }

    setCurrentPage(0);
}

void WeStudioWindow::addPage(const QString &key, const QString &navText,
                             const QString &title, const QString &subtitle,
                             QWidget *page)
{
    const int index = m_pages.size();
    auto *button = new QPushButton(navText, this);
    button->setProperty("nav", true);
    button->setCheckable(true);
    button->setCursor(Qt::PointingHandCursor);
    m_navGroup->addButton(button, index);

    m_navLayout->addWidget(button);

    page->setParent(m_stack);
    m_stack->addWidget(page);
    m_pages.append({key, title, subtitle, button, page});
}

void WeStudioWindow::setCurrentPage(int index)
{
    if (index < 0 || index >= m_pages.size())
        return;

    const PageInfo &page = m_pages.at(index);
    m_stack->setCurrentIndex(index);
    page.button->setChecked(true);
    m_pageTitle->setText(page.title);
    m_pageSubtitle->setText(page.subtitle);
    m_pageIndex->setText(
        QStringLiteral("WORKSPACE %1 / %2")
            .arg(index + 1, 2, 10, QLatin1Char('0'))
            .arg(m_pages.size(), 2, 10, QLatin1Char('0')));
}

bool WeStudioWindow::showPage(const QString &key)
{
    for (int i = 0; i < m_pages.size(); ++i) {
        if (m_pages.at(i).key == key) {
            setCurrentPage(i);
            return true;
        }
    }
    return false;
}
