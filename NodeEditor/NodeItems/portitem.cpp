#include "portitem.h"
#include "../theme.h"
#include "../EdgeItems/edgeitem.h"
#include <QPainter>

namespace{

constexpr double kVisibleR = 5.0;   // 看得见的圆半径
constexpr double kHitR     = 15;   // 能点中的半径（比看见的大，好点）

}


PortItem::PortItem(NodeItem *owner, Direction dir, PortType type, const QString &label, int slot)
    :_owner(owner),_dir(dir),_type(type),_label(label),_slot(slot)
{
    setParentItem(owner);

    setAcceptedMouseButtons(Qt::NoButton);
    setFlag(ItemIsSelectable,false);
    setZValue(2);

}

QRectF PortItem::boundingRect() const
{
    return QRectF(-kHitR, -kHitR, kHitR * 2, kHitR * 2);
    // return QRectF(-kVisibleR - 2, -kVisibleR - 2,(kVisibleR + 2) * 2, (kVisibleR + 2) * 2);
}

void PortItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget)
{
    painter->setRenderHint(QPainter::Antialiasing);

    const QColor c = (_type == PortType::Number) ? Theme::kPortNumber : Theme::kPortAny;
    if (_dir == Direction::Output){
        painter->setPen(QPen(Theme::kPortRing,1.5));
        painter->setBrush(c);
    }else{
        painter->setPen(QPen(c,2.0));
        painter->setBrush(Theme::kPortRing);
    }
    painter->drawEllipse(QPointF(0,0),kVisibleR,kVisibleR);
}

int PortItem::type() const
{
    return Type;
}

QPainterPath PortItem::shape() const
{
    QPainterPath path;
    path.addEllipse(QPointF(0, 0), kHitR, kHitR);
    return path;
}

QPointF PortItem::anchors() const
{
    return mapToScene(QPointF(0, 0));
}

bool PortItem::attach(EdgeItem *edge,bool out)
{
    if (out){
        for(auto*p:_edges){
            if (p->fromPort() == edge->toPort()){
                return false;
            }
        }
    }else{
        for(auto*p:_edges){
            if (p->toPort() == edge->fromPort()){
                return false;
            }
        }
    }

    if (edge && !_edges.contains(edge))
        _edges.append(edge);
    return true;
}

void PortItem::detach(EdgeItem *edge)
{
    _edges.removeAll(edge);
}

bool PortItem::isFull() const
{
    return _dir == Direction::Input && !_edges.isEmpty();
}
