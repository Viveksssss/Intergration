#pragma once

#include <QGraphicsItem>
#include <QList>
#include <QString>

class NodeItem;
class EdgeItem;

// 端口的数据类型。连线时检查两端类型，不匹配就不让连。
enum class PortType {
    Number,   // 数值
    Any       // 什么都收（Display 节点的输入）
};

// ---------------------------------------------------------------------------
//  PortItem —— 节点边上的那个小圆点。
//
//  它是 NodeItem 的子图元（parentItem() == 所属节点），所以：
//    · 节点移动  -> 端口自动跟着移动，不用写一行代码
//    · 端口坐标  -> 是相对节点的，想拿场景坐标得 mapToScene()
//
//  为什么端口要单独做成 item，而不是在节点里画个圆？
//  因为拖拽连线时需要用 items(scenePos) 查出"鼠标下面是哪个端口"，
//  而场景只会返回真正的图元。画出来的圆是查不到的。
// ---------------------------------------------------------------------------
class PortItem : public QGraphicsItem
{
public:
    enum Direction { Input, Output };
    enum { Type = UserType + 1 };          // 自定义类型号，qgraphicsitem_cast 靠它识别

    PortItem(NodeItem *owner, Direction dir, PortType type,
             const QString &label, int slot);

    int type() const override { return Type; }

    // QGraphicsItem 三个必写接口
    QRectF boundingRect() const override;
    QPainterPath shape() const override;
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option,
               QWidget *widget) override;

    // 连线的端点坐标（场景坐标）。圆心就是 item 原点，所以直接 mapToScene(0,0)
    QPointF anchor() const;

    Direction direction() const { return m_dir; }
    PortType  portType()  const { return m_type; }
    NodeItem *owner()     const { return m_owner; }
    QString   label()     const { return m_label; }
    int       slot()      const { return m_slot; }

    const QList<EdgeItem *> &edges() const { return m_edges; }

    // 输入端口只接一根线（新线会把旧线顶掉）；输出端口可以接很多根
    bool isFull() const;

    void attach(EdgeItem *edge);
    void detach(EdgeItem *edge);

private:
    NodeItem  *m_owner;
    Direction  m_dir;
    PortType   m_type;
    QString    m_label;
    int        m_slot;
    QList<EdgeItem *> m_edges;             // 挂在这个端口上的所有线
};
