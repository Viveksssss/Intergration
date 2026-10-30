#include "NodeView.h"
#include "NodeScene.h"
#include "FlowTheme.h"

#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QScrollBar>
#include <QVarLengthArray>
#include <QWheelEvent>
#include <cmath>

namespace {
constexpr double kBaseStep = 24.0;    // 100% 缩放时网格的间距（场景单位）
constexpr double kMinZoom  = 0.25;
constexpr double kMaxZoom  = 3.0;
constexpr double kZoomStep = 1.15;    // 每滚一格缩放多少
}

NodeView::NodeView(NodeScene *scene, QWidget *parent)
    : QGraphicsView(scene, parent)
{
    setRenderHints(QPainter::Antialiasing | QPainter::TextAntialiasing);
    setDragMode(QGraphicsView::RubberBandDrag);          // 空白处拖 = 框选

    // ★ 缩放锚点钉在鼠标下那个点上。不设的话默认锚在视口中心，
    //   鼠标在左下角时内容会从指针底下溜走，手感立刻廉价。
    setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
    setResizeAnchor(QGraphicsView::AnchorViewCenter);

    // ★ 缩放时把整个视口标记为脏，而不是只重绘变化区域。
    //   网格背景是按"当前可见矩形"重画的，局部更新会留下旧密度的网格碎片。
    setViewportUpdateMode(QGraphicsView::FullViewportUpdate);

    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    // 所谓"无限画布"就是给一个够大的矩形。真正的无限没法做，
    // 但 ±50000 已经够任何人拖到手酸了。
    setSceneRect(-50000, -50000, 100000, 100000);

    setFocusPolicy(Qt::StrongFocus);   // 不设的话 Del / Esc 收不到
}

// ---------------------------------------------------------------- 背景
void NodeView::drawBackground(QPainter *painter, const QRectF &rect)
{
    painter->fillRect(rect, FlowTheme::kCanvasBg);

    // ★ 网格密度随缩放换挡。
    //   不换挡的话：缩小时屏幕上会挤进上千条线（卡），放大时又只剩几条（空）。
    //   思路和时间轴的刻度自适应一样 —— 先定"屏幕上一个格子至少多少像素"，
    //   再反算场景里的步长。
    double step = kBaseStep;
    while (step * m_zoom < 12.0) step *= 5.0;
    while (step * m_zoom > 120.0) step /= 5.0;
    const double major = step * 5.0;                 // 每 5 格一条粗线

    // 从 rect 的左边界开始，向下取整到步长的整数倍 —— 保证网格整体跟着画布走，
    // 而不是跟着视口飘（这就是平移时网格"不动"的关键）
    const double x0 = std::floor(rect.left()   / step) * step;
    const double y0 = std::floor(rect.top()    / step) * step;
    const double mx0 = std::floor(rect.left()  / major) * major;
    const double my0 = std::floor(rect.top()   / major) * major;

    QPen pen(FlowTheme::kGridFine);
    pen.setCosmetic(true);          // ★ cosmetic = 线宽按屏幕像素算，不随缩放变粗

    QVarLengthArray<QLineF, 256> fine;
    for (double x = x0; x <= rect.right(); x += step)
        fine.append(QLineF(x, rect.top(), x, rect.bottom()));
    for (double y = y0; y <= rect.bottom(); y += step)
        fine.append(QLineF(rect.left(), y, rect.right(), y));

    painter->setPen(pen);
    painter->drawLines(fine.data(), int(fine.size()));

    pen.setColor(FlowTheme::kGridCoarse);
    QVarLengthArray<QLineF, 128> coarse;
    for (double x = mx0; x <= rect.right(); x += major)
        coarse.append(QLineF(x, rect.top(), x, rect.bottom()));
    for (double y = my0; y <= rect.bottom(); y += major)
        coarse.append(QLineF(rect.left(), y, rect.right(), y));

    painter->setPen(pen);
    painter->drawLines(coarse.data(), int(coarse.size()));
}

// ---------------------------------------------------------------- 缩放
void NodeView::wheelEvent(QWheelEvent *event)
{
    if (event->angleDelta().y() == 0) {
        event->ignore();
        return;
    }

    const double f = (event->angleDelta().y() > 0) ? kZoomStep : 1.0 / kZoomStep;
    const double next = m_zoom * f;
    if (next < kMinZoom || next > kMaxZoom)
        return;                                   // 到极限了就不动，避免数值漂移

    m_zoom = next;
    scale(f, f);                                  // 锚点由 AnchorUnderMouse 负责
    viewport()->update();                         // 网格步长可能跨档了，重画背景
    emit zoomChanged(m_zoom);
}

void NodeView::resetZoom()
{
    resetTransform();
    m_zoom = 1.0;
    viewport()->update();
    emit zoomChanged(m_zoom);
}

// ---------------------------------------------------------------- 平移
void NodeView::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::MiddleButton) {
        m_panning = true;
        m_panStart = event->pos();
        setCursor(Qt::ClosedHandCursor);
        event->accept();
        return;
    }
    QGraphicsView::mousePressEvent(event);
}

void NodeView::mouseMoveEvent(QMouseEvent *event)
{
    if (m_panning) {
        // 最省事的平移办法：把鼠标位移换算成滚动条位移。
        // 自己算 transform 也能做，但要处理缩放后的比例，反而容易出错。
        const QPoint d = event->pos() - m_panStart;
        m_panStart = event->pos();
        horizontalScrollBar()->setValue(horizontalScrollBar()->value() - d.x());
        verticalScrollBar()->setValue(verticalScrollBar()->value() - d.y());
        event->accept();
        return;
    }
    QGraphicsView::mouseMoveEvent(event);
}

void NodeView::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::MiddleButton && m_panning) {
        m_panning = false;
        unsetCursor();
        event->accept();
        return;
    }
    QGraphicsView::mouseReleaseEvent(event);
}

// ---------------------------------------------------------------- 键盘
void NodeView::keyPressEvent(QKeyEvent *event)
{
    auto *s = qobject_cast<NodeScene *>(scene());
    if (s) {
        if (event->key() == Qt::Key_Delete) {
            s->deleteSelected();
            return;
        }
        if (event->key() == Qt::Key_Escape) {
            s->cancelDrag();
            return;
        }
    }
    QGraphicsView::keyPressEvent(event);
}
