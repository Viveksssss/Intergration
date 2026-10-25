#include "nodeview.h"
#include "NodeItems/nodeitem.h"
#include "nodescene.h"
#include "theme.h"
#include <QGraphicsScene>
#include <QGraphicsView>
#include <QMouseEvent>
#include <QOpenGLWidget>
#include <QPainter>
#include <QScrollBar>
#include <cmath>

namespace {

constexpr double BASESTEP = 24.0;   // 网格间距
constexpr double MINZOOM = 0.1;
constexpr double MAXZOOM = 10.0;
constexpr double ZOOMSTEP = 1.15;
}


NodeView::NodeView(QGraphicsScene *scene, QWidget *parent)
    : QGraphicsView(scene,parent)
{
    setViewport(new QOpenGLWidget);
    setViewportUpdateMode(QGraphicsView::FullViewportUpdate);

    setRenderHints(QPainter::Antialiasing | QPainter::TextAntialiasing);
    setDragMode(QGraphicsView::RubberBandDrag);

    setTransformationAnchor(QGraphicsView::AnchorViewCenter);
    setResizeAnchor(QGraphicsView::AnchorViewCenter);

    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    setSceneRect(-50000,-50000,100000,100000);
    setFocusPolicy(Qt::StrongFocus);
}

void NodeView::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::MiddleButton){
        _panning = true;
        _panStart = event->pos();
        setCursor(Qt::ClosedHandCursor);
        event->accept();
        return;
    }
    QGraphicsView::mousePressEvent(event);

    const auto items = scene()->selectedItems();
    for(auto*it:items){
        if (auto*node = qgraphicsitem_cast<NodeItem*>(it)){
            _selectedPos = node->scenePos();
            _hasSelection = true;
            _selectedNode = node;
            break;
        }
    }
}

void NodeView::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::MiddleButton && _panning){
        _panning = false;
        unsetCursor();
        event->accept();
        return;
    }
    QGraphicsView::mouseReleaseEvent(event);
}

void NodeView::mouseMoveEvent(QMouseEvent *event)
{
    if (_panning) {
        // 最省事的平移办法：把鼠标位移换算成滚动条位移。
        // 自己算 transform 也能做，但要处理缩放后的比例，反而容易出错。
        const QPoint d = event->pos() - _panStart;
        _panStart = event->pos();

        this->_position += d;

        horizontalScrollBar()->setValue(horizontalScrollBar()->value() - d.x());
        verticalScrollBar()->setValue(verticalScrollBar()->value() - d.y());
        event->accept();
        return;
    }
    if (_hasSelection && _selectedNode ){
        _selectedPos = _selectedNode->scenePos();
    }
    QGraphicsView::mouseMoveEvent(event);
}

void NodeView::drawBackground(QPainter *painter, const QRectF &rect)
{
    painter->fillRect(rect, Theme::kCanvasBg);   // 底色;

    double step = BASESTEP;
    while (step * _zoom < 12.0) step *= 5.0;
    while (step * _zoom > 120.0) step /= 5.0;
    const double major = step * 5.0;                 // 每 5 格一条粗线

    // 从 rect 的左边界开始，向下取整到步长的整数倍 —— 保证网格整体跟着画布走，
    // 而不是跟着视口飘（这就是平移时网格"不动"的关键）
    const double x0  = std::floor(rect.left()  / step)  * step;
    const double y0  = std::floor(rect.top()   / step)  * step;
    const double mx0 = std::floor(rect.left()  / major) * major;
    const double my0 = std::floor(rect.top()   / major) * major;

    QPen pen(Theme::kGridFine);
    pen.setCosmetic(true);          // ★ cosmetic = 线宽按屏幕像素算，不随缩放变粗

    QVarLengthArray<QLineF, 256> fine;
    for (double x = x0; x <= rect.right(); x += step)
        fine.append(QLineF(x, rect.top(), x, rect.bottom()));
    for (double y = y0; y <= rect.bottom(); y += step)
        fine.append(QLineF(rect.left(), y, rect.right(), y));

    painter->setPen(pen);
    painter->drawLines(fine.data(), int(fine.size()));

    pen.setColor(Theme::kGridCoarse);
    QVarLengthArray<QLineF, 128> coarse;
    for (double x = mx0; x <= rect.right(); x += major)
        coarse.append(QLineF(x, rect.top(), x, rect.bottom()));
    for (double y = my0; y <= rect.bottom(); y += major)
        coarse.append(QLineF(rect.left(), y, rect.right(), y));

    painter->setPen(pen);
    painter->drawLines(coarse.data(), int(coarse.size()));

    // 位置
    painter->save();
    painter->resetTransform();           // 回到视口像素坐标
    painter->setPen(QPen(QColor(0x11,0xff,0x11,0xaa), 10)); // 注意：10px 粗的笔，文字会糊
    painter->drawText(10,height() - 10,QString("x%1").arg(_zoom,3,10,1,'0'));
    painter->drawText(50, height() - 10, QString("场景: x:%1 y:%2").arg(_position.x(),3,10,'0').arg(_position.y(),3,10,'0'));
    painter->drawText(200,height() - 10, QString("节点: (%1,%2)").arg(_selectedPos.toPoint().x(),3,10,'0').arg(_selectedPos.toPoint().y(),3,10,'0'));
    painter->restore();
}

void NodeView::resetZoom()
{
    resetTransform();
    _zoom = 1.0;
    viewport()->update();
    emit zoomChanged(_zoom);
}

void NodeView::keyPressEvent(QKeyEvent *event)
{
    auto*scene = qobject_cast<NodeScene *>(this->scene());
    if (scene){
        if (event->key() == Qt::Key_Delete){
            scene->deleteSelected();
            _selectedNode = nullptr;//防止删除节点后，鼠标移动立马触发move事件，然后访问悬空指针
            return;
        }
        if (event->key() == Qt::Key_Escape){
            scene->cancelDrag();
            return;
        }
    }
    QGraphicsView::keyPressEvent(event);
}

void NodeView::wheelEvent(QWheelEvent *event)
{
    if (event->angleDelta().y() == 0) {
        event->ignore();
        return;
    }

    const double f = (event->angleDelta().y() > 0) ? ZOOMSTEP : 1.0 / ZOOMSTEP;
    const double next = _zoom * f;
    if (next < MINZOOM || next > MAXZOOM)
        return;
    _zoom = next;
    scale(f,f);
    viewport()->update();
    emit zoomChanged(_zoom);
}
