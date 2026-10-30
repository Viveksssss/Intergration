#pragma once

#include <QGraphicsScene>
#include <QList>
#include <QPointF>

#include "NodeItem.h"

class PortItem;
class EdgeItem;

// ---------------------------------------------------------------------------
//  NodeScene —— 图的所有权在这里，所有"图结构"的规则也在这里。
//
//  它跟时间轴那套最大的区别：这里没有 paintEvent。
//  场景只负责"有哪些东西、谁连着谁"，怎么画交给每个图元自己。
//
//  两个核心职责：
//    ① 连线规则：一进一出、类型匹配、不成环、输入端口单连接
//    ② 拖线状态机：在端口上按下 -> 拖一根临时线 -> 松开时决定成不成立
// ---------------------------------------------------------------------------
class NodeScene : public QGraphicsScene
{
    Q_OBJECT

public:
    explicit NodeScene(QObject *parent = nullptr);

    NodeItem *addNode(NodeItem::Kind kind, const QPointF &pos);
    void      clearGraph();

    // 便捷连接：自动挑 from 的第一个输出 port 和 to 的第一个空闲输入 port
    bool connectNodes(NodeItem *from, NodeItem *to);

    int nodeCount() const;
    int edgeCount() const;

    // 图变了以后重新算一遍所有节点的值
    void refreshValues();

    void cancelDrag();                 // 放弃手上那根线（按 Esc 或中途取消）

public slots:
    void deleteSelected();

signals:
    void graphChanged();

protected:
    void mousePressEvent(QGraphicsSceneMouseEvent *event) override;
    void mouseMoveEvent(QGraphicsSceneMouseEvent *event) override;
    void mouseReleaseEvent(QGraphicsSceneMouseEvent *event) override;

private:
    PortItem *portAt(const QPointF &scenePos) const;
    bool      canConnect(PortItem *a, PortItem *b) const;
    bool      wouldCreateCycle(NodeItem *from, NodeItem *to) const;
    void      connectPorts(PortItem *out, PortItem *in);
    void      removeEdge(EdgeItem *edge);

    PortItem *m_dragPort = nullptr;   // 从哪个端口开始拉的
    EdgeItem *m_tempEdge = nullptr;   // 手上那根还没落地的线
};
