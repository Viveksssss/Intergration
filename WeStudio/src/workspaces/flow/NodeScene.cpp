#include "NodeScene.h"
#include "EdgeItem.h"
#include "PortItem.h"

#include <QGraphicsSceneMouseEvent>
#include <QSet>

NodeScene::NodeScene(QObject *parent)
    : QGraphicsScene(parent)
{
    // 场景本身不需要很大：真正限制可视范围的是 NodeView 里的 setSceneRect
    setItemIndexMethod(QGraphicsScene::NoIndex);   // 图元会频繁移动，BSP 索引反而更慢
}

// ---------------------------------------------------------------- 增删
NodeItem *NodeScene::addNode(NodeItem::Kind kind, const QPointF &pos)
{
    auto *node = new NodeItem(kind, pos);
    addItem(node);
    refreshValues();
    emit graphChanged();
    return node;
}

void NodeScene::clearGraph()
{
    cancelDrag();

    // 先收集再删除：删除过程中 items() 会变，边遍历边删会崩
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

bool NodeScene::connectNodes(NodeItem *from, NodeItem *to)
{
    if (!from || !to || from->outputs().isEmpty()) return false;

    PortItem *out = from->outputs().first();
    for (PortItem *in : to->inputs()) {
        if (!in->edges().isEmpty()) continue;      // 已经接了别的
        if (!canConnect(out, in)) continue;
        connectPorts(out, in);
        return true;
    }
    return false;
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

// ---------------------------------------------------------------- 鼠标
PortItem *NodeScene::portAt(const QPointF &scenePos) const
{
    // items(scenePos) 按 z 值从高到低返回所有"粗筛命中"的图元
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

void NodeScene::mousePressEvent(QGraphicsSceneMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        PortItem *port = portAt(event->scenePos());
        if (port) {
            // 输出端口随时可以拉；输入端口只有在空着的时候才能反向拉
            const bool canPull = (port->direction() == PortItem::Output)
                                 || port->edges().isEmpty();
            if (canPull) {
                m_dragPort = port;
                m_tempEdge = new EdgeItem(port, event->scenePos());
                addItem(m_tempEdge);
            }
            // ★ 无论能不能拉，都要把事件吞掉。
            //   端口把 setAcceptedMouseButtons 关掉了，不吞的话事件会穿到下面的
            //   节点上，变成"点端口时节点开始跟着鼠标跑"。
            event->accept();
            return;
        }
    }
    QGraphicsScene::mousePressEvent(event);
}

void NodeScene::mouseMoveEvent(QGraphicsSceneMouseEvent *event)
{
    if (m_tempEdge) {
        m_tempEdge->setFloatingEnd(event->scenePos());
        event->accept();
        return;
    }
    QGraphicsScene::mouseMoveEvent(event);
}

void NodeScene::mouseReleaseEvent(QGraphicsSceneMouseEvent *event)
{
    if (m_tempEdge && m_dragPort) {
        PortItem *target = portAt(event->scenePos());
        PortItem *out = nullptr;
        PortItem *in  = nullptr;

        if (target && canConnect(m_dragPort, target)) {
            if (m_dragPort->direction() == PortItem::Output) { out = m_dragPort; in = target; }
            else                                            { out = target;      in = m_dragPort; }
        }

        cancelDrag();                       // 临时线一律丢弃，成立的话在下面重建

        if (out && in)
            connectPorts(out, in);

        event->accept();
        return;
    }
    QGraphicsScene::mouseReleaseEvent(event);
}

void NodeScene::cancelDrag()
{
    if (m_tempEdge) {
        removeItem(m_tempEdge);
        delete m_tempEdge;
        m_tempEdge = nullptr;
    }
    m_dragPort = nullptr;
}

// ---------------------------------------------------------------- 规则
bool NodeScene::canConnect(PortItem *a, PortItem *b) const
{
    if (!a || !b || a == b) return false;
    if (a->direction() == b->direction()) return false;   // 必须一进一出
    if (a->owner() == b->owner()) return false;           // 不能自己连自己

    PortItem *out = (a->direction() == PortItem::Output) ? a : b;
    PortItem *in  = (a->direction() == PortItem::Input)  ? a : b;

    // 类型检查：Any 收一切；Number 只收 Number
    if (in->portType() != PortType::Any && in->portType() != out->portType())
        return false;

    // 成环检查：加完这条边以后，如果下游还能走回上游，就成环了
    if (wouldCreateCycle(out->owner(), in->owner()))
        return false;

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

void NodeScene::connectPorts(PortItem *out, PortItem *in)
{
    // ★ 输入端口单连接：先把它原来那根线拆掉，再连新的。
    //   输出端口不限，所以一个数值可以同时喂给很多节点。
    while (!in->edges().isEmpty())
        removeEdge(in->edges().first());

    auto *edge = new EdgeItem(out, in);
    addItem(edge);
    out->attach(edge);
    in->attach(edge);

    refreshValues();
    emit graphChanged();
}

void NodeScene::removeEdge(EdgeItem *edge)
{
    if (!edge) return;
    if (edge->fromPort()) edge->fromPort()->detach(edge);
    if (edge->toPort())   edge->toPort()->detach(edge);
    removeItem(edge);
    delete edge;
}

// ---------------------------------------------------------------- 删除
void NodeScene::deleteSelected()
{
    QSet<NodeItem *> nodes;
    QSet<EdgeItem *> edges;

    for (QGraphicsItem *it : selectedItems()) {
        if (auto *n = qgraphicsitem_cast<NodeItem *>(it))      nodes.insert(n);
        else if (auto *e = qgraphicsitem_cast<EdgeItem *>(it)) edges.insert(e);
    }
    if (nodes.isEmpty() && edges.isEmpty()) return;

    // ① 被删节点上挂着的线也得删，否则删完节点线还挂在半空
    for (NodeItem *n : nodes) {
        for (PortItem *p : n->inputs())
            for (EdgeItem *e : p->edges()) edges.insert(e);
        for (PortItem *p : n->outputs())
            for (EdgeItem *e : p->edges()) edges.insert(e);
    }

    // ② 先删线：线析构前必须从两端端口摘掉自己，
    //    否则端口里会留下指向已释放内存的指针（后面遍历就崩）
    for (EdgeItem *e : edges) removeEdge(e);

    // ③ 再删节点。端口是节点的子图元，会跟着节点一起销毁
    for (NodeItem *n : nodes) {
        removeItem(n);
        delete n;
    }

    refreshValues();
    emit graphChanged();
}

// ---------------------------------------------------------------- 求值
void NodeScene::refreshValues()
{
    QList<NodeItem *> all;
    for (QGraphicsItem *it : items())
        if (auto *n = qgraphicsitem_cast<NodeItem *>(it))
            all.append(n);

    // 先把所有节点的"算过了"标记清掉，再逐个求值。
    // 求值是递归的（问上游要值），所以不需要先做拓扑排序。
    for (NodeItem *n : all) n->resetEval();
    for (NodeItem *n : all) {
        n->evaluate();
        n->update();
    }
}
