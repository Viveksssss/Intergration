#ifndef NODEITEM_H
#define NODEITEM_H

#include <QGraphicsItem>
#include "type.h"

class PortItem;
class NodeItem : public QGraphicsItem
{
public:

    enum {Type = UserType + 1};

    NodeItem(Kind kind,const QPointF &pos);

    /*Must to inplement*/
    virtual int type()const override{return Type;};
    virtual QRectF boundingRect() const override;
    virtual void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override;
    virtual QPainterPath shape() const override;
    Kind kind()const{return _kind;};
    QString title()const{return _title;}
    const QList<PortItem *> &inputs()  const { return _inputs; }
    const QList<PortItem *> &outputs() const { return _outputs; }

    double evaluate();
    void resetEval();
    void setFixedValue(double v);

private:
    void buildPorts();
    void refreshPortLabels();
    double bodyHeight() const;
    void notifyEdges();

    Kind _kind;
    QString _title;
    QList<PortItem *> _inputs;
    QList<PortItem *> _outputs;


    double _value = 0.0;
    double _fixedValue = 0.0;
    int _evalMark = 0; // 0没算，1正在算，2算完了

protected:
    virtual QVariant itemChange(GraphicsItemChange change, const QVariant &value) override;
    virtual void mouseDoubleClickEvent(QGraphicsSceneMouseEvent *event) override;
};

#endif // NODEITEM_H
