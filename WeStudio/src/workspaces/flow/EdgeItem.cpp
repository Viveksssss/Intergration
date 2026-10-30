#include "EdgeItem.h"
#include "NodeItem.h"
#include "PortItem.h"
#include "FlowTheme.h"

#include <QGraphicsSceneHoverEvent>
#include <QPainter>
#include <QPainterPathStroker>

namespace {
constexpr double kHitWidth = 14.0;     // 命中区宽度（屏幕上大致 14 个单位）
constexpr double kMinCtrl  = 60.0;     // 控制点最小外推距离
}

EdgeItem::EdgeItem(PortItem *from, PortItem *to)
    : m_from(from), m_to(to), m_floating(false)
{
    setZValue(-1);                     // 线在节点下面，不会盖住节点内容
    setFlags(ItemIsSelectable);
    setAcceptHoverEvents(true);        // 鼠标移到线附近就高亮
    m_path = buildPath();
}

EdgeItem::EdgeItem(PortItem *anchorPort, const QPointF &mousePos)
    : m_anchor(anchorPort), m_freeEnd(mousePos), m_floating(true)
{
    m_anchorIsOutput = (anchorPort->direction() == PortItem::Output);
    setZValue(1000);                   // 拖拽中的线画在最上面
    setAcceptedMouseButtons(Qt::NoButton);
    m_path = buildPath();
}

// ---------------------------------------------------------------- 曲线
QPainterPath EdgeItem::buildPath() const
{
    QPointF a, b;                      // a 永远是上游，b 永远是下游

    if (m_floating) {
        const QPointF p = m_anchor->anchor();
        if (m_anchorIsOutput) { a = p;          b = m_freeEnd; }
        else                  { a = m_freeEnd;  b = p;         }
    } else {
        a = m_from->anchor();
        b = m_to->anchor();
    }

    // ★ 三次贝塞尔的两个控制点，都沿"水平方向"外推：
    //   a + (dx,0) 让线从输出端口水平地"伸出去"，
    //   b - (dx,0) 让线水平地"插进"输入端口。
    //   这就是所有节点编辑器的标准手感 —— 端口附近永远是水平的。
    //
    //   dx 取两点水平距离的一半，并设一个下限：
    //   当两个端口几乎垂直相对时（水平距离≈0），没有下限就会退化成一条直线。
    const double dx = qMax(kMinCtrl, qAbs(b.x() - a.x()) * 0.5);

    QPainterPath path(a);
    path.cubicTo(a + QPointF(dx, 0.0),
                 b - QPointF(dx, 0.0),
                 b);
    return path;
}

// ---------------------------------------------------------------- 几何
QRectF EdgeItem::boundingRect() const
{
    // 必须把 shape() 的范围也包进来（±kHitWidth/2），再加一点抗锯齿余量。
    // boundingRect 小于 shape 会导致：鼠标明明在线上，却因为"没进脏区"而不重绘。
    return m_path.boundingRect().adjusted(-10, -10, 10, 10);
}

QPainterPath EdgeItem::shape() const
{
    // ★ 整个项目最"想不起来"的一招：
    //   QPainterPathStroker 能给任意路径生成一个"描边后的实心区域"。
    //   曲线本身宽度是 0，contains() 永远返回 false；
    //   用 stroker 加粗成 14 宽以后，就能像判断矩形一样判断"点在不在线上"了。
    QPainterPathStroker stroker;
    stroker.setWidth(kHitWidth);
    stroker.setCapStyle(Qt::RoundCap);
    return stroker.createStroke(m_path);
}

void EdgeItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *, QWidget *)
{
    painter->setRenderHint(QPainter::Antialiasing);

    QColor c = FlowTheme::kEdge;
    if (m_floating)             c = FlowTheme::kEdgeTemp;
    else if (isSelected())      c = FlowTheme::kEdgeHot;
    else if (m_hot)             c = FlowTheme::kEdgeHot;

    const double w = (m_hot || isSelected()) ? 2.4 : 1.8;
    painter->setPen(QPen(c, w, Qt::SolidLine, Qt::RoundCap));
    painter->setBrush(Qt::NoBrush);
    painter->drawPath(m_path);
}

// ---------------------------------------------------------------- 更新
void EdgeItem::updatePath()
{
    // ★ prepareGeometryChange() 必须在改几何之前调用。
    //   它告诉场景"我的 boundingRect 要变了，先把旧的脏区记下来"。
    //   漏掉它的典型症状：拖动节点时，线一边跟着动，一边在原地留下碎片残影。
    prepareGeometryChange();
    m_path = buildPath();
    update();
}

void EdgeItem::setFloatingEnd(const QPointF &p)
{
    prepareGeometryChange();
    m_freeEnd = p;
    m_path = buildPath();
    update();
}

void EdgeItem::finish(PortItem *target)
{
    prepareGeometryChange();
    if (m_anchorIsOutput) {
        m_from = m_anchor;
        m_to   = target;
    } else {
        m_from = target;
        m_to   = m_anchor;
    }
    m_floating = false;
    setZValue(-1);
    m_path = buildPath();
    update();
}

void EdgeItem::setHot(bool hot)
{
    if (m_hot == hot) return;
    m_hot = hot;
    update();
}

// ---------------------------------------------------------------- 悬停
void EdgeItem::hoverEnterEvent(QGraphicsSceneHoverEvent *event)
{
    setHot(true);
    QGraphicsItem::hoverEnterEvent(event);
}

void EdgeItem::hoverLeaveEvent(QGraphicsSceneHoverEvent *event)
{
    setHot(false);
    QGraphicsItem::hoverLeaveEvent(event);
}

// ---------------------------------------------------------------- 邻居
NodeItem *EdgeItem::fromNode() const
{
    return m_from ? m_from->owner() : nullptr;
}

NodeItem *EdgeItem::toNode() const
{
    return m_to ? m_to->owner() : nullptr;
}
