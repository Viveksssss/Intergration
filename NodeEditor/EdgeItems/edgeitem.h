#ifndef EDGEITEM_H
#define EDGEITEM_H

#include <QGraphicsItem>

class PortItem;
class NodeItem;
class EdgeItem : public QGraphicsItem
{
public:
    enum {Type = UserType + 3};
    EdgeItem(PortItem *from, PortItem *to);
    EdgeItem(PortItem *anchorPort, const QPointF &mousePos);

    NodeItem *fromNode() const;
    NodeItem *toNode()   const;

    int type() const override { return Type; }
    QRectF boundingRect() const override;
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option,
               QWidget *widget) override;
    QPainterPath shape() const override;

    PortItem *fromPort() const { return _from; }
    PortItem *toPort()   const { return _to; }
    void updatePath();                 // 端点位置变了，重算曲线

    bool isFloating()const{return _floating;}
    void setFloatingEnd(const QPointF&p);
    void finish(PortItem*target);
    void setHot(bool hot = true);

private:
    QPainterPath buildPath() const;
    PortItem *_from = nullptr;        // 上游（Direction::Output）
    PortItem *_to   = nullptr;        // 下游（Direction::Input）
    PortItem *_anchor = nullptr;
    bool _anchorIsOutput = true;
    bool _floating = true;
    QPointF _freeEnd;
    bool _hot = false;

    QPainterPath _path;


    // QGraphicsItem interface
protected:
    virtual void hoverEnterEvent(QGraphicsSceneHoverEvent *event) override;
    virtual void hoverLeaveEvent(QGraphicsSceneHoverEvent *event) override;
};

#endif // EDGEITEM_H
