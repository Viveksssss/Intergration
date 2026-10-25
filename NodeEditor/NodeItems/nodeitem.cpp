#include "nodeitem.h"
#include "portitem.h"
#include "../theme.h"
#include "../EdgeItems/edgeitem.h"
#include "../nodescene.h"
#include <QFont>
#include <QGraphicsSceneMouseEvent>
#include <QPainter>
#include <QPainterPath>

namespace{


constexpr double kNodeW  = 168.0;
constexpr double kNodeH  = 78.0;    // 阶段 3 固定高度，阶段 4 会按端口数算
constexpr double kTitleH = 28.0;
constexpr double kRowH   = 24.0;    // 每个端口占一行
constexpr double kFootH  = 12.0;    // 底部留白（阶段 10 会加高，用来显示计算结果）


constexpr double kRadius = 7.0;
constexpr double kPad    = 8.0;     // boundingRect 外扩，容纳探出边缘的端口圆

QFont nodeFont(double pointSize = 9.0)
{
    QFont f(QStringLiteral("Microsoft YaHei UI"));
    f.setPointSizeF(pointSize);
    return f;
}

}

NodeItem::NodeItem(Kind kind, const QPointF &pos)
    : _kind(kind)
{
    setPos(pos);
    setFlags(ItemIsMovable | ItemIsSelectable | ItemSendsGeometryChanges);
    switch(static_cast<int>(kind)){
    case Number:{
        _title = QStringLiteral("数值");
        break;
    }
    case Add:{
        _title = QStringLiteral("数值");
        break;
    }
    case Display:{
        _title = QStringLiteral("数值");
        break;
    }
    }

    buildPorts();
}

QRectF NodeItem::boundingRect() const
{
    return QRectF(-kPad, -kPad, kNodeW + kPad * 2, bodyHeight() + kPad * 2);
}

QPainterPath NodeItem::shape() const
{
    QPainterPath path;
    path.addRoundedRect(QRectF(0,0,kNodeW,bodyHeight()),kRadius,kRadius);
    return path;
}

double NodeItem::evaluate()
{
    if (_evalMark == 2)return _value;
    if (_evalMark == 1)return 0.0;
    _evalMark = 1;
    switch(static_cast<int>(_kind)){
    case Kind::Number:{
        _value = _fixedValue;
        break;
    }
    case Kind::Add:{
        double sum = 0.0;
        for(PortItem*in : _inputs){
            if (in->edges().empty())continue;
            sum+=in->edges().first()->fromNode()->evaluate();
        }
        _value = sum;
        break;
    }
    case Kind::Display:{
        _value = 0.0;
        if (!_inputs.empty() && !_inputs.first()->edges().empty()){
            _value = _inputs.first()->edges().first()->fromNode() -> evaluate();
        }
        break;
    }
    }
    _evalMark = 2;
    return _value;
}

void NodeItem::resetEval()
{
    _evalMark = 0;
}

void NodeItem::setFixedValue(double v)
{
    _fixedValue = v;
    if (_kind == Kind::Number){
        _value = v;
        update();
    }
}

void NodeItem::buildPorts()
{
    auto addPort = [this](QList<PortItem *> &list, Direction dir,
                          PortType type, const QString &label, int slot) {
        list.append(new PortItem(this, dir, type, label, slot));
    };


    switch(static_cast<int>(_kind)){
        case static_cast<int>(Kind::Number):{
            addPort(_outputs, Direction::Output, PortType::Number,
                    QStringLiteral("值"), 0);
            break;
        }
        case static_cast<int>(Kind::Add):{
            addPort(_inputs,  Direction::Input,  PortType::Number, QStringLiteral("A"), 0);
            addPort(_inputs,  Direction::Input,  PortType::Number, QStringLiteral("B"), 1);
            addPort(_outputs, Direction::Output, PortType::Number, QStringLiteral("和"), 0);
            break;
        }
        case static_cast<int>(Kind::Display):{
            addPort(_inputs, Direction::Input, PortType::Any, QStringLiteral("输入"), 0);
            break;
        }
    }
    refreshPortLabels();
}

void NodeItem::refreshPortLabels()
{
    for (int i = 0; i < _inputs.size(); ++i)
        _inputs[i]->setPos(0.0, kTitleH + kRowH * i + kRowH / 2.0);
    for (int i = 0; i < _outputs.size(); ++i)
        _outputs[i]->setPos(kNodeW, kTitleH + kRowH * i + kRowH / 2.0);
}

double NodeItem::bodyHeight() const
{
    const int rows = int(qMax(_inputs.size(), _outputs.size()));
    return kTitleH + qMax(1, rows) * kRowH + kFootH;
}

void NodeItem::notifyEdges()
{
    for(PortItem*p:_inputs){
        for(EdgeItem*e:p->edges()){
            e->updatePath();
        }
    }
    for(PortItem*p:_outputs){
        for(EdgeItem*e:p->edges()){
            e->updatePath();
        }
    }
}

QVariant NodeItem::itemChange(GraphicsItemChange change, const QVariant &value)
{
    if (change == ItemPositionHasChanged) {
        notifyEdges();
    } else if (change == ItemSelectedHasChanged) {
        update();                        // 选中要换边框颜色，重画一下
    }
    return QGraphicsItem::itemChange(change,value);
}

void NodeItem::mouseDoubleClickEvent(QGraphicsSceneMouseEvent *event)
{
    if (_kind == Number) {
        _fixedValue = (_fixedValue >= 100.0) ? 0.0 : _fixedValue + 1.0;
        _value = _fixedValue;

        if (auto *s = qobject_cast<NodeScene *>(scene()))
            s->refreshValues();
        event->accept();
        return;
    }
    QGraphicsItem::mouseDoubleClickEvent(event);
}

void NodeItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget)
{
    painter->setRenderHint(QPainter::Antialiasing);

    const QRectF r(0,0,kNodeW,kNodeH);
    const bool isSel = isSelected();

    // // 阴影，下偏2px
    // painter->setPen(Qt::NoPen);
    // painter->setBrush(Theme::kShadow);
    // painter->drawRoundedRect(r.translated(0,2),kRadius,kRadius);

    //主体
    painter->setBrush(isSel?Theme::kNodeBgSelected:Theme::kNodeBg);
    painter->setPen(Qt::NoPen);
    painter->drawRoundedRect(r,kRadius,kRadius);

    // 标题栏
    painter->save();
    QPainterPath clip;
    clip.addRoundedRect(r,kRadius,kRadius);
    painter->setClipPath(clip);
    painter->fillRect(QRectF(r.left(),r.top(),r.width(),kTitleH),Theme::kTitleBg);
    painter->restore();

    // 标题
    QFont bold = nodeFont(9.0);
    bold.setBold(true);
    painter->setFont(bold);
    painter->setPen(Theme::kTitleText);
    painter->drawText(QRectF(11,0,r.width()-22,kTitleH),Qt::AlignLeft | Qt::AlignVCenter,_title);


    // 选择框 ： 最后绘制
    painter->setPen(QPen(isSel? Theme::kNodeBorderHot : Theme::kNodeBorder ,isSel?2.0:1.0));
    painter->setBrush(Qt::NoBrush);
    painter->drawRoundedRect(r,kRadius,kRadius);

    // 端口标签
    painter->setFont(nodeFont(8.0));
    painter->setPen(Theme::kBodyText);
    for (int i = 0; i < _inputs.size(); ++i) {
        const double cy = kTitleH + kRowH * i + kRowH / 2.0;
        painter->drawText(QRectF(20, cy - 9, r.width() - 24, 18),
                          Qt::AlignVCenter | Qt::AlignLeft, _inputs[i]->label());
    }
    for (int i = 0; i < _outputs.size(); ++i) {
        const double cy = kTitleH + kRowH * i + kRowH / 2.0;
        // 右边留出 32：给探出边缘的端口圆点腾位置，否则标签会贴着圆点
        painter->drawText(QRectF(-40, cy - 9, r.width() - 4, 18),
                          Qt::AlignVCenter | Qt::AlignRight, _outputs[i]->label());
    }

    // 当前值
    QFont big = nodeFont(10.5);
    big.setBold(true);
    painter->setFont(big);
    painter->setPen(Theme::kValueText);
    painter->drawText(QRectF(r.right() - 30,r.height()/2 - 7,r.width() - 20,kFootH+5),Qt::AlignLeft|Qt::AlignVCenter,
                      QString::number(_value,'g',6));

}










