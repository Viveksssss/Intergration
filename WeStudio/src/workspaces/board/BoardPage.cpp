#include "BoardPage.h"

#include "BoardWidget.h"
#include "BoardTheme.h"

#include <QPainter>
#include <QScrollArea>
#include <QScrollBar>
#include <QVBoxLayout>

BoardPage::BoardPage(QWidget *parent)
    : QWidget(parent)
{
    setWindowTitle(QStringLiteral("看板拖拽 · Kanban Drag"));
    resize(1280, 820);
    setFont(BoardTheme::uiFont(10));
    setMinimumSize(900, 560);

    m_model = new BoardModel(BoardModel::demo());

    m_board  = new BoardWidget(m_model);
    m_scroll = new QScrollArea;
    m_scroll->setWidget(m_board);
    m_scroll->setWidgetResizable(true);

    // 列宽保持固定，窗口放不下时使用横向滚动，避免最后一列被直接裁掉。
    m_scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    m_scroll->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);

    m_scroll->setFrameShape(QFrame::NoFrame);
    m_scroll->setStyleSheet(QStringLiteral(
        "QScrollArea { background: #11131a; }"
        "QScrollBar:vertical {"
        "  background: transparent; width: 10px; margin: 2px;"
        "}"
        "QScrollBar::handle:vertical {"
        "  background: #34384a; border-radius: 4px; min-height: 40px;"
        "}"
        "QScrollBar::handle:vertical:hover { background: #454a60; }"
        "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {"
        "  height: 0px;"
        "}"
        "QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical {"
        "  background: transparent;"
        "}"));

    auto *lay = new QVBoxLayout(this);
    lay->setContentsMargins(0, 0, 0, 0);
    lay->setSpacing(0);
    lay->addSpacing(52);            // 顶部标题条的高度，画出来的
    lay->addWidget(m_scroll, 1);

    // 视口高度变化时，列要重新铺满
    connect(m_scroll->verticalScrollBar(), &QScrollBar::rangeChanged,
            this, [this] { m_board->relayoutAll(false); });
}

void BoardPage::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.fillRect(rect(), BoardTheme::kBoardBg);
    paintTopBar(p);
}

void BoardPage::paintTopBar(QPainter &p)
{
    const QRect bar(0, 0, width(), 52);

    p.setPen(Qt::NoPen);
    p.setBrush(QColor("#171a23"));
    p.drawRect(bar);

    p.setPen(QPen(QColor("#232733"), 1));
    p.drawLine(0, bar.bottom(), width(), bar.bottom());

    QFont f = BoardTheme::uiFont(1, true);
    f.setPixelSize(16);
    p.setFont(f);
    p.setPen(BoardTheme::kText);
    p.drawText(QRect(24, 0, 400, bar.height()),
               Qt::AlignVCenter | Qt::AlignLeft, QStringLiteral("项目看板"));

    f.setPixelSize(12);
    f.setBold(false);
    p.setFont(f);
    p.setPen(BoardTheme::kTextDim);
    p.drawText(QRect(120, 0, 700, bar.height()),
               Qt::AlignVCenter | Qt::AlignLeft,
               QStringLiteral("按住卡片拖动 → 其它卡片自动让位 → 松手落位"));
}
