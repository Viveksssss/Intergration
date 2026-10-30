#pragma once

#include <QGraphicsItem>
#include <QList>
#include <QString>

class PortItem;

// ---------------------------------------------------------------------------
//  NodeItem —— 画布上的一个节点。
//
//  它自己负责三件事：
//    ① 画自己（圆角矩形 + 标题栏 + 端口标签 + 值）
//    ② 被拖动（ItemIsMovable 由 Qt 帮忙实现，我们只需在位置变化时通知连线）
//    ③ 算出自己的值（Number 固定、Add 求和、Display 透传）
//
//  端口是它的子图元，所以节点一动端口自动跟着动 —— 这是 QGraphicsItem
//  父子关系的核心便利，时间轴那套裸 QPainter 是没有的。
// ---------------------------------------------------------------------------
class NodeItem : public QGraphicsItem
{
public:
    enum Kind { Number, Add, Display };
    enum { Type = UserType + 2 };

    NodeItem(Kind kind, const QPointF &pos);

    int type() const override { return Type; }

    QRectF boundingRect() const override;
    QPainterPath shape() const override;
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option,
               QWidget *widget) override;

    Kind kind() const { return m_kind; }
    QString title() const { return m_title; }

    const QList<PortItem *> &inputs()  const { return m_inputs; }
    const QList<PortItem *> &outputs() const { return m_outputs; }

    double value() const { return m_value; }
    void   setFixedValue(double v);        // Number 节点专用

    // 递归算出自己的值（会向上游要值，所以不用管拓扑顺序）
    double evaluate();
    void   resetEval();                    // 每次图变了就清一遍求值标记
    void   refreshPortLabels();

protected:
    QVariant itemChange(GraphicsItemChange change, const QVariant &value) override;
    void mouseDoubleClickEvent(QGraphicsSceneMouseEvent *event) override;

private:
    void buildPorts();
    void notifyEdges();                    // 我动了 -> 让相连的线重算路径
    double bodyHeight() const;

    Kind    m_kind;
    QString m_title;
    QList<PortItem *> m_inputs;
    QList<PortItem *> m_outputs;

    double m_value      = 0.0;
    double m_fixedValue = 3.0;             // Number 节点里那个可以改的数
    int    m_evalMark   = 0;               // 0=没算 1=正在算 2=算完了
};
