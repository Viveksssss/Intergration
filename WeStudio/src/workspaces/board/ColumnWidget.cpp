#include "ColumnWidget.h"

#include "BoardModel.h"
#include "CardWidget.h"
#include "BoardTheme.h"

#include <QEasingCurve>
#include <QDragEnterEvent>
#include <QDragLeaveEvent>
#include <QDragMoveEvent>
#include <QDropEvent>
#include <QPainter>
#include <QPainterPath>

namespace {

// 全工程只用一条缓动曲线，观感才统一
qreal ease(qreal t)
{
    const QEasingCurve c(QEasingCurve::OutCubic);
    return c.valueForProgress(t);
}

}  // namespace

ColumnWidget::ColumnWidget(int index, BoardModel *model, QWidget *parent)
    : QWidget(parent)
    , m_index(index)
    , m_model(model)
{
    // ★ 不打开这个，dragEnterEvent 永远不会被调用，整列就不认拖放
    setAcceptDrops(true);
    setAttribute(Qt::WA_Hover, true);

    m_slotAnim = new QTimer(this);
    m_slotAnim->setTimerType(Qt::PreciseTimer);
    m_slotAnim->setInterval(15);
    connect(m_slotAnim, &QTimer::timeout, this, [this] {
        const qreal t = m_slotDurMs > 0
            ? qBound(0.0, qreal(m_slotClock.elapsed()) / qreal(m_slotDurMs), 1.0)
            : 1.0;
        m_slotDrawY = m_slotAnimFrom + (m_slotAnimTo - m_slotAnimFrom) * ease(t);
        update();
        if (t >= 1.0)
            m_slotAnim->stop();
    });
}

// ===========================================================================
//  几何：全列只有一个算法
//
//  插槽 i 的 y = 顶部 + i × (卡高 + 间距)。
//  第 i 张卡片的 y 就是 slotY(i)，占位符若要插到第 i 个位置，也落在 slotY(i)。
//  **卡片位置和占位符位置用同一个函数算**，两者不可能错位。
// ===========================================================================
int ColumnWidget::slotTop() const
{
    return BoardTheme::kHeaderHeight + 6;
}

int ColumnWidget::slotY(int i) const
{
    return slotTop() + i * (BoardTheme::kCardHeight + BoardTheme::kCardGap);
}

int ColumnWidget::positionFromY(int y) const
{
    // 一张卡片占 (卡高 + 间距)。第 i 格的"中线"在 slotY(i) + 卡高/2。
    // 落点越过中线就该插到下一格 —— 于是"拖到卡片下半部分"= 插到它后面。
    const qreal unit = BoardTheme::kCardHeight + BoardTheme::kCardGap;
    const qreal rel  = y - slotTop();
    const int idx = int((rel + (unit - BoardTheme::kCardHeight) / 2.0) / unit);
    return qBound(0, idx, int(m_order.size()));
}

// ===========================================================================
//  布局
// ===========================================================================
void ColumnWidget::rebuildCards()
{
    // 删掉模型里已经没有的卡片
    //
    // ★ 必须先 hide() 再 deleteLater()。
    //   deleteLater() 只是**登记**，真正的 delete 要等事件循环回到最外层才执行。
    //   rebuildCards 紧接着就会 relayout → setGeometry 重排剩下的卡片，
    //   而这张已登记删除的旧卡此刻还活着、还 visible、还占着它原来的 (x, y)，
    //   父控件重画时它照样把自己画出来 —— 屏幕上就出现"重影/悬空的卡"。
    //   hide() 立刻生效，先让它从画面上消失，再慢慢删。
    for (int id : m_cards.keys()) {
        if (!m_model->card(id)) {
            CardWidget *w = m_cards.take(id);
            w->hide();
            w->deleteLater();
        }
    }

    const Column &col = m_model->columns()[m_index];
    m_order.clear();

    for (const Card &c : col.cards) {
        CardWidget *w = m_cards.value(c.id, nullptr);
        if (!w) {
            w = new CardWidget(c.id, this);
            connect(w, &CardWidget::pressed, this,
                    [this](int id, const QPoint &off) { emit cardPressed(id, off); });
            m_cards.insert(c.id, w);
            w->show();
        }
        w->setData(c.title, c.tag, c.tagColor, c.footer);
        m_order.append(c.id);
    }

    relayout(false);
}

void ColumnWidget::relayout(bool animate)
{
    // ★ 第一次就在这儿把拖拽态收干净。
    //
    //   rebuildCards() 开头会删掉"模型里已经没有的卡片"，而被拖走的那张
    //   恰好就是这类卡（它已经搬到别的列了）—— m_cards 里没有它了，
    //   但 m_dragCard / m_slotFrom 还留着旧值。
    //
    //   这时候还进让位分支，就是拿一个**已经不存在于本列**的卡片去算行号：
    //   draggedRowInThisColumn() 返回 -1（模型里它不在本列了），
    //   于是"跳过被拖的卡"这一条永远不触发，而 m_slotFrom 仍然是旧插槽号
    //   → 剩下的卡片全部被往下推一格，最底下那个槽位空着，看起来像
    //   "落列之后所有卡片都没归位"。
    //
    //   判据：本列没有正在被拖的卡，就说明拖拽已经结束（或从来没开始）。
    //   注意判据要**两条都成立**：
    //     · m_dragCard >= 0   —— 我确实处在拖拽态
    //     · 本列已经查不到它   —— 它已经搬到别的列了
    //   只判后一条会把正在进行的跨列拖拽也误杀：
    //   拖 col0 的卡悬停在 col2 上时，col2 里本来就查不到这张卡
    //   （模型里它还在 col0），一判就会把 col2 的占位符收回去。
    if (m_dragCard >= 0 && draggedRowInThisColumn() < 0)
        endDrag();
    // ★ 让位的核心：排位置的时候要"假装被拖的那张卡不在列表里"。
    //
    //   否则会这样：m_order = [A, B, C]，拖 A 到 B 和 C 之间（m_slotFrom = 1），
    //   按 m_order 排，B 拿到第 1 格，而占位符也在第 1 格 —— 两者重叠，
    //   画面上是"半透明蓝框底下透出一张卡"，完全看不出"这里空出来了"。
    //
    //   所以：跳过被拖的卡（它自己的虚线框由它自己的 paintEvent 画在原位），
    //   其余卡片依次往上挤；一旦走过占位槽，就整体往下错开一格。
    const int dragRow = draggedRowInThisColumn();

    int row = 0;                                  // 视觉行号：不含被拖的卡
    for (int i = 0; i < m_order.size(); ++i) {
        CardWidget *w = m_cards.value(m_order[i], nullptr);
        if (!w)
            continue;

        if (i == dragRow) {
            // 被拖的卡：本体隐藏（paintEvent 画虚线框），
            // 位置保持在它自己原来的格子里，不参与让位。
            const int cw0 = width() - BoardTheme::kColumnPad * 2;
            if (cw0 != w->width() || w->height() != BoardTheme::kCardHeight)
                w->resize(cw0, BoardTheme::kCardHeight);
            w->moveTo(QPoint(BoardTheme::kColumnPad, slotY(i)), animate);
            continue;
        }

        // 占位符之前的卡不动；之后的卡统一往下让一格
        int target = row;
        if (m_slotFrom >= 0 && row >= m_slotFrom)
            target = row + 1;

        const int cw = width() - BoardTheme::kColumnPad * 2;
        if (cw != w->width() || w->height() != BoardTheme::kCardHeight)
            w->resize(cw, BoardTheme::kCardHeight);
        w->moveTo(QPoint(BoardTheme::kColumnPad, slotY(target)), animate);
        ++row;
    }

    // 占位符也要动画过去，否则它会在两格之间"瞬移"
    if (m_slotFrom >= 0) {
        const qreal to = slotY(m_slotFrom);
        if (qAbs(to - m_slotDrawY) > 0.5) {
            m_slotAnimFrom = m_slotDrawY;
            m_slotAnimTo   = to;
            m_slotClock.restart();
            m_slotDurMs = 140;
            m_slotAnim->start();
        }
    }
    update();
}

void ColumnWidget::resizeEvent(QResizeEvent *e)
{
    QWidget::resizeEvent(e);
    relayout(false);
}

// ===========================================================================
//  拖拽
// ===========================================================================
int ColumnWidget::draggedRowInThisColumn() const
{
    if (m_dragCard < 0)
        return -1;
    int col = -1, row = -1;
    if (!m_model->locate(m_dragCard, &col, &row) || col != m_index)
        return -1;
    return row;
}

void ColumnWidget::beginDrag(int cardId)
{
    m_target = DropTarget();

    // ★ 只有**源列**才认这张卡。
    //
    //   BoardWidget::startDrag 会对每一列都调 beginDrag(cardId)，
    //   但 cardId 只可能在这一列里。如果不管三七二十一都写
    //   m_dragCard = cardId，其余三列就进入了一种坏状态：
    //     · m_dragCard >= 0     → 以为自己正在拖
    //     · m_order.indexOf()   → -1，本列根本没有这张卡
    //     · m_slotFrom 也就成了 -1，占位符没地方放
    //   之后这张卡一旦**真的落进**这一列，rebuildCards 会把它加进 m_order，
    //   可 m_dragCard 还是它 —— 于是 relayout 里 i == dragRow 那一支
    //   永远命中不了（模型里它已经在本列，行号变了），
    //   同时 m_slotFrom 又是个陈旧的数 → 整列卡片被错开一格。
    //
    //   所以：本列没有这张卡，就只记 m_dragCard = -1，什么都不做。
    //   （m_dragCard 仍然要清成 -1：上一轮拖拽的残留必须在这里断干净。）
    CardWidget *w = m_cards.value(cardId, nullptr);
    if (!w) {
        m_dragCard = -1;
        m_slotFrom = -1;
        update();
        return;
    }

    m_dragCard = cardId;

    // 卡片本体隐藏：屏幕上"那张卡"由 QDrag 的幽灵图代表。
    // 但它的**插槽**还在 —— 所以一开始占位符就落在它自己的位置上，
    // 用户看到的是"卡片变成了一个虚线框"，视觉上最连续。
    w->setGrabbed(true);
    m_slotFrom  = m_order.indexOf(cardId);
    m_slotDrawY = slotY(m_slotFrom);
    update();
}

void ColumnWidget::setDropTarget(const DropTarget &t)
{
    m_target = t;

    if (!t.isValid() || t.col != m_index) {
        if (m_slotFrom >= 0) {
            m_slotFrom = -1;                 // 落点不在这列，让开的地方收回去
            relayout(true);
        }
        update();
        return;
    }

    // t.index 是"在完整列表里插到第几个位置"。要换算成
    // "删掉被拖卡片之后那份列表里"的插槽号，否则同列内往后拖会多让一格。
    int slot = t.index;
    const int dr = draggedRowInThisColumn();
    if (dr >= 0 && dr < t.index)
        --slot;

    if (slot != m_slotFrom) {
        m_slotFrom = slot;
        relayout(true);                      // ★ 卡片让位就发生在这里
    }
    update();
}

void ColumnWidget::endDrag()
{
    if (CardWidget *w = m_cards.value(m_dragCard, nullptr))
        w->setGrabbed(false);
    m_dragCard = -1;
    m_slotFrom = -1;
    m_target   = DropTarget();

    // ★ 这里必须 animate = false。
    //
    //   落下的那一刻，卡片已经在从"旧位置"滑向"新位置"的半路上
    //   （rebuildCards 里那次 relayout 起的是让位动画）。如果我们再
    //   起一段动画，起点是"当前显示位置"，终点是归位位置 —— 看起来没问题，
    //   但上一步的目标本身就是错的：rebuildCards 是在拖拽态里跑的，
    //   它在算让位；等拖拽态收掉，那个目标已经作废。
    //   两段动画叠在一起，卡片会先冲到一个错的位置再折返。
    //
    //   拖拽结束本来就是一个"咔"的落定时刻，不需要过渡。
    //   直接跳到最终位置，既不会折返，也不会让"动画还在跑"这个状态
    //   残留到下一次拖拽里。
    relayout(false);
}

DropTarget ColumnWidget::hitTest(const QPoint &pos) const
{
    return DropTarget{DropTarget::Kind::Place, m_index, positionFromY(pos.y())};
}

void ColumnWidget::dragEnterEvent(QDragEnterEvent *e)
{
    if (!e->mimeData()->hasFormat(QString::fromLatin1(kCardMimeType))) {
        e->ignore();
        return;
    }
    m_hovered = true;
    e->acceptProposedAction();
    emit dragEntered(m_index);
    update();
}

void ColumnWidget::dragMoveEvent(QDragMoveEvent *e)
{
    if (!e->mimeData()->hasFormat(QString::fromLatin1(kCardMimeType))) {
        e->ignore();
        return;
    }
    // 拖拽时每移动一点就来一次，这里必须很便宜：
    // hitTest 只有几次除法 + 一次 qBound，不遍历子控件。
    //
    // 顺带把鼠标的 y 报上去 —— 父级要靠它判断"是否进了边缘感应区"，
    // 好决定边缘自动滚动的方向。列自己不知道 QScrollArea 存在。
    const QPoint p = e->position().toPoint();
    emit hoverChanged(m_index, hitTest(p), p.y());
    e->acceptProposedAction();
}

void ColumnWidget::dragLeaveEvent(QDragLeaveEvent *e)
{
    Q_UNUSED(e);
    m_hovered = false;
    emit dragExited(m_index);
    update();
}

void ColumnWidget::dropEvent(QDropEvent *e)
{
    if (!e->mimeData()->hasFormat(QString::fromLatin1(kCardMimeType))) {
        e->ignore();
        return;
    }

    const auto *mime = static_cast<const CardMimeData *>(e->mimeData());

    // 落点用**松手这一刻**重新算，而不是用最近一次 dragMove 缓存的值。
    // 鼠标快速甩动时最后一帧可能跳过了 dragMove，缓存会差一格。
    const DropTarget t = hitTest(e->position().toPoint());

    m_hovered = false;
    e->setDropAction(Qt::MoveAction);
    e->accept();

    emit cardDropped(mime->cardId(), m_index, t.index);
}

void ColumnWidget::enterEvent(QEnterEvent *e)
{
    Q_UNUSED(e);
    update();
}

void ColumnWidget::leaveEvent(QEvent *e)
{
    Q_UNUSED(e);
    update();
}

// ===========================================================================
//  绘制
// ===========================================================================
void ColumnWidget::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);

    const QRectF body = QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5);

    p.setPen(QPen(BoardTheme::kColumnBorder, 1));
    p.setBrush(m_hovered ? BoardTheme::kColumnBgHot : BoardTheme::kColumnBg);
    p.drawRoundedRect(body, BoardTheme::kColumnRadius, BoardTheme::kColumnRadius);

    paintHeader(p);

    if (m_slotFrom >= 0)
        paintSlot(p);
}

void ColumnWidget::paintHeader(QPainter &p)
{
    const QRect head(0, 0, width(), BoardTheme::kHeaderHeight);
    const Column &col = m_model->columns()[m_index];

    // 列头带一点浅色底，而且只让上半部分圆角
    p.save();
    QPainterPath clip;
    clip.addRoundedRect(QRectF(rect()), BoardTheme::kColumnRadius, BoardTheme::kColumnRadius);
    p.setClipPath(clip);
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(255, 255, 255, 8));
    p.drawRect(head);
    p.restore();

    p.setBrush(col.accent);
    p.setPen(Qt::NoPen);
    p.drawEllipse(QPointF(BoardTheme::kColumnPad + 5, BoardTheme::kHeaderHeight / 2.0), 4, 4);

    QFont f = BoardTheme::uiFont(1, true);
    f.setPixelSize(13);
    p.setFont(f);
    p.setPen(BoardTheme::kText);
    const QFontMetrics fm(f);
    const int tw = fm.horizontalAdvance(col.title);
    p.drawText(QRect(BoardTheme::kColumnPad + 16, 0, tw + 4, BoardTheme::kHeaderHeight),
               Qt::AlignVCenter | Qt::AlignLeft, col.title);

    // 计数徽标
    const QString n = QString::number(m_order.size());
    const int bw = qMax(18, fm.horizontalAdvance(n) + 12);
    const QRect badge(width() - BoardTheme::kColumnPad - bw,
                      (BoardTheme::kHeaderHeight - 18) / 2, bw, 18);
    p.setPen(Qt::NoPen);
    p.setBrush(BoardTheme::kCountBg);
    p.drawRoundedRect(badge, 9, 9);
    p.setPen(BoardTheme::kTextDim);
    p.drawText(badge, Qt::AlignCenter, n);
}

void ColumnWidget::paintSlot(QPainter &p)
{
    const QRectF r(BoardTheme::kColumnPad, m_slotDrawY,
                   width() - BoardTheme::kColumnPad * 2, BoardTheme::kCardHeight);

    QPen pen(BoardTheme::kTagBlue, 1.5);
    pen.setStyle(Qt::DashLine);
    p.setPen(pen);
    p.setBrush(QColor(46, 168, 255, 26));
    p.drawRoundedRect(r, BoardTheme::kCardRadius, BoardTheme::kCardRadius);
}

// ===========================================================================
//  自检
// ===========================================================================
QString ColumnWidget::debugSnapshot() const
{
    QString s = QStringLiteral("col%1[").arg(m_index);
    for (int i = 0; i < m_order.size(); ++i)
        s += (i ? "," : "") + QString::number(m_order[i]);
    s += QStringLiteral("] slot=%1").arg(m_slotFrom);
    s += QStringLiteral(" target=") + m_target.toString();

    // 前两格的目标 y，用来核对"让位"算得对不对
    for (int i = 0; i < qMin(2, m_order.size()); ++i) {
        if (CardWidget *w = m_cards.value(m_order[i], nullptr))
            s += QStringLiteral(" y%1=%2").arg(i).arg(w->targetPos().y());
    }
    return s;
}
