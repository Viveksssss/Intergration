#include "PortItem.h"
#include "EdgeItem.h"
#include "NodeItem.h"
#include "FlowTheme.h"

#include <QPainter>

namespace {
constexpr double kVisibleR = 5.0;   // 看得见的圆半径
constexpr double kHitR     = 9.0;   // 能点中的半径（比看见的大，好点）
}

PortItem::PortItem(NodeItem *owner, Direction dir, PortType type,
                   const QString &label, int slot)
    : m_owner(owner), m_dir(dir), m_type(type), m_label(label), m_slot(slot)
{
    // ★ 这一行不能少：m_owner 只是个普通指针，setParentItem 才是真正的父子关系。
    //   没有它，setPos() 会被当成"场景绝对坐标"，所有端口会堆在画布原点附近，
    //   节点移动时端口也不会跟着走。
    setParentItem(owner);

    // 端口自己不接收鼠标事件：点端口的按下/移动/松开统一交给 NodeScene 处理，
    // 那里的状态机更清楚（要区分"点端口拉线"和"点节点拖动"）。
    setAcceptedMouseButtons(Qt::NoButton);
    setFlag(ItemIsSelectable, false);
    setZValue(2);                       // 压在节点上面，免得被节点边缘盖住
}

QRectF PortItem::boundingRect() const
{
    // 外扩 2px 给抗锯齿留余量
    return QRectF(-kVisibleR - 2, -kVisibleR - 2,
                  (kVisibleR + 2) * 2, (kVisibleR + 2) * 2);
}

QPainterPath PortItem::shape() const
{
    // ★ 命中区比画出来的圆大：10px 的小圆在缩放后只有几个屏幕像素，很难点中。
    //   shape() 是"能用鼠标碰到我"的范围，boundingRect() 是"我要占多大地方"，
    //   两者可以不一样 —— 这是 QGraphicsItem 里最容易搞混的一对。
    QPainterPath path;
    path.addEllipse(QPointF(0, 0), kHitR, kHitR);
    return path;
}

void PortItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *, QWidget *)
{
    painter->setRenderHint(QPainter::Antialiasing);

    const QColor c = (m_type == PortType::Number) ? FlowTheme::kPortNumber : FlowTheme::kPortAny;

    if (m_dir == Output) {
        // 输出：实心圆（"我这儿有数据"）
        painter->setPen(QPen(FlowTheme::kPortRing, 1.5));
        painter->setBrush(c);
    } else {
        // 输入：空心圆（"我要数据"）
        painter->setPen(QPen(c, 2.0));
        painter->setBrush(FlowTheme::kPortRing);
    }
    painter->drawEllipse(QPointF(0, 0), kVisibleR, kVisibleR);
}

QPointF PortItem::anchor() const
{
    // 端口是节点的子图元，pos() 是相对节点的。连线要的是场景坐标。
    return mapToScene(QPointF(0, 0));
}

bool PortItem::isFull() const
{
    return m_dir == Input && !m_edges.isEmpty();
}

void PortItem::attach(EdgeItem *edge)
{
    if (edge && !m_edges.contains(edge))
        m_edges.append(edge);
}

void PortItem::detach(EdgeItem *edge)
{
    m_edges.removeAll(edge);
}
