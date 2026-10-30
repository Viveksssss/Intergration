#pragma once

#include <QGraphicsItem>
#include <QPainterPath>
#include <QPointF>

class PortItem;
class NodeItem;

// ---------------------------------------------------------------------------
//  EdgeItem —— 连接两个端口的贝塞尔曲线。
//
//  它有两种存在形态：
//    · 已连接：两端都指向真实端口，节点一动线就跟着重算
//    · 拖拽中：只有一端有端口，另一端跟着鼠标跑（"皮筋"效果）
//
//  ★ 最容易被忽略的一点：连线是"零宽度的曲线"，
//    直接用 boundingRect() 做命中检测的话，点曲线旁边的空白也算点中。
//    所以必须重写 shape()，返回一条"加粗的笔画区域"。
// ---------------------------------------------------------------------------
class EdgeItem : public QGraphicsItem
{
public:
    enum { Type = UserType + 3 };

    // 已连接：上游端口 -> 下游端口
    EdgeItem(PortItem *from, PortItem *to);

    // 拖拽中：一端固定，另一端跟着鼠标（anchorPort 可能是输入端口，
    // 这时线要往反方向画 —— 所以要多存一个"哪端是固定端"）
    EdgeItem(PortItem *anchorPort, const QPointF &mousePos);

    int type() const override { return Type; }

    QRectF boundingRect() const override;
    QPainterPath shape() const override;
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option,
               QWidget *widget) override;

    PortItem *fromPort() const { return m_from; }
    PortItem *toPort()   const { return m_to; }
    NodeItem *fromNode() const;
    NodeItem *toNode()   const;

    bool isFloating() const { return m_floating; }

    void updatePath();                    // 端点位置变了，重算曲线
    void setFloatingEnd(const QPointF &p);// 拖拽时更新滑动端
    void finish(PortItem *target);        // 拖拽结束，固化成一条真实的线
    void setHot(bool hot);

protected:
    void hoverEnterEvent(QGraphicsSceneHoverEvent *event) override;
    void hoverLeaveEvent(QGraphicsSceneHoverEvent *event) override;

private:
    QPainterPath buildPath() const;

    PortItem *m_from = nullptr;           // 上游（Direction::Output）
    PortItem *m_to   = nullptr;           // 下游（Direction::Input）
    PortItem *m_anchor = nullptr;         // 拖拽时固定的那一端
    bool      m_anchorIsOutput = true;

    bool      m_floating = true;
    QPointF   m_freeEnd;                  // 拖拽时的鼠标位置
    bool      m_hot = false;

    QPainterPath m_path;
};
