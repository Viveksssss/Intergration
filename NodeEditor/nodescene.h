#ifndef NODESCENE_H
#define NODESCENE_H

#include <QGraphicsScene>
#include "NodeItems/type.h"

class PortItem;
class EdgeItem;
class NodeItem;
class NodeScene : public QGraphicsScene
{
    Q_OBJECT
public:
    explicit NodeScene(QObject *parent = nullptr);
    NodeItem*addNode(Kind kind,const QPointF&pos);
    bool connectNodes(NodeItem *from, NodeItem *to);
    void refreshValues();
    void clearGraph();

    int nodeCount() const;
    int edgeCount() const;

    /*放弃手上的那根线*/
    void cancelDrag();
public slots:
    void deleteSelected();

signals:
    void graphChanged();

protected:
    void mousePressEvent(QGraphicsSceneMouseEvent *event) override;
    void mouseMoveEvent(QGraphicsSceneMouseEvent *event) override;
    void mouseReleaseEvent(QGraphicsSceneMouseEvent *event) override;

private:
    void connectPorts(PortItem*out,PortItem*in);
    bool canConnect(PortItem *a, PortItem *b) const;
    bool wouldCreateCycle(NodeItem *from, NodeItem *to) const;
    void removeEdge(EdgeItem *edge);
    PortItem *portAt(const QPointF &scenePos) const;

    PortItem*_dragPort = nullptr;//开始端口
    EdgeItem*_tempEdge = nullptr;//临时线
};

#endif // NODESCENE_H
