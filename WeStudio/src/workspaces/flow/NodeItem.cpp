#include "NodeItem.h"
#include "EdgeItem.h"
#include "NodeScene.h"
#include "PortItem.h"
#include "FlowTheme.h"

#include <QFont>
#include <QGraphicsSceneMouseEvent>
#include <QPainter>

namespace {
// 一个节点的全部尺寸都由这几个数推出来，改布局只改这里
constexpr double kNodeW  = 168.0;
constexpr double kTitleH = 28.0;
constexpr double kRowH   = 24.0;    // 每个端口占一行
constexpr double kFootH  = 26.0;    // 底部显示计算结果的区域
constexpr double kRadius = 7.0;
constexpr double kPad    = 8.0;     // boundingRect 外扩，容纳探出边缘的端口圆

// QGraphicsItem 不像 QWidget 那样自带 font 属性（只有 QGraphicsTextItem 有），
// 所以字体得自己造一个。放进 paint 里每次新建也没问题，QFont 很轻。
QFont nodeFont(double pointSize = 9.0)
{
    QFont f(QStringLiteral("Microsoft YaHei UI"));
    f.setPointSizeF(pointSize);
    return f;
}
}

NodeItem::NodeItem(Kind kind, const QPointF &pos)
    : m_kind(kind)
{
    setPos(pos);

    // ★ ItemSendsGeometryChanges 是最关键的一个标志：
    //   不开它，itemChange() 永远不会收到 ItemPositionHasChanged，
    //   于是拖动节点时线不跟着走 —— 而且没有任何报错，只能靠肉眼发现。
    setFlags(ItemIsMovable | ItemIsSelectable | ItemSendsGeometryChanges);

    switch (kind) {
    case Number:  m_title = QStringLiteral("数值");   break;
    case Add:     m_title = QStringLiteral("相加");   break;
    case Display: m_title = QStringLiteral("显示");   break;
    }

    buildPorts();
}

// ---------------------------------------------------------------- 尺寸
double NodeItem::bodyHeight() const
{
    const int rows = int(qMax(m_inputs.size(), m_outputs.size()));
    return kTitleH + qMax(1, rows) * kRowH + kFootH;
}

QRectF NodeItem::boundingRect() const
{
    return QRectF(-kPad, -kPad, kNodeW + kPad * 2, bodyHeight() + kPad * 2);
}

QPainterPath NodeItem::shape() const
{
    // 只有圆角矩形本体能被点中；探出去的端口由 PortItem 自己负责命中
    QPainterPath path;
    path.addRoundedRect(QRectF(0, 0, kNodeW, bodyHeight()), kRadius, kRadius);
    return path;
}

// ---------------------------------------------------------------- 端口
void NodeItem::buildPorts()
{
    auto addPort = [this](QList<PortItem *> &list, PortItem::Direction dir,
                          PortType type, const QString &label, int slot) {
        auto *port = new PortItem(this, dir, type, label, slot);
        list.append(port);
    };

    switch (m_kind) {
    case Number:
        addPort(m_outputs, PortItem::Output, PortType::Number,
                QStringLiteral("值"), 0);
        break;

    case Add:
        addPort(m_inputs, PortItem::Input, PortType::Number, QStringLiteral("A"), 0);
        addPort(m_inputs, PortItem::Input, PortType::Number, QStringLiteral("B"), 1);
        addPort(m_outputs, PortItem::Output, PortType::Number, QStringLiteral("和"), 0);
        break;

    case Display:
        // Display 的输入是 Any：什么类型都收，所以它能接任何节点
        addPort(m_inputs, PortItem::Input, PortType::Any, QStringLiteral("输入"), 0);
        break;
    }

    refreshPortLabels();
}

void NodeItem::refreshPortLabels()
{
    // 端口是子图元，pos() 相对节点。(0,y) 在左边缘，(kNodeW,y) 在右边缘
    for (int i = 0; i < m_inputs.size(); ++i)
        m_inputs[i]->setPos(0.0, kTitleH + kRowH * i + kRowH / 2.0);
    for (int i = 0; i < m_outputs.size(); ++i)
        m_outputs[i]->setPos(kNodeW, kTitleH + kRowH * i + kRowH / 2.0);
}

// ---------------------------------------------------------------- 绘制
void NodeItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *, QWidget *)
{
    painter->setRenderHint(QPainter::Antialiasing);

    const QRectF r(0, 0, kNodeW, bodyHeight());
    const bool sel = isSelected();

    // ① 阴影（往下偏 2px，让节点像浮在画布上）
    painter->setPen(Qt::NoPen);
    painter->setBrush(FlowTheme::kShadow);
    painter->drawRoundedRect(r.translated(0, 2), kRadius, kRadius);

    // ② 主体
    painter->setBrush(sel ? FlowTheme::kNodeBgSelected : FlowTheme::kNodeBg);
    painter->setPen(QPen(sel ? FlowTheme::kNodeBorderHot : FlowTheme::kNodeBorder,
                         sel ? 2.0 : 1.0));
    painter->drawRoundedRect(r, kRadius, kRadius);

    // ③ 标题栏：用 clip 把圆角矩形局限在上面那一条，
    //    比手工拼两个 QPainterPath 稳得多（拼的时候下半部分总会露圆角）
    painter->save();
    QPainterPath clip;
    clip.addRoundedRect(r, kRadius, kRadius);
    painter->setClipPath(clip);
    painter->fillRect(QRectF(r.left(), r.top(), r.width(), kTitleH), FlowTheme::kTitleBg);
    painter->restore();

    // ④ 标题
    QFont bold = nodeFont(9.0);
    bold.setBold(true);
    painter->setFont(bold);
    painter->setPen(FlowTheme::kTitleText);
    painter->drawText(QRectF(11, 0, r.width() - 22, kTitleH),
                      Qt::AlignVCenter | Qt::AlignLeft, m_title);

    // ⑤ 端口标签
    painter->setFont(nodeFont(8.0));
    painter->setPen(FlowTheme::kBodyText);
    for (int i = 0; i < m_inputs.size(); ++i) {
        const double cy = kTitleH + kRowH * i + kRowH / 2.0;
        painter->drawText(QRectF(12, cy - 9, r.width() - 24, 18),
                          Qt::AlignVCenter | Qt::AlignLeft, m_inputs[i]->label());
    }
    for (int i = 0; i < m_outputs.size(); ++i) {
        const double cy = kTitleH + kRowH * i + kRowH / 2.0;
        // 右边留出 32：给探出边缘的端口圆点腾位置，否则标签会贴着圆点
        painter->drawText(QRectF(12, cy - 9, r.width() - 44, 18),
                          Qt::AlignVCenter | Qt::AlignRight, m_outputs[i]->label());
    }

    // ⑥ 底部当前值
    QFont big = nodeFont(10.5);
    big.setBold(true);
    painter->setFont(big);
    painter->setPen(FlowTheme::kValueText);
    painter->drawText(QRectF(10, r.height() - kFootH, r.width() - 20, kFootH),
                      Qt::AlignVCenter | Qt::AlignRight,
                      QString::number(m_value, 'g', 6));
}

// ---------------------------------------------------------------- 移动
QVariant NodeItem::itemChange(GraphicsItemChange change, const QVariant &value)
{
    if (change == ItemPositionHasChanged) {
        notifyEdges();
    } else if (change == ItemSelectedHasChanged) {
        update();                        // 选中要换边框颜色，重画一下
    }
    return QGraphicsItem::itemChange(change, value);
}

void NodeItem::notifyEdges()
{
    // 我移动了 -> 挂在我所有端口上的线都得重新算一遍路径。
    // 要是漏了这一步，线会留在原地，看起来像"线跟节点脱钩了"。
    for (PortItem *p : m_inputs)
        for (EdgeItem *e : p->edges())
            e->updatePath();
    for (PortItem *p : m_outputs)
        for (EdgeItem *e : p->edges())
            e->updatePath();
}

// ---------------------------------------------------------------- 求值
void NodeItem::setFixedValue(double v)
{
    m_fixedValue = v;
    if (m_kind == Number) {
        m_value = v;
        update();
    }
}

double NodeItem::evaluate()
{
    if (m_evalMark == 2) return m_value;   // 这条链上游已经算过了
    if (m_evalMark == 1) return 0.0;       // ★ 又绕回自己 = 有环，直接掐断
    m_evalMark = 1;

    switch (m_kind) {
    case Number:
        m_value = m_fixedValue;
        break;

    case Add: {
        double sum = 0.0;
        for (PortItem *in : m_inputs) {
            if (in->edges().isEmpty()) continue;         // 没接线当 0
            sum += in->edges().first()->fromNode()->evaluate();
        }
        m_value = sum;
        break;
    }

    case Display:
        m_value = 0.0;
        if (!m_inputs.isEmpty() && !m_inputs.first()->edges().isEmpty())
            m_value = m_inputs.first()->edges().first()->fromNode()->evaluate();
        break;
    }

    m_evalMark = 2;
    return m_value;
}

void NodeItem::resetEval()
{
    m_evalMark = 0;
}

void NodeItem::mouseDoubleClickEvent(QGraphicsSceneMouseEvent *event)
{
    // 双击"数值"节点，值 +1（超过 9 回 0），能直观看到数据顺着线流下去
    if (m_kind == Number) {
        m_fixedValue = (m_fixedValue >= 9.0) ? 0.0 : m_fixedValue + 1.0;
        m_value = m_fixedValue;
        if (auto *s = qobject_cast<NodeScene *>(scene()))
            s->refreshValues();
        event->accept();
        return;
    }
    QGraphicsItem::mouseDoubleClickEvent(event);
}
