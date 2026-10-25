#include "nodescene.h"
#include "NodeItems/nodeitem.h"
#include "NodeItems/portitem.h"
#include "EdgeItems/edgeitem.h"

#include <QGraphicsItem>
#include <QGraphicsSceneMouseEvent>

NodeScene::NodeScene(QObject *parent)
    : QGraphicsScene{parent}
{
    setItemIndexMethod(QGraphicsScene::NoIndex); //图元会频繁移动，BSP 索引反而更慢
}

NodeItem *NodeScene::addNode(Kind kind, const QPointF &pos)
{
    auto *node = new NodeItem(kind, pos);
    addItem(node);
    // refreshValues();
    emit graphChanged();
    return node;
}

bool NodeScene::connectNodes(NodeItem *from, NodeItem *to)
{
    if (!from || !to || from->outputs().isEmpty()) return false;

    PortItem *out = from->outputs().first();
    for (PortItem *in : to->inputs()) {
        if (in->isFull()) continue;      // 这个输入已经被占了
        connectPorts(out, in);
        return true;
    }
    return false;
}

void NodeScene::refreshValues()
{
    QList<NodeItem*>all;
    for(QGraphicsItem*it : items()){
        if (auto*n = qgraphicsitem_cast<NodeItem*>(it)){
            all.append(n);
        }
    }
    for(NodeItem*n:all){
        n->resetEval();
    }
    for(NodeItem*n:all){
        n->evaluate();
        n->update();
    }
}

void NodeScene::clearGraph()
{
    cancelDrag();
    QList<EdgeItem *> edges;
    QList<NodeItem *> nodes;
    for (QGraphicsItem *it : items()) {
        if (auto *e = qgraphicsitem_cast<EdgeItem *>(it))      edges.append(e);
        else if (auto *n = qgraphicsitem_cast<NodeItem *>(it)) nodes.append(n);
    }

    for (EdgeItem *e : edges) removeEdge(e);
    for (NodeItem *n : nodes) { removeItem(n); delete n; }

    emit graphChanged();
}

int NodeScene::nodeCount() const
{
    int n = 0;
    for (QGraphicsItem *it : items())
        if (it->type() == NodeItem::Type) ++n;
    return n;
}

int NodeScene::edgeCount() const
{
    int n = 0;
    for (QGraphicsItem *it : items()) {
        if (it->type() != EdgeItem::Type) continue;
        if (auto *e = static_cast<EdgeItem *>(it))
            if (!e->isFloating()) ++n;           // 拖拽中的临时线不算
    }
    return n;
}

void NodeScene::cancelDrag()
{
    if (_tempEdge){
        removeItem(_tempEdge);
        delete _tempEdge;
        _tempEdge = nullptr;
    }
    _dragPort = nullptr;
}

void NodeScene::deleteSelected()
{
    QSet<NodeItem *> nodes;
    QSet<EdgeItem *> edges;
    for (QGraphicsItem *it : selectedItems()) {
        if (auto *n = qgraphicsitem_cast<NodeItem *>(it))      nodes.insert(n);
        else if (auto *e = qgraphicsitem_cast<EdgeItem *>(it)) edges.insert(e);
    }
    if (nodes.isEmpty() && edges.isEmpty()) return;
    for (NodeItem *n : nodes) {
        for (PortItem *p : n->inputs())
            for (EdgeItem *e : p->edges()) edges.insert(e);
        for (PortItem *p : n->outputs())
            for (EdgeItem *e : p->edges()) edges.insert(e);
    }

    for (EdgeItem *e : edges) removeEdge(e);
    for (NodeItem *n : nodes) {
        removeItem(n);
        delete n;
    }
}

void NodeScene::mousePressEvent(QGraphicsSceneMouseEvent *event)
{
    if (event->button() == Qt::LeftButton){
        PortItem*port = portAt(event->scenePos());
        if (port){
            const bool canPull = (port->direction() == Direction::Output ) || !port->isFull();
            if (canPull){
                _dragPort = port;
                _tempEdge = new EdgeItem(port,event->scenePos());

                addItem(_tempEdge);
            }

            event->accept();
            return;
        }
    }
    QGraphicsScene::mousePressEvent(event);


}

void NodeScene::mouseMoveEvent(QGraphicsSceneMouseEvent *event)
{
    if (_tempEdge){
        _tempEdge->setFloatingEnd(event->scenePos());
        event->accept();
        return;
    }
    QGraphicsScene::mouseMoveEvent(event);
}

void NodeScene::mouseReleaseEvent(QGraphicsSceneMouseEvent *event)
{
    if (_tempEdge && _dragPort){
        PortItem*target = portAt(event->scenePos());
        PortItem*out = nullptr;
        PortItem*in = nullptr;
        if (target && canConnect(_dragPort,target)){
            if (_dragPort->direction() == Direction::Output){
                out = _dragPort;
                in = target;
            }else{
                out = target;
                in = _dragPort;
            }
        }
        cancelDrag();

        if (out&&in){
            connectPorts(out,in);
        }
        event->accept();
        return;
    }
    QGraphicsScene::mouseReleaseEvent(event);
}

void NodeScene::connectPorts(PortItem *out, PortItem *in)
{
    while (!in->edges().isEmpty())
        removeEdge(in->edges().first());
    auto *edge = new EdgeItem(out, in);

    if (!out->attach(edge,false) || ! in->attach(edge,true) ){
        cancelDrag();
        delete edge;
    }else{
        addItem(edge);
        // out->owner()->evaluate();
        refreshValues();
        emit graphChanged();
    }

}

bool NodeScene::canConnect(PortItem *a, PortItem *b) const
{
    if (!a || !b || a == b)return false;
    if (a->direction() == b->direction())return false;
    if (a->owner() == b->owner())return false;
    PortItem*out = a->direction() == Direction::Output ? a : b;
    PortItem*in = a->direction() == Direction::Input ? a : b;
    if (in->portType() != PortType::Any && in->portType() !=out->portType())return false;
    return true;
}

bool NodeScene::wouldCreateCycle(NodeItem *from, NodeItem *to) const
{
    // 要加的边是 from -> to。沿已有连线从 to 往下游走，
    // 只要能走到 from，这条边就会闭合成环。
    QSet<NodeItem *> seen;
    QList<NodeItem *> stack;
    stack.append(to);
    while (!stack.isEmpty()) {
        NodeItem *n = stack.takeLast();
        if (n == from) return true;
        if (seen.contains(n)) continue;
        seen.insert(n);

        for (PortItem *p : n->outputs())
            for (EdgeItem *e : p->edges())
                stack.append(e->toNode());
    }
    return false;
}

void NodeScene::removeEdge(EdgeItem *edge)
{
    if (!edge)return;
    if (edge->fromPort())edge->fromPort()->detach(edge);
    if (edge->toPort())edge->toPort()->detach(edge);
    removeItem(edge);
    refreshValues();
    delete edge;
}

PortItem *NodeScene::portAt(const QPointF &scenePos) const
{
    for (QGraphicsItem *it : items(scenePos)) {
        auto *port = qgraphicsitem_cast<PortItem *>(it);
        if (!port) continue;

        // items() 用的是 boundingRect 粗筛，这里再用 shape() 精筛一次：
        // 端口的命中区是个半径 9 的圆，圆外接矩形四角的位置不该算点中
        if (port->shape().contains(port->mapFromScene(scenePos)))
            return port;
    }
    return nullptr;
}
