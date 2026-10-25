#include "edgeitem.h"
#include "../NodeItems/portitem.h"
#include "../NodeItems/nodeitem.h"
#include "../theme.h"
#include <QPainter>

namespace {
constexpr double kMinCtrl = 60.0;
constexpr double kHitWidth = 14.0;    // 命中区宽度（沿曲线两侧各 7）
}

EdgeItem::EdgeItem(PortItem *from, PortItem *to)
    : _from(from),_to(to),_floating(false)
{
    setZValue(-1);
    setFlags(QGraphicsItem::ItemIsSelectable);
    setAcceptHoverEvents(true);
    _path = buildPath();
}

EdgeItem::EdgeItem(PortItem *anchorPort, const QPointF &mousePos)
    :_anchor(anchorPort),_freeEnd(mousePos),_floating(true)
{
    _anchorIsOutput = (anchorPort->direction() == Direction::Output);
    setZValue(100);
    setAcceptedMouseButtons(Qt::NoButton);
    _path = buildPath();
}

NodeItem *EdgeItem::fromNode() const
{
    return _from ? _from->owner() : nullptr;
}

NodeItem *EdgeItem::toNode() const
{
    return _to ? _to->owner() : nullptr;
}

QRectF EdgeItem::boundingRect() const
{
    return _path.boundingRect().adjusted(-10, -10, 10, 10);
}

void EdgeItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget)
{
    painter->setRenderHint(QPainter::Antialiasing);

    QColor c = Theme::kEdge;
    if (_floating)         c = Theme::kEdgeTemp;
    else if (isSelected())  c = Theme::kEdgeHot;
    else if (_hot)         c = Theme::kEdgeHot;

    const double w = (_hot || isSelected()) ? 5 : 2;
    painter->setPen(QPen(c, w, Qt::SolidLine, Qt::RoundCap));
    painter->setBrush(QColor(0x00,0x00,0xaa));
    painter->drawPath(_path);
}

QPainterPath EdgeItem::shape() const
{
    QPainterPathStroker stroker;
    stroker.setWidth(kHitWidth);
    stroker.setCapStyle(Qt::RoundCap);
    return stroker.createStroke(_path);
}

void EdgeItem::updatePath()
{
    prepareGeometryChange();
    _path = buildPath();
    update();
}

void EdgeItem::setFloatingEnd(const QPointF &p)
{
    prepareGeometryChange();
    _freeEnd = p;
    _path = buildPath();
    update();
}

void EdgeItem::finish(PortItem *target)
{
    prepareGeometryChange();
    if (_anchorIsOutput) {
        _from = _anchor;
        _to   = target;
    } else {
        _from = target;
        _to   = _anchor;
    }

    _floating = false;
    setZValue(-1);
    _path = buildPath();
    update();
}

void EdgeItem::setHot(bool hot)
{
    if (_hot == hot)return;
    _hot = hot;
    update();
}

QPainterPath EdgeItem::buildPath() const
{
    QPointF a ;
    QPointF b ;
    if (_floating){
        const auto p = _anchor->anchors();
        if (_anchorIsOutput) {
            a = p;
            b = _freeEnd;
        }else{
            a = _freeEnd;
            b = p;
        }
    }else{
        a = _from->anchors();
        b = _to->anchors();
    }

    const double dx = qMax(kMinCtrl,qAbs(b.x() - a.x())*0.2);

    QPainterPath path(a);
    path.cubicTo(a+QPointF(dx,0.0),b-QPointF(dx,0.0),b);

    return path;
}

void EdgeItem::hoverEnterEvent(QGraphicsSceneHoverEvent *event)
{
    setHot();
    QGraphicsItem::hoverEnterEvent(event);
}

void EdgeItem::hoverLeaveEvent(QGraphicsSceneHoverEvent *event)
{
    setHot(false);
    QGraphicsItem::hoverEnterEvent(event);
}
