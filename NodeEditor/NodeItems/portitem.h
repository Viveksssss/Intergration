#ifndef PORTITEM_H
#define PORTITEM_H

#include "nodeitem.h"

#include <QGraphicsItem>


class EdgeItem;
class NodeItem;
class PortItem : public QGraphicsItem
{
public:

    enum {Type = UserType+2};

    PortItem(NodeItem*owner,Direction dir,PortType type,const QString&label,int slot);

    // QGraphicsItem interface
public:
    virtual QRectF boundingRect() const override;
    virtual void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override;
    virtual int type() const override;
    virtual QPainterPath shape() const override;

    QPointF anchors()const;

    Direction direction()const{return _dir;}
    PortType portType()const  {return _type;}
    NodeItem*owner()const     {return _owner;}
    QString label()const      {return _label;}
    int slot()const           {return _slot;}

    const QList<EdgeItem*>&edges()const{return _edges;}
    bool attach(EdgeItem *edge,bool out = false);
    void detach(EdgeItem *edge);

    bool isFull()const;
private:
    NodeItem*_owner;
    Direction _dir;
    PortType _type;
    QString _label;
    int _slot;
    QList<EdgeItem*>_edges;
};

#endif // PORTITEM_H
