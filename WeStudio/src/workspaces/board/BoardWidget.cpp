#include "BoardWidget.h"

#include "BoardModel.h"
#include "CardWidget.h"
#include "ColumnWidget.h"
#include "BoardTheme.h"

#include <QCursor>
#include <QDrag>
#include <QPainter>
#include <QScrollArea>
#include <QScrollBar>
#include <QTimer>

BoardWidget::BoardWidget(BoardModel *model, QWidget *parent)
    : QWidget(parent)
    , m_model(model)
{
    m_scrollTimer = new QTimer(this);
    m_scrollTimer->setInterval(16);          // ≈ 60fps，比 QScrollBar 自带步进细腻
    connect(m_scrollTimer, &QTimer::timeout, this, [this] {
        if (!m_scrollStep)
            return;
        if (QScrollArea *sa = scrollArea())
            sa->verticalScrollBar()->setValue(
                sa->verticalScrollBar()->value() + m_scrollStep);
    });

    rebuild();
}

QScrollArea *BoardWidget::scrollArea() const
{
    // ★ 坑：QScrollArea::setWidget() 会把 widget 塞进**它自己的 viewport**，
    //    所以 parentWidget() 是 viewport（一个普通 QWidget），不是 QScrollArea。
    //    直接 qobject_cast<QScrollArea*>(parentWidget()) 永远返回 nullptr。
    //    正解：看父级的父级。
    return qobject_cast<QScrollArea *>(parentWidget() ? parentWidget()->parentWidget()
                                                      : nullptr);
}

// ===========================================================================
//  构建 / 布局
// ===========================================================================
void BoardWidget::rebuild()
{
    const int n = m_model->columns().size();

    while (m_cols.size() > n) {
        ColumnWidget *c = m_cols.takeLast();
        c->hide();
        c->deleteLater();
    }

    while (m_cols.size() < n) {
        const int idx = m_cols.size();
        auto *col = new ColumnWidget(idx, m_model, this);

        connect(col, &ColumnWidget::cardPressed, this,
                [this](int id, const QPoint &off) { startDrag(id, off); });
        connect(col, &ColumnWidget::dragEntered, this,
                [this](int i) { m_hoverCol = i; });
        connect(col, &ColumnWidget::dragExited, this, [this](int i) {
            if (m_hoverCol == i) {
                m_hoverCol = -1;
                applyEdgeProbe(-1);           // 拖出列就别再滚了
            }
        });
        connect(col, &ColumnWidget::hoverChanged, this,
                [this](int i, const DropTarget &t, int y) {
                    onHoverChanged(i, t);
                    applyEdgeProbe(y);            // 同一次 dragMove 顺手判边缘
                });
        connect(col, &ColumnWidget::cardDropped, this,
                [this](int id, int c, int i) { applyDrop(id, c, i); });
        // 滚轮不在这里处理：ColumnWidget::wheelEvent 里 e->ignore() 之后，
        // 事件会自动冒泡到父级（BoardWidget → QScrollArea），由 QScrollArea
        // 自己滚。手写转发反而容易造出重复滚动的 bug。

        m_cols.append(col);
        col->show();
    }

    for (ColumnWidget *c : m_cols)
        c->rebuildCards();

    relayoutAll(false);
}

void BoardWidget::refreshHeights()
{
    // 卡片增删后列的内容高度变了，QScrollArea 的滚动范围要跟着变。
    relayoutAll(false);
}

void BoardWidget::relayoutAll(bool animate)
{
    const int n = m_cols.size();
    const int totalW = 24 * 2 + n * BoardTheme::kColumnWidth
                     + qMax(0, n - 1) * BoardTheme::kColumnGap;

    int h = height();
    if (QScrollArea *sa = scrollArea())
        h = sa->viewport()->height();

    // ★ 列高不能永远等于视口高。
    //   卡片多了会溢出列底，溢出部分必须"存在"（列变高 → 内容变高 →
    //   QScrollArea 出现滚动条），否则边缘自动滚动就是个摆设：
    //   鼠标拖到列底时下面根本没有可滚的内容。
    //   所以取两者的较大值。
    int maxContent = 0;
    for (ColumnWidget *c : m_cols)
        maxContent = qMax(maxContent, c->contentHeight());

    const int colH = qMax(qMax(200, h - 32), maxContent);
    setMinimumSize(totalW, colH + 32);

    int x = 24;
    for (ColumnWidget *c : m_cols) {
        c->setGeometry(x, 16, BoardTheme::kColumnWidth, colH);
        x += BoardTheme::kColumnWidth + BoardTheme::kColumnGap;
    }
    Q_UNUSED(animate);
}

void BoardWidget::resizeEvent(QResizeEvent *e)
{
    QWidget::resizeEvent(e);
    relayoutAll(false);
}

void BoardWidget::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.fillRect(rect(), BoardTheme::kBoardBg);
}

// ===========================================================================
//  发起拖拽
//
//  ★ 全工程最反直觉的一点：QDrag::exec() 是**阻塞**的 ——
//    它内部跑自己的事件循环，直到用户松手才返回。
//    所以"松手之后干什么"必须写在 exec() 之后，而不是写在某个信号里。
// ===========================================================================
void BoardWidget::startDrag(int cardId, const QPoint &grabOffset)
{
    int fromCol = -1, fromRow = -1;
    if (!m_model->locate(cardId, &fromCol, &fromRow))
        return;

    m_dragging = true;
    m_dragCard = cardId;
    m_hoverCol = fromCol;

    // 1) 所有列进入"拖拽态"，被拖的卡本体隐藏
    for (ColumnWidget *col : m_cols)
        col->beginDrag(cardId);

    // 2) 初始落点 = 卡片原来所在的位置，占位符立刻出现在它自己的槽里
    onHoverChanged(fromCol, DropTarget{DropTarget::Kind::Place, fromCol, fromRow});

    // 3) 幽灵图：从卡片子控件身上读数据，不直接读模型 ——
    //    模型里的那张卡在拖拽期间被"藏"起来了，但界面上的样子
    //    还是以控件为准（将来卡片上加了进度条、头像，这里自动跟上）。
    CardWidget *src = m_cols.value(fromCol) ? m_cols[fromCol]->cardWidget(cardId)
                                            : nullptr;
    const int w = BoardTheme::kColumnWidth - BoardTheme::kColumnPad * 2;
    const QPixmap pix = src
        ? CardWidget::renderPixmap(src->titleText(), src->tagText(),
                                   src->tagColor(), src->footerText(),
                                   w, devicePixelRatioF())
        : QPixmap();

    auto *drag = new CardDrag(this, cardId, grabOffset,
                              QSize(w, BoardTheme::kCardHeight));
    drag->setMimeData(new CardMimeData(cardId));
    drag->setPixmap(pix);
    drag->setHotSpot(grabOffset);

    // 4) 阻塞执行，直到松手
    drag->exec(Qt::MoveAction, Qt::MoveAction);

    // 5) 收尾。注意：不管放没放到列上，都走这里
    clearDragState();
}

void BoardWidget::clearDragState()
{
    // ★ 必须**遍历所有列**，不是只收 hover 那列。
    //
    //   一次拖拽会在好几列上留下状态：源列留下了 m_dragCard 和
    //   一开始的占位符（就在卡片自己那儿），途经的列留下过临时占位符。
    //   只清当前 hover 的那一列，其余列的 m_slotFrom 就会一直挂着，
    //   下一次 rebuildCards → relayout 时它们还按"有人在拖"排版：
    //   整列卡片被往下错开一格，最底下空一格 —— 看起来像"落列之后
    //   卡片没归位"，其实是上一轮的幽灵插槽。
    for (ColumnWidget *col : m_cols)
        col->endDrag();

    m_dragging  = false;
    m_dragCard  = -1;
    m_hoverCol  = -1;
    m_scrollStep = 0;
    m_scrollTimer->stop();
}

// ===========================================================================
//  落点广播
// ===========================================================================
void BoardWidget::onHoverChanged(int col, const DropTarget &t)
{
    // 全局只有一个落点：只有鼠标所在列能显示占位符，
    // 其它列必须把让开的地方收回去。这就是"跨列拖动时旧列自动复原"。
    for (int i = 0; i < m_cols.size(); ++i) {
        if (i == col)
            m_cols[i]->setDropTarget(t);
        else
            m_cols[i]->setDropTarget(DropTarget());
    }
}

void BoardWidget::beginDragProbe(int cardId)
{
    int col = -1;
    if (!m_model->locate(cardId, &col, nullptr))
        return;
    m_dragging = true;
    m_dragCard = cardId;
    m_hoverCol = col;
    for (ColumnWidget *c : m_cols)
        c->beginDrag(cardId);
}

void BoardWidget::applyHover(int col, const DropTarget &t)
{
    onHoverChanged(col, t);
}

void BoardWidget::applyDrop(int cardId, int toCol, int toIndex)
{
    const bool moved = m_model->moveCard(cardId, toCol, toIndex);

    // 数据变了 → 所有列按新数据重建
    for (ColumnWidget *col : m_cols)
        col->rebuildCards();

    refreshHeights();          // 卡片变多/变少，滚动范围可能变了

    if (moved)
        emit modelChanged();
}

// ===========================================================================
//  边缘自动滚动
//
//  ★ 关键认知：这里不能用 dragMoveEvent 驱动。
//    鼠标拖到列的顶部边缘后如果**停住不动**，dragMoveEvent 就再也不来了 ——
//    而"停下来等它继续滚"恰恰是最常见的操作。
//    所以只能：dragMove 时算出"该不该滚、往哪滚"存下来，
//    再用一个 16ms 的定时器自己推 scrollBar。
// ===========================================================================
int BoardWidget::scrollValue() const
{
    if (QScrollArea *sa = scrollArea())
        return sa->verticalScrollBar()->value();
    return 0;
}

void BoardWidget::applyEdgeProbe(int yInColumn)
{
    // 只有拖拽进行中才滚
    if (!m_dragging) {
        m_scrollStep = 0;
        m_scrollTimer->stop();
        return;
    }

    ColumnWidget *col = m_cols.value(m_hoverCol, nullptr);
    int step = 0;

    if (col && yInColumn >= 0) {
        const int zone = BoardTheme::kEdgeScrollY;
        const int h    = col->height();

        // 越靠近边缘，滚得越快：线性插值 3 → 22 px/帧。
        // 匀速滚会让人在长列表里疯狂等待。
        auto speedAt = [](int depth, int zonePx) {
            const qreal t = qBound(0.0, qreal(depth) / qreal(zonePx), 1.0);
            return int(3 + t * 19);
        };

        if (yInColumn < zone)
            step = -speedAt(zone - yInColumn, zone);           // 上边缘 → 负
        else if (yInColumn > h - zone)
            step =  speedAt(yInColumn - (h - zone), zone);     // 下边缘 → 正
    }

    m_scrollStep = step;
    if (step)
        m_scrollTimer->start();
    else
        m_scrollTimer->stop();
}

QString BoardWidget::debugSnapshot() const
{
    QString s;
    s += QStringLiteral("dragging=%1 card=%2 hover=%3 | ")
             .arg(m_dragging ? 1 : 0).arg(m_dragCard).arg(m_hoverCol);
    for (int i = 0; i < m_cols.size(); ++i) {
        if (i)
            s += QStringLiteral(" | ");
        s += m_cols[i]->debugSnapshot();
    }
    return s;
}
