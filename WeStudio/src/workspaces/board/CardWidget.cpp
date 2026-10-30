#include "CardWidget.h"

#include "BoardTheme.h"

#include <QEasingCurve>
#include <QEnterEvent>
#include <QLineF>
#include <QMouseEvent>
#include <QPainter>
#include <QPixmap>

namespace {

constexpr int kStartDragDistance = 6;    // 超过这个距离才算"拖"，否则当成点击

// 在给定宽度内折行，最多两行，超出部分用省略号。
// Qt 自己只会单行 elidedText，两行要手写。
QStringList wrapUpToTwoLines(const QFontMetrics &fm, const QString &text, int width)
{
    QStringList out;
    QString line;
    for (int i = 0; i < text.size(); ++i) {
        const QString next = line + text.at(i);
        if (fm.horizontalAdvance(next) > width && !line.isEmpty()) {
            out << line;
            line.clear();
            if (out.size() == 2) {
                // 第二行也满了：剩下的全塞进第二行并省略
                out[1] = fm.elidedText(text.mid(i), Qt::ElideRight, width);
                return out;
            }
            line = QString(text.at(i));
        } else {
            line = next;
        }
    }
    if (!line.isEmpty())
        out << line;
    return out;
}

}  // namespace

CardWidget::CardWidget(int id, QWidget *parent)
    : QWidget(parent)
    , m_id(id)
{
    setCursor(Qt::OpenHandCursor);
    setAttribute(Qt::WA_Hover, true);       // 打开 enterEvent / leaveEvent
    setFixedHeight(BoardTheme::kCardHeight);
    setMouseTracking(true);
}

void CardWidget::setData(const QString &title, const QString &tag,
                         const QColor &tagColor, const QString &footer)
{
    m_title    = title;
    m_tag      = tag;
    m_tagColor = tagColor;
    m_footer   = footer;
    update();
}

// ===========================================================================
//  位置与动画
// ===========================================================================
void CardWidget::moveTo(const QPoint &pos, bool animate)
{
    m_target = pos;

    // ★ 关键：先把 m_draw 对齐到控件**真实**的位置。
    //   外部（列的 relayout）可能用 setGeometry 直接改过位置，
    //   此时 m_draw 还停在旧值上；若拿旧 m_draw 去和 pos 比，
    //   会误判成"已经在目标位置"而直接 return，卡片就永远不动了。
    m_draw = QPointF(geometry().topLeft());

    if (!animate) {
        jumpTo(pos);
        return;
    }
    if (qRound(m_draw.x()) == pos.x() && qRound(m_draw.y()) == pos.y())
        return;                                    // 真的已经在目标位置

    // 起点取「当前显示位置」，不是「上一个目标」。
    // 用户连续快速拖动时上一段动画可能只走了一半，从当前位置接上才连贯，
    // 否则卡片会先跳回上一个目标再出发，看起来像抽搐。
    m_animFrom = m_draw;
    m_animTo   = pos;
    m_clock.restart();

    if (!m_anim) {
        m_anim = new QTimer(this);
        m_anim->setTimerType(Qt::PreciseTimer);
        m_anim->setInterval(15);                   // ≈ 66fps
        connect(m_anim, &QTimer::timeout, this, &CardWidget::stepAnimation);
    }

    // 距离越远动画越长，但有上下限：远处的卡不能慢到像卡住，
    // 近处的卡也不能快得像瞬移。
    const int dist = qRound(QLineF(m_animFrom, QPointF(pos)).length());
    m_durMs = qBound(90, 110 + dist * 2, 260);

    m_anim->start();
}

void CardWidget::jumpTo(const QPoint &pos)
{
    m_target = pos;
    m_animFrom = pos;
    m_draw = pos;
    if (m_anim)
        m_anim->stop();
    setGeometry(QRect(pos, size()));
    if (parentWidget())
        parentWidget()->update();
}

void CardWidget::setGrabbed(bool on)
{
    m_grabbed = on;
    update();
}

void CardWidget::finishAnimation()
{
    jumpTo(m_target);
}

void CardWidget::stepAnimation()
{
    const qreal t = m_durMs > 0
        ? qBound(0.0, qreal(m_clock.elapsed()) / qreal(m_durMs), 1.0)
        : 1.0;

    // OutCubic：先快后慢，观感最接近"被轻轻推到位"
    const QEasingCurve curve(QEasingCurve::OutCubic);
    const qreal e = curve.valueForProgress(t);

    m_draw = m_animFrom + (QPointF(m_animTo) - m_animFrom) * e;
    setGeometry(qRound(m_draw.x()), qRound(m_draw.y()), width(), height());

    // 位置一变，旧位置会留下"残影"，让父控件把这块脏区重画
    if (parentWidget())
        parentWidget()->update();

    if (t >= 1.0) {
        m_anim->stop();
        m_draw = m_target;
    }
}

// ===========================================================================
//  绘制
// ===========================================================================
void CardWidget::paintEvent(QPaintEvent *)
{
    // 被拖走的那张卡：本体不画（屏幕上由 QDrag 的幽灵图代表它），
    // 但要留一个虚线框，告诉用户"松手会落回这里"。
    if (m_grabbed) {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing, true);
        QPen pen(BoardTheme::kTagBlue, 1.5);
        pen.setStyle(Qt::DashLine);
        p.setPen(pen);
        p.setBrush(BoardTheme::kSlotBg);
        p.drawRoundedRect(rect().adjusted(0, 0, -1, -1),
                          BoardTheme::kCardRadius, BoardTheme::kCardRadius);
        return;
    }

    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);

    const QRectF r = QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5);

    p.setPen(QPen(m_hover ? BoardTheme::kTagBlue.lighter(140) : BoardTheme::kCardBorder, 1));
    p.setBrush(m_hover ? BoardTheme::kCardBgHover : BoardTheme::kCardBg);
    p.drawRoundedRect(r, BoardTheme::kCardRadius, BoardTheme::kCardRadius);

    const int pad = 12;

    // ---- 标签胶囊
    if (!m_tag.isEmpty()) {
        QFont tf = BoardTheme::uiFont(1, true);
        tf.setPixelSize(11);
        p.setFont(tf);
        const QFontMetrics fm(tf);
        const int tw = fm.horizontalAdvance(m_tag) + 14;
        const int th = fm.height() + 2;
        m_tagRect = QRect(pad, pad, tw, th);

        QColor bg = m_tagColor;
        bg.setAlpha(46);
        p.setPen(Qt::NoPen);
        p.setBrush(bg);
        p.drawRoundedRect(m_tagRect, th / 2.0, th / 2.0);

        p.setPen(m_tagColor);
        p.drawText(m_tagRect, Qt::AlignCenter, m_tag);
    } else {
        m_tagRect = QRect(pad, pad, 0, 0);
    }

    // ---- 标题（最多两行）
    QFont titleFont = BoardTheme::uiFont(1, false);
    titleFont.setPixelSize(14);
    p.setFont(titleFont);
    p.setPen(BoardTheme::kText);

    const int titleTop = m_tag.isEmpty() ? pad : m_tagRect.bottom() + 7;
    const QFontMetrics fm(titleFont);
    const QStringList lines = wrapUpToTwoLines(fm, m_title, width() - pad * 2);
    int y = titleTop;
    for (const QString &ln : lines) {
        p.drawText(QRect(pad, y, width() - pad * 2, fm.height()),
                   Qt::AlignLeft | Qt::AlignVCenter, ln);
        y += fm.height();
    }

    // ---- 底部小字
    if (!m_footer.isEmpty()) {
        QFont ff = BoardTheme::uiFont(1, false);
        ff.setPixelSize(11);
        p.setFont(ff);
        p.setPen(BoardTheme::kTextDim);
        p.drawText(QRect(pad, height() - 22, width() - pad * 2, 16),
                   Qt::AlignLeft | Qt::AlignVCenter, m_footer);
    }
}

// ===========================================================================
//  鼠标：按下只记位置，移动超过阈值才通知列控件去发起 QDrag
// ===========================================================================
void CardWidget::mousePressEvent(QMouseEvent *e)
{
    if (e->button() != Qt::LeftButton)
        return;
    m_mouseDown  = true;
    m_pressGlobal = e->globalPosition().toPoint();
    e->accept();
}

void CardWidget::mouseMoveEvent(QMouseEvent *e)
{
    if (!m_mouseDown || m_grabbed)
        return;

    // 用**全局**坐标算距离。卡片在拖拽一开始就会被隐藏，
    // 局部坐标从此没有意义；全局坐标是唯一稳定的参照物。
    const QPoint now = e->globalPosition().toPoint();
    if ((now - m_pressGlobal).manhattanLength() < kStartDragDistance)
        return;

    m_mouseDown = false;
    // grabOffset = 按下那一点相对卡片左上角。幽灵图靠它对齐，
    // 否则不管按在卡片哪个位置，幽灵图左上角都会跳到鼠标上。
    emit pressed(m_id, m_pressGlobal - mapToGlobal(QPoint(0, 0)));
}

void CardWidget::mouseReleaseEvent(QMouseEvent *e)
{
    Q_UNUSED(e);
    if (!m_mouseDown)
        return;
    m_mouseDown = false;
    emit released(m_id);
}

void CardWidget::enterEvent(QEnterEvent *e)
{
    Q_UNUSED(e);
    m_hover = true;
    update();
}

void CardWidget::leaveEvent(QEvent *e)
{
    Q_UNUSED(e);
    m_hover = false;
    update();
}

// ===========================================================================
//  离屏渲染一张卡片（幽灵图）
//
//  为什么不直接 grab() 自己？
//    · grab() 拿到的是当前可见状态，卡片正在动画时会截到半个位置
//    · 幽灵图要加"被拿起来"的效果（投影），grab() 做不到
// ===========================================================================
QPixmap CardWidget::renderPixmap(const QString &title, const QString &tag,
                                 const QColor &tagColor, const QString &footer,
                                 int width, qreal dpr)
{
    const int h = BoardTheme::kCardHeight;
    QPixmap pm(qRound(width * dpr), qRound(h * dpr));
    pm.setDevicePixelRatio(dpr);
    pm.fill(Qt::transparent);

    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setRenderHint(QPainter::TextAntialiasing, true);

    const QRectF r(0.5, 0.5, width - 1.0, h - 1.0);

    // 投影：三层越来越淡的圆角矩形
    for (int i = 3; i >= 1; --i) {
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(0, 0, 0, 12 - i));
        p.drawRoundedRect(r.adjusted(-i, -i + 3, i, i + 3),
                          BoardTheme::kCardRadius + i, BoardTheme::kCardRadius + i);
    }

    p.setPen(QPen(BoardTheme::kCardBorder, 1));
    p.setBrush(BoardTheme::kCardBg);
    p.drawRoundedRect(r, BoardTheme::kCardRadius, BoardTheme::kCardRadius);

    const int pad = 12;
    QRect tagRect(pad, pad, 0, 0);

    if (!tag.isEmpty()) {
        QFont tf = BoardTheme::uiFont(1, true);
        tf.setPixelSize(11);
        p.setFont(tf);
        const QFontMetrics fm(tf);
        tagRect = QRect(pad, pad, fm.horizontalAdvance(tag) + 14, fm.height() + 2);

        QColor bg = tagColor;
        bg.setAlpha(46);
        p.setPen(Qt::NoPen);
        p.setBrush(bg);
        p.drawRoundedRect(tagRect, tagRect.height() / 2.0, tagRect.height() / 2.0);
        p.setPen(tagColor);
        p.drawText(tagRect, Qt::AlignCenter, tag);
    }

    QFont titleFont = BoardTheme::uiFont(1, false);
    titleFont.setPixelSize(14);
    p.setFont(titleFont);
    p.setPen(BoardTheme::kText);

    const QFontMetrics tfm(titleFont);
    const QStringList lines = wrapUpToTwoLines(tfm, title, width - pad * 2);
    int y = tag.isEmpty() ? pad : tagRect.bottom() + 7;
    for (const QString &ln : lines) {
        p.drawText(QRect(pad, y, width - pad * 2, tfm.height()),
                   Qt::AlignLeft | Qt::AlignVCenter, ln);
        y += tfm.height();
    }

    if (!footer.isEmpty()) {
        QFont ff = BoardTheme::uiFont(1, false);
        ff.setPixelSize(11);
        p.setFont(ff);
        p.setPen(BoardTheme::kTextDim);
        p.drawText(QRect(pad, h - 22, width - pad * 2, 16),
                   Qt::AlignLeft | Qt::AlignVCenter, footer);
    }

    p.end();
    return pm;
}
